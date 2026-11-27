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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>
#include <bitcoin/server/protocols/protocol_mining.hpp>

namespace libbitcoin {
namespace server {

/// Stratum v1 solo mining (attached at completion of the handshake), a job per
/// held template paying the channel's worker, shares checked against the
/// channel difficulty and blocks submitted.
class BCS_API protocol_stratum_v1
  : public protocol_mining<protocol_rpc<interface::stratum_v1>>,
    protected network::tracker<protocol_stratum_v1>
{
public:
    typedef std::shared_ptr<protocol_stratum_v1> ptr;
    using rpc_interface = interface::stratum_v1;
    using channel_t = channel_stratum_v1;
    using options_t = channel_t::options_t;
    using mining_t = protocol_mining<protocol_rpc<rpc_interface>>;

    inline protocol_stratum_v1(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : mining_t(session, channel, options),
        network::tracker<protocol_stratum_v1>(session->log),
        options_(options),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        mining_(channel_->mining())
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Terminal responder (attached last) for unclaimed methods.
    void handle_unclaimed(
        const network::rpc::request_t& request) NOEXCEPT override;

    /// Extranonce of the channel and the miner.
    size_t extranonce_size() const NOEXCEPT override;

    /// Notify the job, and its difficulty if changed.
    void send_job(mining_channel& channel, const job::cptr& job,
        bool clean) NOEXCEPT override;

    /// Handlers.
    void handle_mining_submit(const code& ec,
        rpc_interface::mining_submit, const std::string& worker_name,
        const std::string& job_id, const std::string& extranonce2,
        const std::string& ntime, const std::string& nonce,
        const std::string& version_bits) NOEXCEPT;
    void handle_mining_suggest_difficulty(const code& ec,
        rpc_interface::mining_suggest_difficulty,
        double difficulty) NOEXCEPT;
    void handle_mining_suggest_target(const code& ec,
        rpc_interface::mining_suggest_target,
        const std::string& target) NOEXCEPT;
    void handle_mining_get_transactions(const code& ec,
        rpc_interface::mining_get_transactions,
        const std::string& job_id) NOEXCEPT;
    void handle_mining_multi_version(const code& ec,
        rpc_interface::mining_multi_version, double version) NOEXCEPT;

private:
    // Notifications require a full duplex transport.
    inline bool is_duplex() const NOEXCEPT
    {
        return channel_->websocket() || channel_->downgraded();
    }

    // These are thread safe.
    const options_t& options_;
    const channel_t::ptr channel_;
    const mining_channel::ptr mining_;
};

} // namespace server
} // namespace libbitcoin

#endif
