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
#include <bitcoin/server/protocols/protocol_stratum_v2_setup.hpp>

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_stratum_v2_setup

using namespace system;
using namespace network::messages::sv2;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/finished (handshake).
// ----------------------------------------------------------------------------

// Session resumes channel following return from start().
void protocol_stratum_v2_setup::shake(
    network::result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
    {
        handler(network::error::operation_failed);
        return;
    }

    handler_ = move_shared<network::result_handler>(std::move(handler));

    SUBSCRIBE_CHANNEL(setup_connection, handle_receive_setup_connection, _1, _2);
    network::protocol::start();
}

void protocol_stratum_v2_setup::finished(const code& ec,
    const code& shake) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    callback(shake);
}

// Allow channel stop to terminate handshake.
void protocol_stratum_v2_setup::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    callback(ec);
    network::protocol::stopping(ec);
}

// private
// Idempotent on strand, first caller gets handler.
void protocol_stratum_v2_setup::callback(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!handler_)
        return;

    // Invoke handshake completion, error will result in stopped channel.
    (*handler_)(ec);
    handler_.reset();
}

// Handler.
// ----------------------------------------------------------------------------

// private
void protocol_stratum_v2_setup::refuse(uint32_t flags,
    const std::string& reason) NOEXCEPT
{
    BC_ASSERT(stranded());

    const setup_connection_error error{ flags, reason };
    SEND(error, finished, _1, network::error::peer_unsupported);
}

bool protocol_stratum_v2_setup::handle_receive_setup_connection(
    const code& ec, const setup_connection::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Handshake must leave channel paused, no more receives after this one.
    pause();

    if (message->protocol != mining)
    {
        refuse({}, "unsupported-protocol");
        return false;
    }

    if (message->min_version > version || message->max_version < version)
    {
        refuse({}, "protocol-version-mismatch");
        return false;
    }

    const auto unsupported = bit_and(message->flags, bit_not(supported));
    if (!is_zero(unsupported))
    {
        refuse(unsupported, "unsupported-feature-flags");
        return false;
    }

    channel_->set_handshaked();
    const setup_connection_success success{ version, {} };
    SEND(success, finished, _1, network::error::success);
    return false;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef CLASS

} // namespace server
} // namespace libbitcoin
