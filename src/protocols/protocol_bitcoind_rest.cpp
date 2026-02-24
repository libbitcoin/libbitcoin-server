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

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

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

    // The post is saved off during asynchonous handling and used in send_json
    // to formulate response headers, isolating handlers from http semantics.
    set_request(get);

    ////if (const auto code = rest_dispatcher_.notify({}))
    ////    stop(code);
    protocol_bitcoind_rpc::handle_receive_get(ec, get);
}

// Handlers.
// ----------------------------------------------------------------------------

////constexpr auto data = to_value(media_type::application_octet_stream);
////constexpr auto json = to_value(media_type::application_json);
////constexpr auto text = to_value(media_type::text_plain);

bool protocol_bitcoind_rest::handle_get_block(const code& ec,
    rest_interface::block, uint8_t , system::hash_cptr ) NOEXCEPT
{
    if (stopped(ec))
        return false;

    ////const auto& query = archive();
    ////if (const auto block = query.get_block(query.to_header(*hash), true))
    ////{
    ////    const auto size = block->serialized_size(true);
    ////    switch (media)
    ////    {
    ////        case data:
    ////            send_chunk(to_bin(*block, size, true));
    ////            return true;
    ////        case text:
    ////            send_text(to_hex(*block, size, true));
    ////            return true;
    ////        case json:
    ////            send_json(value_from(block), two * size);
    ////            return true;
    ////    }
    ////}

    send_not_found();
    return true;
}

// private
// ----------------------------------------------------------------------------

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
