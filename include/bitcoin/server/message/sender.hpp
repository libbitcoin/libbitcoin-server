/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_SENDER_HPP
#define LIBBITCOIN_SERVER_SENDER_HPP

#include <czmq++/czmqpp.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/message/message_outgoing.hpp>

namespace libbitcoin {
namespace server {

/**
 * We don't want to block the originating threads that execute a send
 * as that would slow down requests if they all have to sync access
 * to a single socket.
 *
 * Instead we have a queue (push socket) where send requests are pushed,
 * and then the sender is notified. The worker wakes up and pushes
 * all pending requests to the socket.
 */
class BCS_API sender
{
public:
    sender(czmqpp::context& context);

    void queue(const message_outgoing& message);

private:
    czmqpp::context& context_;
};

} // namespace server
} // namespace libbitcoin

#endif
