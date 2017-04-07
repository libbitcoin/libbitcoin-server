/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/interface/unsubscribe.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

void unsubscribe::address(server_node& node, const message& request,
    send_handler handler)
{
    static constexpr size_t address_args_size = short_hash_size;

    const auto& data = request.data();

    if (data.size() != address_args_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // [ address_hash:20 ]
    auto deserial = make_safe_deserializer(data.begin(), data.end());
    auto address_hash = deserial.read_short_hash();

    auto ec = node.subscribe_address(request, std::move(address_hash), true);
    handler(message(request, ec));
}

void unsubscribe::stealth(server_node& node, const message& request,
    send_handler handler)
{
    static constexpr size_t stealth_args_min = sizeof(uint8_t);
    static constexpr size_t stealth_args_max = stealth_args_min +
        sizeof(uint32_t);

    const auto& data = request.data();
    const auto size = data.size();

    if (size < stealth_args_min || size > stealth_args_max)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // [ prefix_bitsize:1 ]
    // [ prefix_blocks:1..4 ]
    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto bits = deserial.read_byte();
    const auto bytes = deserial.read_bytes(binary::blocks_size(bits));

    auto ec = node.subscribe_stealth(request, binary(bits, bytes), true);
    handler(message(request, ec));
}

} // namespace server
} // namespace libbitcoin
