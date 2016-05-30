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
#include <bitcoin/server/endpoints/heart_endpoint.hpp>

#include <cstdint>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define PUBLIC_NAME "public_heartbeat"
#define SECURE_NAME "secure_heartbeat"

using std::placeholders::_1;
using namespace bc::protocol;

static inline bool is_enabled(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return settings.heartbeat_endpoints_enabled &&
        (!secure || settings.server_private_key);
}

static inline config::endpoint get_endpoint(server_node& node, bool secure)
{
    const auto& settings = node.server_settings();
    return secure ? settings.secure_heartbeat_endpoint :
        settings.public_heartbeat_endpoint;
}

static inline deadline::ptr deadline_factory(server_node& node)
{
    auto& pool = node.thread_pool();
    const auto& settings = node.server_settings();
    return std::make_shared<deadline>(pool, settings.heartbeat_interval());
}

heart_endpoint::heart_endpoint(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : counter_(rand()),
    socket_(authenticator, zmq::socket::role::publisher),
    deadline_(deadline_factory(node)),
    endpoint_(get_endpoint(node, secure)),
    enabled_(is_enabled(node, secure)),
    secure_(secure)
{
    const auto name = secure ? SECURE_NAME : PUBLIC_NAME;

    // The authenticator logs apply failures and stopped socket halts start.
    if (!enabled_ || !authenticator.apply(socket_, name, secure))
        socket_.stop();
}

// The endpoint is not restartable.
bool heart_endpoint::start()
{
    if (!enabled_)
        return true;

    if (!socket_)
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize heartbeat service.";
        return false;
    }

    if (!socket_.bind(endpoint_))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to bind heartbeat service to " << endpoint_;
        stop();
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound " << (secure_ ? "secure " : "public ")
        << "heartbeat service to " << endpoint_;

    start_timer();
    return true;
}

bool heart_endpoint::stop()
{
    deadline_->stop();
    const auto result = socket_.stop();

    log::debug(LOG_ENDPOINT)
        << "Unbound " << (secure_ ? "secure " : "public ")
        << "heartbeat service to " << endpoint_;

    return result;
}

void heart_endpoint::start_timer()
{
    deadline_->start(
        std::bind(&heart_endpoint::send,
            this, _1));
}

void heart_endpoint::send(const code& ec)
{
    if (ec)
    {
        log::error(LOG_ENDPOINT)
            << "Heartbeat timer failed: " << ec.message();
        return;
    }

    zmq::message message;
    message.enqueue_little_endian(counter_);
    const auto result = message.send(socket_);

    if (result)
        log::debug(LOG_ENDPOINT)
            << "Heartbeat sent: " << counter_;
    else
        log::debug(LOG_ENDPOINT)
            << "Heartbeat failed to send.";

    ++counter_;
    start_timer();
}

} // namespace server
} // namespace libbitcoin
