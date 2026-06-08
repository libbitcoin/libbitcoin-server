/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

#include <algorithm>
#include <string>
#include <vector>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/protocols/bitcoind_json.hpp>
#include <bitcoin/system/chain/json/json.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_rest
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network::rpc;
using namespace network::http;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_hash, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_txs, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_block_part, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_BITCOIND(handle_get_block_spent_tx_outputs, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_filter, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_block_filter_headers, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_chain_information, _1, _2);
    protocol_bitcoind_rpc::start();
}

void protocol_bitcoind_rest::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rest_dispatcher_.stop(ec);
    protocol_bitcoind_rpc::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

// Base rpc protocol handles options and post, this derived handles get.
void protocol_bitcoind_rest::handle_receive_get(const code& ec,
    const get::cptr& get) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

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

    // The get is saved off during asynchonous handling and used in send_json
    // to formulate response headers, isolating handlers from http semantics.
    set_request(get);

    // Parse the REST url into a json-rpc model and dispatch to a handler.
    request_t model{};
    if (bitcoind_target(model, get->target()))
    {
        send_not_found();
        return;
    }

    if (rest_dispatcher_.notify(model))
        send_not_found();
}

// Handlers.
// ----------------------------------------------------------------------------

constexpr auto data = to_value(media_type::application_octet_stream);
constexpr auto json = to_value(media_type::application_json);
constexpr auto text = to_value(media_type::text_plain);

template <typename Object, typename ...Args>
data_chunk to_bin(const Object& object, size_t size, Args&&... args) NOEXCEPT
{
    data_chunk out(size);
    stream::out::fast sink{ out };
    write::bytes::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    return out;
}

template <typename Object, typename ...Args>
std::string to_hex(const Object& object, size_t size, Args&&... args) NOEXCEPT
{
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    return out;
}

bool protocol_bitcoind_rest::handle_get_block(const code& ec,
    rest_interface::block, uint8_t media, system::hash_cptr hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!hash)
    {
        send_not_found();
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto block = query.get_block(query.to_header(*hash), witness);
    if (is_null(block))
    {
        send_not_found();
        return true;
    }

    const auto size = block->serialized_size(witness);
    switch (media)
    {
        case data:
            send_data(to_bin(*block, size, witness));
            return true;
        case text:
            send_hex(to_hex(*block, size, witness));
            return true;
        case json:
            send_dom(value_from(bitcoind_verbose(*block)), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_hash(const code& ec,
    rest_interface::block_hash, uint8_t media, uint32_t height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = query.to_confirmed(height);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    const auto hash = query.get_header_key(link);
    switch (media)
    {
        case data:
            send_data(to_chunk(hash));
            return true;
        case text:
            send_hex(encode_base16(hash));
            return true;
        case json:
            send_dom(boost::json::object{ { "blockhash", encode_hash(hash) } },
                two * hash_size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_txs(const code& ec,
    rest_interface::block_txs, uint8_t media, system::hash_cptr hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!hash)
    {
        send_not_found();
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto block = query.get_block(query.to_header(*hash), witness);
    if (is_null(block))
    {
        send_not_found();
        return true;
    }

    // notxdetails: raw block for bin/hex, txid list (bitcoind_hashed) for json.
    const auto size = block->serialized_size(witness);
    switch (media)
    {
        case data:
            send_data(to_bin(*block, size, witness));
            return true;
        case text:
            send_hex(to_hex(*block, size, witness));
            return true;
        case json:
            send_dom(value_from(bitcoind_hashed(*block)), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_headers(const code& ec,
    rest_interface::block_headers, uint8_t media, system::hash_cptr hash,
    uint32_t count) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!hash || is_zero(count))
    {
        send_not_found();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(*hash);
    size_t height{};
    if (link.is_terminal() || !query.get_height(height, link))
    {
        send_not_found();
        return true;
    }

    // Core caps the header count at 2000.
    constexpr size_t maximum = 2000;
    constexpr auto header_size = chain::header::serialized_size();
    const auto top = query.get_top_confirmed();
    const auto limit = std::min(static_cast<size_t>(count), maximum);

    std::vector<chain::header::cptr> headers{};
    headers.reserve(limit);
    for (size_t index = 0; index < limit && height + index <= top; ++index)
    {
        const auto header = query.get_header(query.to_confirmed(height + index));
        if (!header)
            break;

        headers.push_back(header);
    }

    if (headers.empty())
    {
        send_not_found();
        return true;
    }

    switch (media)
    {
        case data:
        {
            data_chunk out{};
            out.reserve(headers.size() * header_size);
            for (const auto& header: headers)
            {
                const auto bin = to_bin(*header, header_size);
                out.insert(out.end(), bin.begin(), bin.end());
            }

            send_data(std::move(out));
            return true;
        }
        case text:
        {
            std::string out{};
            out.reserve(headers.size() * two * header_size);
            for (const auto& header: headers)
                out += to_hex(*header, header_size);

            send_hex(std::move(out));
            return true;
        }
        case json:
        {
            boost::json::array out{};
            out.reserve(headers.size());
            for (const auto& header: headers)
                out.push_back(header_to_bitcoind(*header));

            send_dom(std::move(out), headers.size() * two * header_size);
            return true;
        }
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_part(const code& ec,
    rest_interface::block_part, uint8_t media, system::hash_cptr hash,
    uint32_t offset, uint32_t size) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!hash)
    {
        send_not_found();
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto block = query.get_block(query.to_header(*hash), witness);
    if (is_null(block))
    {
        send_not_found();
        return true;
    }

    const auto full = to_bin(*block, block->serialized_size(witness), witness);
    if (offset >= full.size())
    {
        send_not_found();
        return true;
    }

    const auto end = std::min(static_cast<size_t>(offset) + size, full.size());
    data_chunk part(full.begin() + offset, full.begin() + end);
    switch (media)
    {
        case data:
            send_data(std::move(part));
            return true;
        case text:
            send_hex(encode_base16(part));
            return true;
    }

    // block_part is bin|hex only (json not supported).
    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_spent_tx_outputs(const code& ec,
    rest_interface::block_spent_tx_outputs, uint8_t media,
    system::hash_cptr hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!hash)
    {
        send_not_found();
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto block = query.get_block(query.to_header(*hash), witness);
    if (is_null(block))
    {
        send_not_found();
        return true;
    }

    // Resolve every prevout spent by the block's non-coinbase transactions.
    chain::output_cptrs spent{};
    const auto& txs = *block->transactions_ptr();
    for (size_t tx = 1; tx < txs.size(); ++tx)
        for (const auto& in: *txs.at(tx)->inputs_ptr())
            if (const auto out = query.get_output(query.to_output(in->point())))
                spent.push_back(out);

    size_t size{};
    for (const auto& output: spent)
        size += output->serialized_size();

    switch (media)
    {
        case data:
        {
            data_chunk out(size);
            stream::out::fast sink{ out };
            write::bytes::fast writer{ sink };
            for (const auto& output: spent)
                output->to_data(writer);

            send_data(std::move(out));
            return true;
        }
        case text:
        {
            std::string out(two * size, '\0');
            stream::out::fast sink{ out };
            write::base16::fast writer{ sink };
            for (const auto& output: spent)
                output->to_data(writer);

            send_hex(std::move(out));
            return true;
        }
        case json:
            send_dom(value_from(bitcoind(spent)), two * size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_filter(const code& ec,
    rest_interface::block_filter, uint8_t media, system::hash_cptr hash,
    uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!hash || !query.filter_enabled())
    {
        send_not_found();
        return true;
    }

    // libbitcoin stores only the neutrino (basic) filter; type is ignored.
    data_chunk filter{};
    if (!query.get_filter_body(filter, query.to_header(*hash)))
    {
        send_not_found();
        return true;
    }

    switch (media)
    {
        case data:
            send_data(std::move(filter));
            return true;
        case text:
            send_hex(encode_base16(filter));
            return true;
        case json:
            send_dom(boost::json::object
            {
                { "filter", encode_base16(filter) }
            }, two * filter.size());
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_block_filter_headers(const code& ec,
    rest_interface::block_filter_headers, uint8_t media, system::hash_cptr hash,
    uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!hash || !query.filter_enabled())
    {
        send_not_found();
        return true;
    }

    hash_digest filter_head{};
    if (!query.get_filter_head(filter_head, query.to_header(*hash)))
    {
        send_not_found();
        return true;
    }

    switch (media)
    {
        case data:
            send_data(to_chunk(filter_head));
            return true;
        case text:
            send_hex(encode_base16(filter_head));
            return true;
        case json:
            send_dom(boost::json::object
            {
                { "filter_header", encode_hash(filter_head) }
            }, two * hash_size);
            return true;
    }

    send_not_found();
    return true;
}

bool protocol_bitcoind_rest::handle_get_chain_information(const code& ec,
    rest_interface::chain_information) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto blocks = query.get_top_confirmed();
    const auto link = query.to_confirmed(blocks);
    const auto header = query.get_header(link);
    if (is_null(header))
    {
        send_not_found();
        return true;
    }

    send_dom(boost::json::object
    {
        { "chain", chain_name(query) },
        { "blocks", static_cast<uint64_t>(blocks) },
        { "headers", static_cast<uint64_t>(query.get_top_candidate()) },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(header->bits())) },
        { "difficulty", header->difficulty() },
        { "time", header->timestamp() },
        { "mediantime", median_time_past(query, blocks) },
        { "pruned", false }
    }, 256);
    return true;
}

// Raw-http response senders (mirror protocol_html, not json-rpc enveloped).
// ----------------------------------------------------------------------------

void protocol_bitcoind_rest::send_data(data_chunk&& bytes) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    network::http::response message{ network::http::status::ok,
        request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(network::http::field::content_type, network::http::
        from_media_type(media_type::application_octet_stream));
    message.body() = std::move(bytes);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_bitcoind_rest::send_hex(std::string&& text) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    network::http::response message{ network::http::status::ok,
        request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(network::http::field::content_type, network::http::
        from_media_type(media_type::text_plain));
    message.body() = std::move(text);
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

void protocol_bitcoind_rest::send_dom(boost::json::value&& model,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto request = reset_request();
    network::http::response message{ network::http::status::ok,
        request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(network::http::field::content_type, network::http::
        from_media_type(media_type::application_json));
    message.body() = network::http::json_value
    {
        .model = std::move(model),
        .size_hint = size_hint
    };
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

// private
// ----------------------------------------------------------------------------

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
