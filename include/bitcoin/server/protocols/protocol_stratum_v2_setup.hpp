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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V2_SETUP_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V2_SETUP_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

/// Stratum v2 connection setup (handshake, attached first).
class BCS_API protocol_stratum_v2_setup
  : public server::protocol,
    public network::protocol,
    protected network::tracker<protocol_stratum_v2_setup>
{
public:
    typedef std::shared_ptr<protocol_stratum_v2_setup> ptr;
    using channel_t = channel_stratum_v2;
    using options_t = channel_t::options_t;
    using setup_connection = network::messages::sv2::setup_connection;

    /// The protocol version served.
    static constexpr uint16_t version{ 2 };

    /// The mining subprotocol, and the setup flags of its clients.
    static constexpr uint8_t mining{ 0 };
    static constexpr uint32_t requires_standard_jobs{ 0x00000001 };
    static constexpr uint32_t requires_work_selection{ 0x00000002 };
    static constexpr uint32_t requires_version_rolling{ 0x00000004 };

    /// The client flags served.
    static constexpr uint32_t supported
    {
        requires_standard_jobs | requires_version_rolling
    };

    inline protocol_stratum_v2_setup(const auto& session,
        const network::channel::ptr& channel, const options_t&) NOEXCEPT
      : server::protocol(session, channel),
        network::protocol(session, channel),
        network::tracker<protocol_stratum_v2_setup>(session->log),
        channel_(std::dynamic_pointer_cast<channel_t>(channel))
    {
    }

    /// Start the handshake, handler invoked once at its completion.
    virtual void shake(network::result_handler&& handler) NOEXCEPT;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    DECLARE_SEND()
    DECLARE_SUBSCRIBE_CHANNEL()

    virtual bool handle_receive_setup_connection(const code& ec,
        const setup_connection::cptr& message) NOEXCEPT;
    virtual void finished(const code& ec, const code& shake) NOEXCEPT;

private:
    void refuse(uint32_t flags, const std::string& reason) NOEXCEPT;
    void callback(const code& ec) NOEXCEPT;

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;

    // This is protected by strand.
    std::shared_ptr<network::result_handler> handler_{};
};

} // namespace server
} // namespace libbitcoin

#endif
