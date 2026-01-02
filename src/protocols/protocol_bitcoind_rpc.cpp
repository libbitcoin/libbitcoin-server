/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

    if (verbosity == 1.0)
    {
        send_error(error::not_implemented);
        return true;
    }

    if (verbosity == 2.0)
    {
        send_error(error::not_implemented);
        return true;
    }

    send_error(error::invalid_argument);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_chain_info(const code& ec,
    rpc_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_count(const code& ec,
    rpc_interface::get_block_count) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_filter(const code& ec,
    rpc_interface::get_block_filter, const std::string&,
    const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_hash(const code& ec,
    rpc_interface::get_block_hash, network::rpc::number_t) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_header(const code& ec,
    rpc_interface::get_block_header, const std::string&, bool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
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
    rpc_interface::get_tx_out, const std::string&, double, bool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
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
