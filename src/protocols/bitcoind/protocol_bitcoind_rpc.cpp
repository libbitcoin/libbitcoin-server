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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/bitcoind_json.hpp>
#include <bitcoin/network/messages/peer/peer.hpp>
#include <bitcoin/system/chain/json/json.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_rpc
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::json;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rpc::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_best_block_hash, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_chain_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_count, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_filter, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_hash, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_header, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_block_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_chain_tx_stats, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_chain_work, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_tx_out, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_get_tx_out_set_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prune_block_chain, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_save_mem_pool, _1, _2);
    SUBSCRIBE_BITCOIND(handle_scan_tx_out_set, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_chain, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_verify_tx_out_set, _1, _2, _3);

    SUBSCRIBE_BITCOIND(handle_get_network_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_transaction, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_send_raw_transaction, _1, _2, _3, _4);
    network::protocol_http::start();
}

void protocol_bitcoind_rpc::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rpc_dispatcher_.stop(ec);
    network::protocol_http::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

// Handled here for rpc and derived rest protocol.
void protocol_bitcoind_rpc::handle_receive_options(const code& ec,
    const options::cptr& options) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*options, options->version()))
    {
        send_bad_host(*options);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*options, options->version()))
    {
        send_forbidden(*options);
        return;
    }

    send_ok(*options);
}

// Derived rest protocol handles get and rpc handles post.
void protocol_bitcoind_rpc::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Enforce http host header (if any hosts are configured).
    if (!is_allowed_host(*post, post->version()))
    {
        send_bad_host(*post);
        return;
    }

    // Enforce http origin policy (if any origins are configured).
    if (!is_allowed_origin(*post, post->version()))
    {
        send_forbidden(*post);
        return;
    }

    // Endpoint accepts only json-rpc posts.
    if (!post->body().contains<rpc::request>())
    {
        send_bad_request(*post);
        return;
    }

    // Get the parsed json-rpc request object.
    // v1 or v2 both supported, batch not yet supported.
    // v1 null id and v2 missing id implies notification and no response.
    const auto& message = post->body().get<rpc::request>().message;

    // The post is saved off during asynchonous handling and used in send_json
    // to formulate response headers, isolating handlers from http semantics.
    set_rpc_request(message.jsonrpc, message.id, post);

    // Dispatch the request to subscribers.
    if (const auto code = rpc_dispatcher_.notify(message))
        stop(code);
}

template <typename Object, typename ...Args>
std::string to_hex(const Object& object, size_t size, Args&&... args) NOEXCEPT
{
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    object.to_data(writer, std::forward<Args>(args)...);
    BC_ASSERT(writer);
    return out;
}

// Handlers.
// ----------------------------------------------------------------------------
// github.com/bitcoin/bitcoin/blob/master/doc/JSON-RPC-interface.md
// TODO: precompute size for buffer hints.

bool protocol_bitcoind_rpc::handle_get_best_block_hash(const code& ec,
    rpc_interface::get_best_block_hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto hash = archive().get_top_confirmed_hash();
    send_result(encode_hash(hash), two * system::hash_size);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block(const code& ec,
    rpc_interface::get_block, const std::string& blockhash,
    double verbosity) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(hash);

    if (verbosity == 0.0)
    {
        const auto block = query.get_block(link, witness);
        if (is_null(block))
        {
            send_error(error::not_found, blockhash, blockhash.size());
            return true;
        }

        send_text(to_hex(*block, block->serialized_size(witness), witness));
        return true;
    }

    if (verbosity == 1.0 || verbosity == 2.0)
    {
        const auto block = query.get_block(link, witness);
        if (is_null(block))
        {
            send_error(error::not_found, blockhash, blockhash.size());
            return true;
        }

        // verbosity 1 lists txids; verbosity 2 embeds full tx objects.
        auto model = (verbosity == 1.0) ?
            value_from(bitcoind_hashed(*block)) :
            value_from(bitcoind_verbose(*block));

        inject_block_context(model.as_object(), query, link, block->header());
        send_result(rpc::value_t(std::move(model)),
            two * block->serialized_size(witness));
        return true;
    }

    send_error(error::invalid_argument);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_chain_info(const code& ec,
    rpc_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto blocks = query.get_top_confirmed();
    const auto link = query.to_confirmed(blocks);
    const auto header = query.get_header(link);
    if (is_null(header))
    {
        send_error(database::error::integrity);
        return true;
    }

    send_result(rpc::object_t
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

bool protocol_bitcoind_rpc::handle_get_block_count(const code& ec,
    rpc_interface::get_block_count) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(rpc::value_t(static_cast<uint64_t>(
        archive().get_top_confirmed())), 20);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_filter(const code& ec,
    rpc_interface::get_block_filter, const std::string& blockhash,
    const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    const auto link = query.to_header(hash);
    data_chunk filter{};
    hash_digest filter_header{};
    if (!query.get_filter_body(filter, link) ||
        !query.get_filter_head(filter_header, link))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    send_result(rpc::object_t
    {
        { "filter", encode_base16(filter) },
        { "header", encode_hash(filter_header) }
    }, two * filter.size());
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_hash(const code& ec,
    rpc_interface::get_block_hash, double height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (height < 0.0)
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_confirmed(static_cast<size_t>(height));
    if (link.is_terminal())
    {
        send_error(error::not_found);
        return true;
    }

    send_result(encode_hash(query.get_header_key(link)), two * hash_size);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_header(const code& ec,
    rpc_interface::get_block_header, const std::string& blockhash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(hash);
    const auto header = query.get_header(link);
    if (is_null(header))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    if (!verbose)
    {
        send_text(to_hex(*header, chain::header::serialized_size()));
        return true;
    }

    auto out = header_to_bitcoind(*header);
    out["nTx"] = static_cast<uint64_t>(query.get_tx_count(link));
    inject_block_context(out, query, link, *header);
    send_result(rpc::value_t(boost::json::value(std::move(out))), 512);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_stats(const code& ec,
    rpc_interface::get_block_stats, const std::string&,
    const network::rpc::array_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_chain_tx_stats(const code& ec,
    rpc_interface::get_chain_tx_stats, double, const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_chain_work(const code& ec,
    rpc_interface::get_chain_work) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_tx_out(const code& ec,
    rpc_interface::get_tx_out, const std::string& txid, double n,
    bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, txid) || n < 0.0)
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto index = static_cast<uint32_t>(n);
    const auto output_fk = query.to_output(hash, index);

    // Core returns json null for a missing or spent output (mempool ignored).
    if (output_fk.is_terminal() || query.is_spent(output_fk))
    {
        send_result(rpc::value_t{}, 4);
        return true;
    }

    const auto output = query.get_output(output_fk);
    if (is_null(output))
    {
        send_result(rpc::value_t{}, 4);
        return true;
    }

    const auto tx_fk = query.to_tx(hash);
    size_t tx_height{};
    const auto have_height = query.get_tx_height(tx_height, tx_fk);
    const auto top = query.get_top_confirmed();

    send_result(rpc::object_t
    {
        { "bestblock", encode_hash(query.get_top_confirmed_hash()) },
        { "confirmations", have_height ?
            static_cast<int64_t>(top - tx_height + 1u) : int64_t{ 0 } },
        { "value", static_cast<double>(output->value()) / 100000000.0 },
        { "scriptPubKey", value_from(bitcoind(output->script())) },
        { "coinbase", query.is_coinbase(tx_fk) }
    }, 256);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_tx_out_set_info(const code& ec,
    rpc_interface::get_tx_out_set_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_prune_block_chain(const code& ec,
    rpc_interface::prune_block_chain, double) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_save_mem_pool(const code& ec,
    rpc_interface::save_mem_pool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_scan_tx_out_set(const code& ec,
    rpc_interface::scan_tx_out_set, const std::string&,
    const network::rpc::array_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_verify_chain(const code& ec,
    rpc_interface::verify_chain, double, double) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_verify_tx_out_set(const code& ec,
    rpc_interface::verify_tx_out_set, const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_network_info(const code& ec,
    rpc_interface::get_network_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // libbitcoin-server is a node, not a wallet/peer-introspection service;
    // peer-dependent fields (connections, addresses) are reported as empty.
    // TODO: surface live connection count and relay fee from node settings.
    send_result(rpc::object_t
    {
        { "version", 0 },
        { "subversion", std::string{ "/libbitcoin:server/" } },
        { "protocolversion", 70016 },
        { "localrelay", true },
        { "timeoffset", 0 },
        { "connections", 0 },
        { "networkactive", true },
        { "networks", rpc::array_t{} },
        { "relayfee", 0.00001 },
        { "incrementalfee", 0.00001 },
        { "localaddresses", rpc::array_t{} },
        { "warnings", std::string{} }
    }, 256);
    return true;
}

// Rawtransactions methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_rpc::handle_get_raw_transaction(const code& ec,
    rpc_interface::get_raw_transaction, const std::string& txid,
    double verbose, const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The blockhash hint is unused: libbitcoin archives all tx (global index).
    hash_digest hash{};
    if (!decode_hash(hash, txid))
    {
        send_error(error::not_found, txid, txid.size());
        return true;
    }

    constexpr auto witness = true;
    auto& query = archive();
    const auto link = query.to_tx(hash);
    const auto tx = query.get_transaction(link, witness);
    if (is_null(tx))
    {
        send_error(error::not_found, txid, txid.size());
        return true;
    }

    if (verbose == 0.0)
    {
        send_text(to_hex(*tx, tx->serialized_size(witness), witness));
        return true;
    }

    // bitcoind() (not bitcoind_verbose) yields Core's tx fields: txid/hash/
    // size/vsize/weight/vin/vout/hex (bitcoind_verbose on a standalone tx
    // falls back to libbitcoin's plain inputs/outputs form).
    auto model = value_from(bitcoind(*tx));
    inject_tx_context(model.as_object(), query, link);
    send_result(rpc::value_t(std::move(model)),
        two * tx->serialized_size(witness));
    return true;
}

bool protocol_bitcoind_rpc::handle_send_raw_transaction(const code& ec,
    rpc_interface::send_raw_transaction, const std::string& hexstring,
    double) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto tx = std::make_shared<const chain::transaction>(data, true);
    if (!tx->is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    auto& query = archive();
    const auto txid = tx->hash(false);

    // Archive (so the out-relay can serve getdata) only if not already known.
    // TODO: contextual validation (populate_with_metadata + connect) for policy.
    if (query.to_tx(txid).is_terminal())
    {
        if (tx->check())
        {
            send_error(error::invalid_argument);
            return true;
        }

        if (query.set_code(*tx))
        {
            send_error(database::error::integrity);
            return true;
        }
    }

    // Announce to peers; protocol_transaction_out_106 serves the tx on getdata.
    broadcast<messages::peer::transaction>(
        std::make_shared<const messages::peer::transaction>(
            messages::peer::transaction{ tx }));

    send_result(encode_hash(txid), two * system::hash_size);
    return true;
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_bitcoind_rpc::send_error(const code& ec) NOEXCEPT
{
    send_error(ec, two * ec.message().size());
}

void protocol_bitcoind_rpc::send_error(const code& ec,
    size_t size_hint) NOEXCEPT
{
    send_error(ec, {}, size_hint);
}

void protocol_bitcoind_rpc::send_error(const code& ec,
    rpc::value_option&& error, size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .error = rpc::result_t
        {
            .code = ec.value(),
            .message = ec.message(),
            .data = std::move(error)
        }
    }, size_hint);
}

void protocol_bitcoind_rpc::send_text(std::string&& hexidecimal) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_result(hexidecimal, hexidecimal.size());
}

void protocol_bitcoind_rpc::send_result(rpc::value_option&& result,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_rpc(
    {
        .jsonrpc = version_,
        .id = id_,
        .result = std::move(result)
    }, size_hint);
}

// private
void protocol_bitcoind_rpc::send_rpc(rpc::response_t&& model,
    size_t size_hint) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace http;
    static const auto json = from_media_type(media_type::application_json);
    const auto request = reset_rpc_request();
    http::response message{ status::ok, request->version() };
    add_common_headers(message, *request);
    add_access_control_headers(message, *request);
    message.set(field::content_type, json);
    message.body() = rpc::response
    {
        { .size_hint = size_hint }, std::move(model),
    };
    message.prepare_payload();
    SEND(std::move(message), handle_complete, _1, error::success);
}

// private
void protocol_bitcoind_rpc::set_rpc_request(rpc::version version,
    const rpc::id_option& id, const http::request_cptr& request) NOEXCEPT
{
    BC_ASSERT(stranded());
    id_ = id;
    version_ = version;
    set_request(request);
}

// private
http::request_cptr protocol_bitcoind_rpc::reset_rpc_request() NOEXCEPT
{
    BC_ASSERT(stranded());
    id_.reset();
    version_ = rpc::version::undefined;
    return reset_request();
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
