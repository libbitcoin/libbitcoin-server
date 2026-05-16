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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <atomic>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;

#define CLASS protocol_native

bool protocol_native::handle_get_configuration(const code& ec,
    interface::configuration, uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    boost::json::object object
    {
        { "address", archive().address_enabled() },
        { "filter", archive().filter_enabled() },
        { "turbo", database_settings().turbo },
        { "witness", network_settings().witness_node() },
        { "retarget", system_settings().forks.retarget },
        { "difficult", system_settings().forks.difficult },
    };
    BC_POP_WARNING()

    send_json(std::move(object), 64);
    return true;
}

bool protocol_native::handle_get_log_subscribe(const code& ec,
    interface::log_subscribe, uint8_t , uint8_t ,
    bool stop) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // TODO: add levels param, if empty implies stop.
    // TODO: implement as bit flags that map to levels here.
    log_subscribe_.store(stop, std::memory_order_relaxed);
    if (stop)
    {
        send_empty();
        return true;
    }

    // handle_log will unsubscribe when !log_subscribe_ upon any message.
    log.subscribe_messages(BIND(handle_log, _1, _2, _3, _4));
    send_empty();
    return true;
}

bool protocol_native::handle_get_event_subscribe(const code& ec,
    interface::event_subscribe, uint8_t , uint8_t ,
    bool stop) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // TODO: add events param, if empty implies stop.
    // TODO: implement as bit flags that map to events here.
    event_subscribe_.store(stop, std::memory_order_relaxed);
    if (stop)
    {
        send_empty();
        return true;
    }

    // handle_events will unsubscribe when !log_subscribe_ upon any event.
    log.subscribe_events(BIND(handle_events, _1, _2, _3, _4));
    send_empty();
    return true;
}

} // namespace server
} // namespace libbitcoin
