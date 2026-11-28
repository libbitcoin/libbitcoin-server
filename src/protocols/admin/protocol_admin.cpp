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
#include <bitcoin/server/protocols/protocol_admin.hpp>

#include <atomic>
#include <chrono>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

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
    SUBSCRIBE_ADMIN(handle_get_mining, _1, _2, _3);
    SUBSCRIBE_ADMIN(handle_get_mining_subscribe, _1, _2, _3);
    protocol_native::start();
}

void protocol_admin::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    log_interest_->set(zero);
    mining_timer_->stop();
    admin_dispatcher_.stop(ec);
    protocol_native::stopping(ec);
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
    {
        // An unrecognized admin target may be a native interface request.
        if (ec == error::invalid_target)
            return protocol_native::try_dispatch_object(request);

        // A target without a version or requesting html is a page request.
        if (ec == error::missing_version || ec == error::empty_path ||
            is_page_request(request))
            return false;

        send_bad_target(ec, *reset_request());
        return true;
    }

    // No media defaults injected for an http request.
    if (!admin_query(model, request))
    {
        send_bad_request();
        return true;
    }

    const auto media = strip_media(model);
    if (media == media_type::unknown)
    {
        send_not_acceptable();
        return true;
    }

    // Falls through to html page dispatch.
    if (media == media_type::text_html)
        return false;

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(model.method))
    {
        send_forbidden();
        return true;
    }

    if (admin_dispatcher_.notify(model))
        send_bad_request();

    return true;
}

void protocol_admin::dispatch_websocket(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // ws frames carry no headers, so ws authorization is enforced here, not by
    // channel (established by basic auth on upgrade request).
    if (!authorized())
    {
        send_unauthorized(request);
        return;
    }

    // Admin websocket interface supports only string requests.
    if (!request.body().contains<http::string_value>())
    {
        send_not_acceptable(request);
        return;
    }

    // Target with query string is passed via websocket string body.
    const auto target = request.body().get<http::string_value>();

    rpc::request_t model{};
    if (const auto ec = admin_target(model, target))
    {
        // An unrecognized admin target may be a native interface request.
        if (ec == error::invalid_target)
            protocol_native::dispatch_websocket(request);
        else
            send_bad_target(ec, request);

        return;
    }

    // Default to json by simulating a json accept header (format overrides).
    if (!admin_query(model, target, { media_type::application_json }))
    {
        send_bad_request(request);
        return;
    }

    if (strip_media(model) != media_type::application_json)
    {
        send_not_acceptable(request);
        return;
    }

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(model.method))
    {
        send_forbidden(request);
        return;
    }

    if (admin_dispatcher_.notify(model))
        send_bad_request(request);
}

// Interface handlers.
// ----------------------------------------------------------------------------

void protocol_admin::handle_get_log_subscribe(const code& ec,
    admin_interface::log_subscribe, uint8_t /*version*/,
    uint64_t filter) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // log outlives node, so cannot hold shared_ptr to the protocol.
    const auto previous = log_interest_->set(filter);
    if (!is_zero(filter) && !log_subscribed_)
    {
        auto handler = BIND_WEAK(handle_log, _1, _2, _3, _4);
        log.subscribe_messages(log_interest_, std::move(handler));
        log_subscribed_ = true;
    }

    send_json({ { "previous", previous } });
}

void protocol_admin::handle_get_event_subscribe(const code& ec,
    admin_interface::event_subscribe, uint8_t /*version*/,
    uint64_t filter) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // log outlives node, so cannot hold shared_ptr to the protocol.
    uint64_t previous{};
    if (update_filter(previous, filter, event_state_))
        log.subscribe_events(BIND_WEAK(handle_event, _1, _2, _3, _4));

    send_json({ { "previous", previous } });
}

void protocol_admin::handle_get_mining(const code& ec,
    admin_interface::mining, uint8_t /*version*/) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    send_json(mining_snapshot());
}

// The snapshot is pushed as channels open or close, and periodically.
void protocol_admin::handle_get_mining_subscribe(const code& ec,
    admin_interface::mining_subscribe, uint8_t /*version*/) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!websocket())
    {
        send_not_acceptable();
        return;
    }

    mining_generation_ = mining()->generation();
    mining_checks_ = zero;
    send_json(mining_snapshot());

    if (!mining_subscribed_)
    {
        mining_subscribed_ = true;
        start_mining();
    }
}

// Event handlers.
// ----------------------------------------------------------------------------

bool protocol_admin::handle_log(const code& ec, uint8_t level, time_t zulu,
    const std::string& message) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (websocket())
    {
        POST(do_log, level, zulu * 1'000_u64, message);
    }

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

void protocol_admin::do_log(uint8_t level, uint64_t zulu,
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
    });
}

void protocol_admin::do_event(uint8_t event_, uint64_t zulu,
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
    });
}

void protocol_admin::start_mining() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    mining_timer_->start(BIND(do_mining, _1),
        seconds{ mining_check_seconds });
}

void protocol_admin::do_mining(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded() && websocket());

    if (stopped() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        stop(ec);
        return;
    }

    const auto generation = mining()->generation();
    if (generation != mining_generation_ ||
        ++mining_checks_ >= mining_snapshot_checks)
    {
        mining_generation_ = generation;
        mining_checks_ = zero;
        notify_json(mining_snapshot());
    }

    start_mining();
}

// Utils.
// ----------------------------------------------------------------------------
// static/private

system::json::value protocol_admin::mining_snapshot() const NOEXCEPT
{
    auto& registry = *mining();
    system::json::array channels{};
    for (const auto& channel: registry.get_channels(miners::now()))
    {
        system::json::array workers{};
        for (const auto& worker: channel.workers)
            workers.emplace_back(worker);

        system::json::object rejections{};
        for (const auto& [result, count]: channel.rejections)
            rejections[result] = count;

        system::json::array shares{};
        for (const auto& share: channel.shares)
            shares.push_back(system::json::object
            {
                { "time", share.time },
                { "height", share.height },
                { "difficulty", share.difficulty },
                { "result", share.result }
            });

        system::json::array history{};
        for (const auto& sample: channel.history)
            history.push_back(system::json::array{ sample.time, sample.rate });

        system::json::value last{};
        if (channel.last_share.has_value())
            last = channel.last_share.value();

        channels.push_back(system::json::object
        {
            { "id", channel.id },
            { "service", channel.service },
            { "transport", channel.transport },
            { "address", channel.address },
            { "client", channel.client },
            { "workers", std::move(workers) },
            { "connected", channel.connected },
            { "difficulty", channel.difficulty },
            { "hashrate", channel.hashrate },
            { "accepted", channel.accepted },
            { "rejected", channel.rejected },
            { "rejections", std::move(rejections) },
            { "last_share", std::move(last) },
            { "shares", std::move(shares) },
            { "history", std::move(history) }
        });
    }

    system::json::array blocks{};
    for (const auto& block: registry.get_blocks())
        blocks.push_back(system::json::object
        {
            { "hash", encode_hash(block.hash) },
            { "height", block.height },
            { "time", block.time },
            { "channel", block.channel }
        });

    return system::json::object
    {
        { "channels", std::move(channels) },
        { "blocks", std::move(blocks) }
    };
}

// zulu time in milliseconds since epoch (native js time format).
uint64_t protocol_admin::to_zulu(const logger::time& point) NOEXCEPT
{
    using namespace std::chrono;
    const auto now = logger::now();
    const auto delta = duration_cast<system_clock::duration>(now - point);
    const auto time = (system_clock::now() - delta).time_since_epoch();
    return duration_cast<milliseconds>(time).count();
}

// Unsubscribe if false.
bool protocol_admin::get_filter(bool& filtered, size_t bit,
    filter_t& filter) NOEXCEPT
{
    auto state = filter.load(relaxed);
    const auto next = set_left(state, zero, false);

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
    const auto next = set_left(value, zero, true);
    const auto state = is_zero(value) ? filter.fetch_and(next, relaxed) :
        filter.exchange(next, relaxed);

    prior = set_left(state, zero, false);
    return !is_zero(value) && !get_left(state);
}

} // namespace server
} // namespace libbitcoin
