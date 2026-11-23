/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/protocols/protocol_bitcoind_rest.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_rest
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace std::placeholders;
using namespace system::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// bitcoind serializes a block filter as its type, block hash and filter.
template <typename Writer>
static void write_filter(Writer& writer, const hash_digest& hash,
    const data_chunk& filter) NOEXCEPT
{
    writer.write_byte(zero);
    writer.write_bytes(hash);
    writer.write_variable(filter.size());
    writer.write_bytes(filter);
}

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    allow("GET");
    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_hash, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_txs, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_block_part, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_BITCOIND(handle_get_block_spent_tx_outputs, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_filter, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_block_filter_headers, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_BITCOIND(handle_get_chain_information, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_utxos, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_mempool, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mempool_information, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_deployment_info, _1, _2, _3);
    SUBSCRIBE_CHANNEL(get, handle_receive_get, _1, _2);
    network::protocol::start();
}

void protocol_bitcoind_rest::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rest_dispatcher_.stop(ec);
    protocol_bitcoind::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

// The terminal responder handles options and post, this protocol claims get.
void protocol_bitcoind_rest::handle_receive_get(const code& ec,
    const get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*get, get->version()))
    {
        send_bad_host(*get);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*get, get->version()))
    {
        send_forbidden(*get);
        return;
    }

    // Get is saved off during asynchronous handling and used by senders to
    // formulate response headers, isolating handlers from http.
    set_request(get);

    // Parse REST url into json-rpc model and dispatch to handler.
    // Malformed parameters are bad requests, unknown targets are not found.
    request_t model{};
    const auto target = get->target();
    if (const auto fault = bitcoind_target(model, target))
    {
        if ((fault == error::invalid_hash) ||
            (fault == error::invalid_number) ||
            (fault == error::missing_hash) ||
            (fault == error::missing_height) ||
            (fault == error::missing_target))
            send_bad_request();
        else
            send_not_found();

        return;
    }

    // Overlay query string parameters onto the parsed model.
    if (!bitcoind_query(model, target))
    {
        send_bad_request();
        return;
    }

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(model.method))
    {
        send_forbidden();
        return;
    }

    // Required parameters may be query string sourced (e.g. blockpart).
    if (const auto fault = rest_dispatcher_.notify(model))
    {
        if (fault == network::error::missing_parameter)
            send_bad_request();
        else
            send_not_found();
    }
}

// Media types.
// ----------------------------------------------------------------------------

constexpr auto data = to_value(http::media_type::application_octet_stream);
constexpr auto json = to_value(http::media_type::application_json);
constexpr auto text = to_value(http::media_type::text_plain);

// Handlers.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::handle_get_block(const code& ec,
    rest_interface::block, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!hash)
    {
        send_not_found();
        return;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(*hash);
    const auto turbo = database_settings().turbo;
    size_t size{};
    if (query.is_pruned(link) || !query.get_block_size(size, link, witness))
    {
        send_not_found();
        return;
    }

    switch (media)
    {
        case data:
        {
            send_stream(size, [&query, link, turbo](
                const data_slab& out) NOEXCEPT
            {
                return query.get_wire_block(out, link, witness, turbo);
            }, http::media_type::application_octet_stream);
            return;
        }
        case text:
        {
            send_stream(add1(two * size), [&query, link, turbo](
                const data_slab& out) NOEXCEPT
            {
                const auto last = std::prev(out.end());
                *last = '\n';
                return query.get_wire_block<write::base16::fast>(
                    { out.begin(), last }, link, witness, turbo);
            }, http::media_type::text_plain);
            return;
        }
        case json:
        {
            const auto block = query.get_block(link, witness);
            if (!block)
            {
                send_not_found();
                return;
            }

            send_json(value_from(bitcoind_verbose(*block)));
            return;
        }
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_tx(const code& ec,
    rest_interface::tx, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!hash)
    {
        send_not_found();
        return;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_tx(*hash);
    const auto tx = query.is_pruned(link) ? nullptr :
        query.get_transaction(link, witness);

    if (!tx)
    {
        send_not_found();
        return;
    }

    const auto size = tx->serialized_size(witness);
    switch (media)
    {
        case data:
            send_chunk(to_data(*tx, size, witness));
            return;
        case text:
            send_text(to_text(*tx, size, witness));
            return;
        case json:
        {
            auto model = value_from(bitcoind(*tx, flags_));
            inject_tx_context(model.as_object(), query, link);
            inject_tx_scripts(model.as_object(), *tx, p2kh_, p2sh_, witness_);
            send_json(std::move(model));
            return;
        }
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_block_hash(const code& ec,
    rest_interface::block_hash, uint8_t media, uint32_t height) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = query.to_confirmed(height);
    if (link.is_terminal())
    {
        send_not_found();
        return;
    }

    const auto hash = query.get_header_key(link);
    switch (media)
    {
        case data:
            send_chunk(to_chunk(hash));
            return;
        case text:
            send_text(encode_hash(hash));
            return;
        case json:
            send_json(object{ { "blockhash", encode_hash(hash) } });
            return;
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_block_txs(const code& ec,
    rest_interface::block_txs, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!hash)
    {
        send_not_found();
        return;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(*hash);
    const auto block = query.is_pruned(link) ? nullptr :
        query.get_block(link, witness);

    if (!block)
    {
        send_not_found();
        return;
    }

    const auto size = block->serialized_size(witness);
    switch (media)
    {
        case data:
            send_chunk(to_data(*block, size, witness));
            return;
        case text:
            send_text(to_text(*block, size, witness));
            return;
        case json:
            send_json(value_from(bitcoind_hashed(*block)));
            return;
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_block_headers(const code& ec,
    rest_interface::block_headers, uint8_t media, const hash_cptr& hash,
    uint32_t count) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!hash || is_zero(count))
    {
        send_not_found();
        return;
    }

    const auto& query = archive();
    const auto header_link = query.to_header(*hash);
    if (!query.is_confirmed_block(header_link))
    {
        send_not_found();
        return;
    }

    size_t height{};
    if (!query.get_height(height, header_link))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    constexpr auto header_size = chain::header::serialized_size();
    const auto limit = lesser(count, messages::peer::max_get_headers);
    const auto links = query.get_confirmed_headers(height, limit);
    if (links.empty())
    {
        send_not_found();
        return;
    }

    switch (media)
    {
        case data:
        {
            data_chunk out(links.size() * header_size);
            stream::out::fast sink{ out };
            write::bytes::fast writer{ sink };
            for (const auto& link: links)
            {
                if (!query.get_wire_header(writer, link))
                {
                    send_internal_server_error(database::error::integrity);
                    return;
                }
            }

            send_chunk(std::move(out));
            return;
        }
        case text:
        {
            std::string out{};
            out.reserve(add1(links.size() * two * header_size));
            out.resize(links.size() * two * header_size);
            stream::out::fast sink{ out };
            write::base16::fast writer{ sink };
            for (const auto& link: links)
            {
                if (!query.get_wire_header(writer, link))
                {
                    send_internal_server_error(database::error::integrity);
                    return;
                }
            }

            send_text(std::move(out));
            return;
        }
        case json:
        {
            array out{};
            out.reserve(links.size());
            for (const auto& link: links)
            {
                const auto header = query.get_header(link);
                if (!header)
                {
                    send_internal_server_error(database::error::integrity);
                    return;
                }

                out.push_back(value_from(bitcoind(*header)));
            }

            send_json(std::move(out));
            return;
        }
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_block_part(const code& ec,
    rest_interface::block_part, uint8_t media, const hash_cptr& hash,
    uint32_t offset, uint32_t size) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(interface::bitcoind::version::v31_0) || !hash)
    {
        send_not_found();
        return;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(*hash);
    const auto block = query.is_pruned(link) ? nullptr :
        query.get_block(link, witness);

    if (!block)
    {
        send_not_found();
        return;
    }

    // bitcoind reports an out of range part as a bad request.
    const auto full = to_data(*block, block->serialized_size(witness), witness);
    if (ceilinged_add<size_t>(offset, size) > full.size())
    {
        send_bad_request();
        return;
    }

    const auto begin = std::next(full.begin(), offset);
    data_chunk part{ begin, std::next(begin, size) };
    switch (media)
    {
        case data:
            send_chunk(std::move(part));
            return;
        case text:
            send_text(encode_base16(part));
            return;
    }

    // block_part is bin|hex only (json is a bad request, as bitcoind).
    send_bad_request();
}

void protocol_bitcoind_rest::handle_get_block_spent_tx_outputs(const code& ec,
    rest_interface::block_spent_tx_outputs, uint8_t media,
    const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!hash)
    {
        send_not_found();
        return;
    }

    // bitcoind serves spent outputs from undo data (confirmed blocks only).
    const auto& query = archive();
    const auto link = query.to_header(*hash);
    if (!query.is_confirmed_block(link))
    {
        send_not_found();
        return;
    }

    // Prevouts grouped per spending tx (the undo form).
    std::vector<chain::output_cptrs> spent{};
    for (const auto& tx: query.to_spending_txs(link))
    {
        chain::output_cptrs outs{};
        for (const auto& out: query.to_prevouts(tx))
        {
            const auto output = query.get_output(out);
            if (!output)
            {
                send_internal_server_error(database::error::integrity);
                return;
            }

            outs.push_back(output);
        }

        spent.push_back(std::move(outs));
    }

    // The tx count includes the coinbase, which has no prevouts.
    auto size = variable_size(add1(spent.size())) + variable_size(zero);
    for (const auto& outs: spent)
    {
        size += variable_size(outs.size());
        for (const auto& output: outs)
            size += output->serialized_size();
    }

    data_chunk wire(size);
    stream::out::fast sink{ wire };
    write::bytes::fast writer{ sink };
    writer.write_variable(add1(spent.size()));
    writer.write_variable(zero);
    for (const auto& outs: spent)
    {
        writer.write_variable(outs.size());
        for (const auto& output: outs)
            output->to_data(writer);
    }

    switch (media)
    {
        case data:
        {
            send_chunk(std::move(wire));
            return;
        }
        case text:
        {
            send_text(encode_base16(wire));
            return;
        }
        case json:
        {
            array models{};
            models.emplace_back(array{});
            for (const auto& outs: spent)
            {
                array prevouts{};
                for (const auto& output: outs)
                    prevouts.emplace_back(object
                    {
                        { "value", to_btc(output->value()) },
                        { "scriptPubKey", script_public_key(output->script()) }
                    });

                models.emplace_back(std::move(prevouts));
            }

            send_json(std::move(models));
            return;
        }
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_block_filter(const code& ec,
    rest_interface::block_filter, uint8_t media, const hash_cptr& hash,
    uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    if (!hash || !query.filter_enabled())
    {
        send_not_found();
        return;
    }

    // libbitcoin stores only the neutrino (basic) filter; type is ignored.
    data_chunk filter{};
    if (!query.get_filter_body(filter, query.to_header(*hash)))
    {
        send_not_found();
        return;
    }

    const auto size = add1(hash_size) + variable_size(filter.size()) +
        filter.size();

    switch (media)
    {
        case data:
        {
            data_chunk out(size);
            stream::out::fast sink{ out };
            write::bytes::fast writer{ sink };
            write_filter(writer, *hash, filter);
            send_chunk(std::move(out));
            return;
        }
        case text:
        {
            std::string out{};
            out.reserve(add1(two * size));
            out.resize(two * size);
            stream::out::fast sink{ out };
            write::base16::fast writer{ sink };
            write_filter(writer, *hash, filter);
            send_text(std::move(out));
            return;
        }
        case json:
            send_json(object
            {
                { "filter", encode_base16(filter) }
            });
            return;
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_block_filter_headers(const code& ec,
    rest_interface::block_filter_headers, uint8_t media, const hash_cptr& hash,
    uint8_t, uint32_t count) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    if (!hash || !query.filter_enabled())
    {
        send_not_found();
        return;
    }

    // bitcoind serves filter headers only for a hash on the active chain.
    const auto header_link = query.to_header(*hash);
    if (!query.is_confirmed_block(header_link))
    {
        send_not_found();
        return;
    }

    size_t height{};
    if (!query.get_height(height, header_link))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    const auto limit = lesser(count, messages::peer::max_get_headers);
    const auto links = query.get_confirmed_headers(height, limit);
    if (links.empty())
    {
        send_not_found();
        return;
    }

    hashes filter_heads{};
    filter_heads.reserve(links.size());
    for (const auto& link: links)
    {
        hash_digest filter_head{};
        if (!query.get_filter_head(filter_head, link))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        filter_heads.push_back(filter_head);
    }

    switch (media)
    {
        case data:
        {
            data_chunk out{};
            out.reserve(filter_heads.size() * hash_size);
            for (const auto& head: filter_heads)
                out.insert(out.end(), head.begin(), head.end());

            send_chunk(std::move(out));
            return;
        }
        case text:
        {
            std::string out{};
            out.reserve(filter_heads.size() * two * hash_size);
            for (const auto& head: filter_heads)
                out += encode_base16(head);

            send_text(std::move(out));
            return;
        }
        case json:
        {
            array models{};
            for (const auto& head: filter_heads)
                models.emplace_back(encode_hash(head));

            send_json(std::move(models));
            return;
        }
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_chain_information(const code& ec,
    rest_interface::chain_information) NOEXCEPT
{
    if (stopped(ec))
        return;

    size_t blocks{};
    const auto& query = archive();
    const auto link = query.to_top_confirmed();
    const auto header = query.get_header(link);
    if (!header || !query.get_height(blocks, link))
    {
        send_not_found();
        return;
    }

    send_json(object
    {
        { "chain", chain_name(system_settings()) },
        { "blocks", blocks },
        { "headers", query.get_top_candidate() },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(header->bits())) },
        { "difficulty", header->difficulty() },
        { "time", header->timestamp() },
        { "mediantime", median_time(query, system_settings(), link) },
        { "pruned", node_settings().limited_blocks }
    });
}

// bitcoind's bip64 form: dummy version, height, output.
void protocol_bitcoind_rest::handle_get_utxos(const code& ec,
    rest_interface::get_utxos, uint8_t media, const array_t& outpoints,
    bool mempool) NOEXCEPT
{
    if (stopped(ec))
        return;

    // bitcoind's bip64 outpoint limit.
    constexpr size_t maximum_outpoints = 15;

    // bitcoind's height of a pooled output.
    constexpr size_t mempool_height = 0x7fffffff;

    if (outpoints.empty() || outpoints.size() > maximum_outpoints)
    {
        send_bad_request();
        return;
    }

    struct utxo { uint32_t height; chain::output::cptr out; };
    const auto& query = archive();
    std::vector<bool> hits{};
    std::vector<utxo> utxos{};
    for (const auto& item: outpoints)
    {
        const auto& fields = std::get<object_t>(item.value());
        const auto& any = std::get<any_t>(fields.at("hash").value());
        const auto hash = any.get<const hash_digest>();
        const auto index = std::get<uint32_t>(fields.at("index").value());

        const auto output_link = query.to_output(*hash, index);
        const auto tx = query.to_output_tx(output_link);
        const auto confirmed = !output_link.is_terminal() &&
            query.is_confirmed_output(output_link);
        const auto pooled = mempool && !confirmed && !tx.is_terminal() &&
            query.is_pooled(tx) && query.is_confirmable_tx(tx);
        const auto spent = query.is_confirmed_spent(output_link) ||
            (mempool && query.is_confirmable_spent(output_link));
        const auto hit = (confirmed || pooled) && !spent;
        hits.push_back(hit);
        if (!hit)
            continue;

        size_t height{ mempool_height };
        const auto output = query.get_output(output_link);
        if (!output || (confirmed && !query.get_tx_height(height, tx)))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        utxos.push_back({ possible_narrow_cast<uint32_t>(height), output });
    }

    size_t top{};
    const auto top_link = query.to_top_confirmed();
    if (!query.get_height(top, top_link))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    const auto top_hash = query.get_header_key(top_link);

    // The bip64 bitmap flags each outpoint hit, lsb first within each byte.
    data_chunk bitmap(ceilinged_divide(hits.size(), 8u), 0x00);
    for (size_t index = 0; index < hits.size(); ++index)
        if (hits.at(index))
            bitmap.at(index / 8u) |= possible_narrow_cast<uint8_t>(
                shift_left(one, index % 8u));

    auto size = sizeof(uint32_t) + hash_size +
        variable_size(bitmap.size()) + bitmap.size() +
        variable_size(utxos.size());
    for (const auto& unspent: utxos)
        size += two * sizeof(uint32_t) + unspent.out->serialized_size();

    data_chunk wire(size);
    stream::out::fast sink{ wire };
    write::bytes::fast writer{ sink };
    writer.write_4_bytes_little_endian(possible_narrow_cast<uint32_t>(top));
    writer.write_bytes(top_hash);
    writer.write_variable(bitmap.size());
    writer.write_bytes(bitmap);
    writer.write_variable(utxos.size());
    for (const auto& unspent: utxos)
    {
        writer.write_4_bytes_little_endian(0);
        writer.write_4_bytes_little_endian(unspent.height);
        unspent.out->to_data(writer);
    }

    switch (media)
    {
        case data:
        {
            send_chunk(std::move(wire));
            return;
        }
        case text:
        {
            send_text(encode_base16(wire));
            return;
        }
        case json:
        {
            std::string bits{};
            for (const auto hit: hits)
                bits += hit ? "1" : "0";

            array models{};
            for (const auto& unspent: utxos)
                models.emplace_back(object
                {
                    { "height", unspent.height },
                    { "value", to_btc(unspent.out->value()) },
                    { "scriptPubKey", script_public_key(unspent.out->script()) }
                });

            send_json(object
            {
                { "chainHeight", top },
                { "chaintipHash", encode_hash(top_hash) },
                { "bitmap", bits },
                { "utxos", std::move(models) }
            });
            return;
        }
    }

    send_not_found();
}

void protocol_bitcoind_rest::handle_get_mempool(const code& ec,
    rest_interface::mempool, bool verbose, bool mempool_sequence) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (verbose && mempool_sequence)
    {
        send_bad_request();
        return;
    }

    get_confirmable_txs(BIND(handle_mempool, _1, _2, verbose, mempool_sequence,
        gate()));
}

void protocol_bitcoind_rest::handle_mempool(const code& ec,
    const database::tx_links& links, bool verbose, bool sequence,
    const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_mempool, ec, links, verbose, sequence, gate);
}

void protocol_bitcoind_rest::complete_mempool(const code& ec,
    const database::tx_links& links, bool verbose, bool sequence,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec == node::error::pooling_disabled)
    {
        send_not_found();
        return;
    }

    system::json::value out{};
    if (ec || !raw_mempool(out, archive(), links, verbose, sequence))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    send_json(std::move(out));
}

// Minimum is that sent to peers (feefilter), relay minimum is configured rate.
void protocol_bitcoind_rest::handle_get_mempool_information(const code& ec,
    rest_interface::mempool_information) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto relay = node_settings().pool.minimum_fee_rate_();
    get_confirmable_fees(BIND(handle_mempool_information, _1, _2,
        minimum_fee_rate(), relay, relay, gate()));
}

void protocol_bitcoind_rest::handle_mempool_information(const code& ec,
    const database::fee_rates& rates, uint64_t minimum, uint64_t relay,
    uint64_t increment, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_mempool_information, ec, rates, minimum, relay, increment,
        gate);
}

void protocol_bitcoind_rest::complete_mempool_information(const code& ec,
    const database::fee_rates& rates, uint64_t minimum, uint64_t relay,
    uint64_t increment, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec == node::error::pooling_disabled)
    {
        send_not_found();
        return;
    }

    object out{};
    if (ec || !mempool_info(out, archive(), rates, database_settings().path,
        minimum, relay, increment))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    send_json(std::move(out));
}

void protocol_bitcoind_rest::handle_get_deployment_info(const code& ec,
    rest_interface::deployment_info,
    const std::optional<hash_cptr>& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    auto link = query.to_top_confirmed();

    // bitcoind reports an unknown block as a bad request here.
    if (hash.has_value())
    {
        link = query.to_header(*hash.value());
        if (link.is_terminal())
        {
            send_bad_request();
            return;
        }
    }

    size_t height{};
    if (!query.get_height(height, link))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    const auto doc = deployment_info(query, system_settings(), link, height);
    send_json(value_from(doc));
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::send_text(std::string&& text) NOEXCEPT
{
    BC_ASSERT(stranded());
    text.push_back('\n');
    server::protocol_http::send_text(std::move(text));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
