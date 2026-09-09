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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SPARROW_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SPARROW_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// The sparrow interface, served in addition to electrum on the same channel.
/// Attached after the version handshake and before protocol_electrum, which
/// remains the terminal responder, so these three methods are claimed here
/// and everything else falls through to electrum unchanged.
class BCS_API protocol_sparrow
  : public protocol_rpc<interface::sparrow>,
    protected network::tracker<protocol_sparrow>
{
public:
    typedef std::shared_ptr<protocol_sparrow> ptr;
    using rpc_interface = interface::sparrow;
    using channel_t = channel_electrum;
    using options_t = settings::sparrow_server;

    inline protocol_sparrow(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<interface::sparrow>(session, channel, options),
        options_(options),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        network::tracker<protocol_sparrow>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    void handle_blockchain_block_stats(const code& ec,
        rpc_interface::blockchain_block_stats, double height) NOEXCEPT;
    void handle_blockchain_silent_payments_subscribe(const code& ec,
        rpc_interface::blockchain_silent_payments_subscribe,
        const std::string& scan_private_key,
        const std::string& spend_public_key,
        const interface::value_t& start,
        const interface::array_t& labels) NOEXCEPT;
    void handle_blockchain_silent_payments_unsubscribe(const code& ec,
        rpc_interface::blockchain_silent_payments_unsubscribe,
        const std::string& scan_private_key,
        const std::string& spend_public_key) NOEXCEPT;

    /// The negotiated electrum version is at least the specified level.
    inline bool at_least(server::electrum::version version) const NOEXCEPT
    {
        return channel_->version() >= version;
    }

    /// Configuration options.
    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

private:
    // This is thread safe.
    const options_t& options_;

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;
};

} // namespace server
} // namespace libbitcoin

#endif
