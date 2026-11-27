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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V2_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V2_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_mining.hpp>
#include <bitcoin/server/protocols/protocol_sv2.hpp>

namespace libbitcoin {
namespace server {

/// Stratum v2 solo mining, a job per held template paying the channel's
/// user, shares checked against the channel target and blocks submitted.
class BCS_API protocol_stratum_v2
  : public protocol_mining<protocol_sv2>,
    protected network::tracker<protocol_stratum_v2>
{
public:
    typedef std::shared_ptr<protocol_stratum_v2> ptr;
    using channel_t = channel_stratum_v2;
    using options_t = channel_t::options_t;
    using mining_t = protocol_mining<protocol_sv2>;
    using open_standard_mining_channel =
        network::messages::sv2::open_standard_mining_channel;
    using open_extended_mining_channel =
        network::messages::sv2::open_extended_mining_channel;
    using update_channel = network::messages::sv2::update_channel;
    using close_channel = network::messages::sv2::close_channel;
    using submit_shares_standard =
        network::messages::sv2::submit_shares_standard;
    using submit_shares_extended =
        network::messages::sv2::submit_shares_extended;

    inline protocol_stratum_v2(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : mining_t(session, channel, options),
        network::tracker<protocol_stratum_v2>(session->log),
        options_(options),
        p2kh_(session->server_settings().wallet.p2kh_prefix),
        p2sh_(session->server_settings().wallet.p2sh_prefix),
        witness_(session->server_settings().wallet.witness_prefix),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        authorize_timer_(system::emplace_shared<network::deadline>(
            session->log, channel->strand(),
            network::seconds(options.authorize_timeout_seconds)))
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    DECLARE_SEND()
    DECLARE_SUBSCRIBE_CHANNEL()

    /// Extranonce of the channel and the miner.
    size_t extranonce_size() const NOEXCEPT override;

    /// Send the job, and its target if changed.
    void send_job(mining_channel& channel, const job::cptr& job,
        bool clean) NOEXCEPT override;

    /// Handlers.
    virtual bool handle_receive_open_standard_mining_channel(const code& ec,
        const open_standard_mining_channel::cptr& message) NOEXCEPT;
    virtual bool handle_receive_open_extended_mining_channel(const code& ec,
        const open_extended_mining_channel::cptr& message) NOEXCEPT;
    virtual bool handle_receive_update_channel(const code& ec,
        const update_channel::cptr& message) NOEXCEPT;
    virtual bool handle_receive_close_channel(const code& ec,
        const close_channel::cptr& message) NOEXCEPT;
    virtual bool handle_receive_submit_shares_standard(const code& ec,
        const submit_shares_standard::cptr& message) NOEXCEPT;
    virtual bool handle_receive_submit_shares_extended(const code& ec,
        const submit_shares_extended::cptr& message) NOEXCEPT;

private:
    struct channel_state
      : public mining_channel
    {
        typedef std::shared_ptr<channel_state> ptr;

        inline channel_state(const extranonces::prefix& prefix,
            double difficulty, uint32_t id, bool standard) NOEXCEPT
          : mining_channel(prefix, difficulty), id(id), standard(standard)
        {
        }

        inline void set_max_target(const system::hash_digest& target) NOEXCEPT
        {
            max_target = target;
            set_floor(to_hash_difficulty(target));
        }

        const uint32_t id;
        const bool standard;
        system::hash_digest max_target{};
        system::hash_digest notified_target{};
    };

    // A share of a channel, the extranonce empty for a standard channel.
    struct share
    {
        uint32_t channel_id;
        uint32_t sequence_number;
        uint32_t job_id;
        uint32_t nonce;
        uint32_t ntime;
        uint32_t version;
        const system::data_chunk& extranonce;
    };

    void open_channel(uint32_t request_id, const std::string& identity,
        float hash_rate, const system::hash_digest& max_target,
        bool standard) NOEXCEPT;
    void open_failed(uint32_t request_id, const std::string& reason) NOEXCEPT;
    void submit_share(const share& share, bool standard) NOEXCEPT;
    void share_failed(const share& share, const std::string& reason) NOEXCEPT;
    void share_dropped(const share& share, const std::string& reason,
        const code& ec) NOEXCEPT;
    void handle_dropped(const code& ec, const code& reason) NOEXCEPT;
    bool to_payout(system::chain::script& out,
        const std::string& identity) const NOEXCEPT;
    channel_state::ptr find(uint32_t id) const NOEXCEPT;
    void handle_timeout(const code& ec) NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    const std::string witness_;
    const channel_t::ptr channel_;
    const network::deadline::ptr authorize_timer_;

    // This is protected by strand.
    uint32_t next_{};
};

} // namespace server
} // namespace libbitcoin

#endif
