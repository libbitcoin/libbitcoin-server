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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_MINING_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_MINING_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_submit.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

/// Outcome of a submitted share.
enum class share_outcome
{
    /// Valid work, at or below the share target.
    accepted,

    /// Accepted before.
    duplicate,

    /// Above the share target.
    low_difficulty,

    /// Not a valid header, by the code.
    invalid
};

/// Mining, common shape of protocols that issue jobs of held templates to
/// mining channels, check their shares and submit the blocks they mine.
template <typename Base>
class protocol_mining
  : public protocol_submit<Base>
{
public:
    void stopping(const code& ec) NOEXCEPT override;

protected:
    using base = protocol_submit<Base>;
    using base::log;
    using job = mining_channel::job;

    inline protocol_mining(const auto& session,
        const network::channel::ptr& channel,
        const settings::mining_server& options) NOEXCEPT
      : base(session, channel, options),
        options_(options),
        decay_timer_(system::emplace_shared<network::deadline>(session->log,
            channel->strand()))
    {
    }

    /// Bytes of coinbase extranonce, of the channel and of the miner.
    virtual size_t extranonce_size() const NOEXCEPT = 0;

    /// Issue the job to the channel, notifying the miner.
    virtual void send_job(mining_channel& channel, const job::cptr& job,
        bool clean) NOEXCEPT = 0;

    /// Add or remove a mining channel of the connection, registering its
    /// entry (service, transport, client and workers) (requires strand).
    void add_channel(uint32_t id, const mining_channel::ptr& channel,
        miners::channel&& entry) NOEXCEPT;
    void remove_channel(uint32_t id) NOEXCEPT;

    /// The mining channel of the identifier, empty if none (requires strand).
    mining_channel::ptr find_channel(uint32_t id) const NOEXCEPT;

    /// The mining channels of the connection (requires strand).
    size_t mining_channel_count() const NOEXCEPT;

    /// The difficulty of a new channel, of the hash rate if automatic.
    double start_difficulty(double hash_rate) const NOEXCEPT;

    /// Demand templates and issue jobs to new channels (requires strand).
    void start_jobs() NOEXCEPT;

    /// Check the share of the job, with the extranonce rolled by the miner,
    /// setting its header (requires strand).
    share_outcome check_share(mining_channel& channel, const job& job,
        uint32_t version, uint32_t timestamp, uint32_t nonce,
        const system::data_chunk& rolled, system::chain::header& out,
        code& ec) NOEXCEPT;

    /// Record a share refused before it is checked (requires strand).
    void reject_share(const mining_channel& channel,
        const std::string& result) NOEXCEPT;

    /// Submit the block of an accepted share if it mines one, and retarget
    /// the channel (requires strand).
    void accept_share(mining_channel& channel, const job& job,
        const system::chain::header& header,
        const system::data_chunk& rolled) NOEXCEPT;

    /// Set the difficulty of the channel, at or above its floor.
    void suggest(mining_channel& channel, double difficulty) NOEXCEPT;

    /// Issue the current job again at channel difficulty and a later time.
    void refresh(mining_channel& channel) NOEXCEPT;

    /// Chase events (template replacement and submitted block completion).
    bool handle_chase(const code& ec,
        node::event_value value) NOEXCEPT override;

    /// The outcome of a block mined by a channel.
    void handle_submitted(const system::hash_digest& hash,
        submission outcome, const code& ec) NOEXCEPT override;

private:
    // Shares counted (or the share interval multiplied) before a retarget.
    static constexpr size_t retarget_shares = 16;

    // Share intervals without a share before the difficulty decays.
    static constexpr size_t decay_intervals = 4;

    // Retarget bounds on the ratio of target to observed share interval.
    static constexpr double minimum_ratio = 0.25;
    static constexpr double maximum_ratio = 4.0;
    static constexpr double lower_dead_band = 0.8;
    static constexpr double upper_dead_band = 1.25;

    void handle_template(const code& ec,
        const node::block_template::cptr& held) NOEXCEPT;
    void do_template(const code& ec,
        const node::block_template::cptr& held) NOEXCEPT;
    double floor(const mining_channel& channel) const NOEXCEPT;
    void retarget(mining_channel& channel) NOEXCEPT;
    void start_decay() NOEXCEPT;
    void handle_decay(const code& ec) NOEXCEPT;
    void submit_mined(const mining_channel& channel,
        const system::chain::header& header, const job& job,
        const system::data_chunk& extranonce) NOEXCEPT;
    share_outcome to_outcome(mining_channel& channel, const job& job,
        uint32_t version, uint32_t timestamp, uint32_t nonce,
        const system::data_chunk& rolled, system::chain::header& out,
        code& ec) NOEXCEPT;
    void record_share(const mining_channel& channel, const job& job,
        const system::chain::header& header, share_outcome outcome) NOEXCEPT;

    // These are thread safe.
    const settings::mining_server& options_;
    const network::deadline::ptr decay_timer_;
    std::atomic_bool active_{};

    // These are protected by strand.
    std::unordered_map<uint32_t, mining_channel::ptr> channels_{};
    std::unordered_map<system::hash_digest,
        std::pair<size_t, std::string>> mined_{};
};

} // namespace server
} // namespace libbitcoin

#include <bitcoin/server/impl/protocols/protocol_mining.ipp>

#endif
