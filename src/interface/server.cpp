/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/interface/server.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/version.hpp>

// TODO: rename this class to minimize namespace conflicts.

namespace libbitcoin {
namespace server {

using namespace bc::system;

void server::version(server_node&, const message& request,
    send_handler handler)
{
    static const std::string version{ LIBBITCOIN_SERVER_VERSION };

    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        to_chunk(version)
    });

    handler(message(request, std::move(result)));
}

} // namespace server
} // namespace libbitcoin
