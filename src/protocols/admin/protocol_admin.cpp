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
#include <bitcoin/server/protocols/protocol_admin.hpp>

#include <atomic>
#include <chrono>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::http;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

#define CLASS protocol_admin
#define SUBSCRIBE_ADMIN(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

// Limited by json number domain and sentinel bit.
static_assert(node::events::unknown < 53u);
static_assert(network::levels::verbose < 53u);

// Start.
// ----------------------------------------------------------------------------

void protocol_admin::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Subscription methods.
    SUBSCRIBE_ADMIN(handle_get_log_subscribe, _1, _2, _3, _4);
    SUBSCRIBE_ADMIN(handle_get_event_subscribe, _1, _2, _3, _4);
    protocol_html::start();
}

void protocol_admin::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    dispatcher_.stop(ec);
    protocol_html::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_admin::try_dispatch_object(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto target = request.target();
    BC_POP_WARNING()

    rpc::request_t model{};
    if (const auto ec = admin_target(model, target))
        return !ec;

    if (!admin_query(model, request))
    {
        send_not_acceptable(request);
        return true;
    }

    if (strip_media(model) == media_type::text_html)
        return false;

    if (const auto ec = dispatcher_.notify(model))
        send_internal_server_error(ec, request);

    return true;
}

void protocol_admin::dispatch_websocket(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Admin websocket interface supports only string requests.
    if (!request.body().contains<http::string_value>())
    {
        stop(network::error::not_acceptable);
        return;
    }

    // Target with query string is passed via websocket string body.
    const auto target = request.body().get<http::string_value>();

    rpc::request_t model{};
    if (const auto ec = admin_target(model, target))
        return;

    // Default to json by simulating a json accept header (format overrides).
    if (!admin_query(model, target, { media_type::application_json }) ||
        strip_media(model) == media_type::text_html)
    {
        stop(network::error::not_acceptable);
        return;
    }

    if (const auto ec = dispatcher_.notify(model))
    {
        stop(network::error::internal_server_error);
        return;
    }
}

// Interface handlers.
// ----------------------------------------------------------------------------

bool protocol_admin::handle_get_log_subscribe(const code& ec,
    interface::log_subscribe, uint8_t /*version*/, uint64_t filter) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    uint64_t previous{};
    if (update_filter(previous, filter, log_state_))
        log.subscribe_messages(BIND(handle_log, _1, _2, _3, _4));

    send_json({ { "previous", previous } }, 42);
    return true;
}

bool protocol_admin::handle_get_event_subscribe(const code& ec,
    interface::event_subscribe, uint8_t /*version*/, uint64_t filter) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    uint64_t previous{};
    if (update_filter(previous, filter, event_state_))
        log.subscribe_events(BIND(handle_event, _1, _2, _3, _4));

    send_json({ { "previous", previous } }, 42);
    return true;
}

// Event handlers.
// ----------------------------------------------------------------------------

bool protocol_admin::handle_log(const code& ec, uint8_t level, time_t zulu,
    const std::string& message) NOEXCEPT
{
    bool filtered{};
    if (stopped(ec) || !get_filter(filtered, level, log_state_))
        return false;

    if (!filtered || !websocket())
        return true;

    POST(do_log, level, zulu, message);
    return true;
}

bool protocol_admin::handle_event(const code& ec, uint8_t event_,
    uint64_t value, const logger::time& point) NOEXCEPT
{
    bool filtered{};
    if (stopped(ec) || !get_filter(filtered, event_, event_state_))
        return false;

    if (!filtered || !websocket())
        return true;

    POST(do_event, event_, to_zulu(point), value);
    return true;
}

// Notification event handlers.
// ----------------------------------------------------------------------------
// Posted to the protocol strand.

void protocol_admin::do_log(uint8_t level, time_t zulu,
    const std::string& message) NOEXCEPT
{
    BC_ASSERT(stranded() && websocket());

    if (stopped())
        return;

    notify_json(
    {
        { "level", level },
        { "time", zulu },
        { "message", message }
    }, message.size() + 64u);
}

void protocol_admin::do_event(uint8_t event_, time_t zulu,
    uint64_t value_) NOEXCEPT
{
    BC_ASSERT(stranded() && websocket());

    if (stopped())
        return;

    notify_json(
    {
        { "event", event_ },
        { "value", value_ },
        { "time", zulu }
    }, 64);
}

// Utils.
// ----------------------------------------------------------------------------
// static/private

time_t protocol_admin::to_zulu(const logger::time& point) NOEXCEPT
{
    using namespace std::chrono;
    const auto now = logger::now();
    const auto delta = duration_cast<system_clock::duration>(now - point);
    return system_clock::to_time_t(system_clock::now() - delta);
}

// Unsubscribe if false.
bool protocol_admin::get_filter(bool& filtered, size_t bit,
    filter_t& filter) NOEXCEPT
{
    auto state = filter.load(relaxed);
    const auto next = set_left(state, false);

    // Empty filter unregisters, failure implies a new filter arrived.
    if (is_zero(next) && filter.compare_exchange_strong(state, next, relaxed))
        return false;

    filtered = get_right(state, bit);
    return true;
}

// Subscribe if true.
bool protocol_admin::update_filter(uint64_t& prior, uint64_t value,
    filter_t& filter) NOEXCEPT
{
    const auto next = set_left(value, true);
    const auto state = is_zero(value) ? filter.fetch_and(next, relaxed) :
        filter.exchange(next, relaxed);

    prior = set_left(state, false);
    return !is_zero(value) && !get_left(state);
}

} // namespace server
} // namespace libbitcoin
