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

#include <atomic>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>
#include <bitcoin/server/protocols/protocol_submit.hpp>

namespace libbitcoin {
namespace server {

/// Stratum v1 solo mining, a job per held template paying the channel's
/// worker, shares checked against the channel difficulty and blocks submitted.
class BCS_API protocol_stratum_v1
  : public protocol_submit<protocol_rpc<interface::stratum_v1>>,
    protected network::tracker<protocol_stratum_v1>
{
public:
    typedef std::shared_ptr<protocol_stratum_v1> ptr;
    using rpc_interface = interface::stratum_v1;
    using channel_t = channel_stratum_v1;
    using options_t = channel_t::options_t;
    using submit_t = protocol_submit<protocol_rpc<rpc_interface>>;

    inline protocol_stratum_v1(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : submit_t(session, channel, options),
        network::tracker<protocol_stratum_v1>(session->log),
        options_(options),
        p2kh_(session->server_settings().wallet.p2kh_prefix),
        p2sh_(session->server_settings().wallet.p2sh_prefix),
        witness_(session->server_settings().wallet.witness_prefix),
        channel_(std::dynamic_pointer_cast<channel_t>(channel))
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Terminal responder (attached last) for unclaimed methods.
    void handle_unclaimed(
        const network::rpc::request_t& request) NOEXCEPT override;

    /// Chase events (template replacement and submitted block completion).
    bool handle_chase(const code& ec,
        node::event_value value) NOEXCEPT override;

    /// The outcome of a block mined by the channel.
    void handle_submitted(submission outcome,
        const code& ec) NOEXCEPT override;

    /// Handlers.
    void handle_mining_subscribe(const code& ec,
        rpc_interface::mining_subscribe, const std::string& user_agent,
        const std::string& session_id, const std::string& host,
        const network::rpc::value_t& port) NOEXCEPT;
    void handle_mining_authorize(const code& ec,
        rpc_interface::mining_authorize, const std::string& username,
        const std::string& password) NOEXCEPT;
    void handle_mining_submit(const code& ec,
        rpc_interface::mining_submit, const std::string& worker_name,
        const std::string& job_id, const std::string& extranonce2,
        const std::string& ntime, const std::string& nonce,
        const std::string& version_bits) NOEXCEPT;
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
    void handle_mining_get_transactions(const code& ec,
        rpc_interface::mining_get_transactions,
        const std::string& job_id) NOEXCEPT;
    void handle_mining_multi_version(const code& ec,
        rpc_interface::mining_multi_version, double version) NOEXCEPT;
    void handle_mining_extranonce_subscribe(const code& ec,
        rpc_interface::mining_extranonce_subscribe) NOEXCEPT;

private:
    using job = channel_t::job;

    /// Notifications require a full duplex transport.
    inline bool is_duplex() const NOEXCEPT
    {
        return channel_->websocket() || channel_->downgraded();
    }

    /// Jobs.
    void start_jobs() NOEXCEPT;
    void handle_template(const code& ec,
        const node::block_template::cptr& held) NOEXCEPT;
    void do_template(const code& ec,
        const node::block_template::cptr& held) NOEXCEPT;
    void send_job(job&& job, bool clean) NOEXCEPT;
    void refresh() NOEXCEPT;
    void retarget() NOEXCEPT;
    void suggest(double difficulty) NOEXCEPT;

    /// Blocks.
    void submit_mined(const system::chain::header& header, const job& job,
        const system::data_chunk& extranonce) NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    const std::string witness_;
    const channel_t::ptr channel_;
    std::atomic_bool active_{};
};

} // namespace server
} // namespace libbitcoin

#endif
