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
#include <bitcoin/server/protocols/protocol_bitcoind_notifications.hpp>

#include <bitcoin/server/protocols/protocol_bitcoind_zmq.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_notifications
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_notifications::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_zmq_notifications, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Notifications methods.
// ----------------------------------------------------------------------------

// Each configured publisher binding carries every topic (as bitcoind, one
// entry per notifier). There is no high water mark, reported as unbounded.
static void add_zmq_notifications(array_t& notifications,
    const network::config::authorities& bindings) NOEXCEPT
{
    for (const auto& bind: bindings)
    {
        const auto address = "tcp://" + bind.to_string();
        for (const auto topic: protocol_bitcoind_zmq::topics)
        {
            notifications.push_back(object_t
            {
                { "type", "pub" + std::string{ topic } },
                { "address", address },
                { "hwm", 0 }
            });
        }
    }
}

// The clear (NULL) and secured (CURVE) bindings are reported alike.
bool protocol_bitcoind_notifications::handle_get_zmq_notifications(const code& ec,
    rpc_interface::get_zmq_notifications) NOEXCEPT
{
    if (stopped(ec))
        return false;

    array_t notifications{};
    const auto& zmq = server_settings().bitcoind_zmq;
    add_zmq_notifications(notifications, zmq.binds);
    add_zmq_notifications(notifications, zmq.safes);
    send_result(std::move(notifications), 2);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
