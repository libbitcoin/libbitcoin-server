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
#include <bitcoin/server/protocols/protocol_electrum_version.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum_version
    
using namespace network;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/finished (handshake).
// ----------------------------------------------------------------------------

// Session resumes the channel following return from start().
// Sends are not precluded, but no messages can be received while paused.
void protocol_electrum_version::shake(result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
    {
        handler(network::error::operation_failed);
        return;
    }

    handler_ = system::move_shared<result_handler>(std::move(handler));

    SUBSCRIBE_RPC(handle_server_version, _1, _2, _3, _4);
    protocol_rpc<interface::electrum_handshake>::start();
}

void protocol_electrum_version::finished(const code& ec,
    const code& shake) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Calls after handshake completion are allowed and will skip this.
    if (handler_)
    {
        // Invoke handshake completion, error will result in stopped channel.
        (*handler_)(shake);
        handler_.reset();
    }
}

// Handler.
// ----------------------------------------------------------------------------

// A non-version opener is a pre-1.6 client. The handshake passes at the
// minimum version, restricted until (unless) a server.version arrives.
void protocol_electrum_version::handle_unclaimed(
    const request_t&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!handler_)
        return;

    pause();
    channel_->set_restricted();
    channel_->set_version(electrum::version_floor(options().protocol_minimum));
    finished(error::success, error::success);
}

void protocol_electrum_version::handle_server_version(const code& ec,
    rpc_interface::server_version, const std::string& client_name,
    const value_t& protocol_version) NOEXCEPT
{
    if (stopped(ec))
        return;

    // Handshake must leave channel paused, no more receives after this one.
    if (handler_)
        pause();

    // Only the first server.version is accepted from 1.4 (channel retained).
    if (channel_->negotiated() &&
        channel_->version() >= electrum::version::v1_4)
    {
        send_code(error::electrum::bad_request,
            BIND(finished, _1, error::success));
        return;
    }

    if (!set_client(client_name) || !set_version(protocol_version))
    {
        const auto reason = error::electrum::bad_request;
        if (handler_)
            send_code(reason, BIND(finished, _1, reason));
        else
            stop(reason);

        return;
    }

    send_result(electrum::version_result(channel_->version(),
        options().server_name), 70, BIND(finished, _1, error::success));
}

// Client/server names.
// ----------------------------------------------------------------------------

std::string_view protocol_electrum_version::client_name() const NOEXCEPT
{
    return channel_->client();
}

bool protocol_electrum_version::set_client(const std::string& name) NOEXCEPT
{
    // Do not put to log without escaping.
    channel_->set_client(electrum::escape_client(
        name.substr(zero, electrum::maximum_client_name)));
    return true;
}

// Negotiated version.
// ----------------------------------------------------------------------------

bool protocol_electrum_version::set_version(const value_t& version) NOEXCEPT
{
    // A non-version opener restricts negotiation to below 1.6.
    const auto minimum = options().protocol_minimum;
    const auto maximum = channel_->restricted() ?
        std::min(options().protocol_maximum,
            electrum::version_to_number(electrum::restricted_maximum)) :
        options().protocol_maximum;

    // Below 1.4 a repeat must agree with the negotiated version.
    const auto floor = electrum::negotiate(version, minimum, maximum);

    if (floor == electrum::version::v0_0 ||
        (channel_->negotiated() && floor != channel_->version()))
        return false;

    LOGA("Electrum [" << opposite() << "] version ("
        << electrum::version_to_string(floor) << ") " << client_name());

    channel_->set_version(floor);
    channel_->set_negotiated();
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
