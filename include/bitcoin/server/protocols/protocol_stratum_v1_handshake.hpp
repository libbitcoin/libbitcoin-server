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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HANDSHAKE_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HANDSHAKE_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

/// Stratum v1 handshake (attached first), subscription and authorization of
/// the miner, completed by its first authorized worker of a subscription.
class BCS_API protocol_stratum_v1_handshake
  : public protocol_rpc<interface::stratum_v1_handshake>,
    protected network::tracker<protocol_stratum_v1_handshake>
{
public:
    typedef std::shared_ptr<protocol_stratum_v1_handshake> ptr;
    using rpc_interface = interface::stratum_v1_handshake;
    using channel_t = channel_stratum_v1;
    using options_t = channel_t::options_t;

    inline protocol_stratum_v1_handshake(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<rpc_interface>(session, channel, options),
        network::tracker<protocol_stratum_v1_handshake>(session->log),
        options_(options),
        p2kh_(session->server_settings().wallet.p2kh_prefix),
        p2sh_(session->server_settings().wallet.p2sh_prefix),
        witness_(session->server_settings().wallet.witness_prefix),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        mining_(to_mining(session->allocate_prefix(options.extranonce1_size),
            options.initial_difficulty(0.0))),
        authorize_timer_(system::emplace_shared<network::deadline>(
            session->log, channel->strand(),
            network::seconds(options.authorize_timeout_seconds)))
    {
    }

    /// Start the handshake, handler invoked once at its completion.
    virtual void shake(network::result_handler&& handler) NOEXCEPT;
    virtual void finished(const code& ec, const code& shake) NOEXCEPT;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Difficulty suggestions are claimed until completion.
    bool claims(const std::string& method) const NOEXCEPT override;

    /// The handshake authorizes the channel by mining.authorize.
    bool in_band() const NOEXCEPT override;

    /// Before completion, answers the methods of mining (not yet attached).
    void handle_unclaimed(
        const network::rpc::request_t& request) NOEXCEPT override;

    /// Handlers.
    void handle_mining_subscribe(const code& ec,
        rpc_interface::mining_subscribe, const std::string& user_agent,
        const std::string& session_id, const std::string& host,
        const network::rpc::value_t& port) NOEXCEPT;
    void handle_mining_authorize(const code& ec,
        rpc_interface::mining_authorize, const std::string& username,
        const std::string& password) NOEXCEPT;
    void handle_mining_configure(const code& ec,
        rpc_interface::mining_configure,
        const interface::array_t& extensions,
        const interface::object_t& parameters) NOEXCEPT;
    void handle_mining_suggest_difficulty(const code& ec,
        rpc_interface::mining_suggest_difficulty,
        double difficulty) NOEXCEPT;
    void handle_mining_suggest_target(const code& ec,
        rpc_interface::mining_suggest_target,
        const std::string& target) NOEXCEPT;
    void handle_mining_extranonce_subscribe(const code& ec,
        rpc_interface::mining_extranonce_subscribe) NOEXCEPT;

private:
    static mining_channel::ptr to_mining(const extranonces::prefix& prefix,
        double difficulty) NOEXCEPT;

    void callback(const code& ec) NOEXCEPT;
    void complete() NOEXCEPT;
    void handle_timeout(const code& ec) NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    const std::string witness_;
    const channel_t::ptr channel_;
    const mining_channel::ptr mining_;
    const network::deadline::ptr authorize_timer_;

    // This is protected by strand.
    std::shared_ptr<network::result_handler> handler_{};
};

} // namespace server
} // namespace libbitcoin

#endif
