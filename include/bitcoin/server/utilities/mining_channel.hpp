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
#ifndef LIBBITCOIN_SERVER_UTILITIES_MINING_CHANNEL_HPP
#define LIBBITCOIN_SERVER_UTILITIES_MINING_CHANNEL_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/extranonces.hpp>

namespace libbitcoin {
namespace server {

/// A mining channel, its extranonce prefix, payout, difficulty, the jobs
/// issued to it and the shares it has submitted. Not thread safe.
class BCS_API mining_channel
{
public:
    typedef std::shared_ptr<mining_channel> ptr;

    /// Jobs retained for share submission, the oldest dropped first.
    static constexpr size_t maximum_jobs = 16;

    /// A job issued to the channel, of a held template.
    struct job
    {
        typedef std::shared_ptr<const job> cptr;

        node::block_template::cptr held{};
        system::data_chunk prefix{};
        system::data_chunk suffix{};
        system::hash_digest previous{};
        uint32_t version{};
        uint32_t bits{};
        uint32_t timestamp{};
        double difficulty{};
    };

    mining_channel(const extranonces::prefix& prefix,
        double difficulty) NOEXCEPT;

    /// The coinbase extranonce prefix of the channel.
    const system::data_chunk& extranonce() const NOEXCEPT;

    /// The miners registry key of the channel.
    const std::string& key() const NOEXCEPT;
    void set_key(const std::string& key) NOEXCEPT;

    /// The script paid by the coinbase of the channel's jobs.
    const system::chain::script& payout() const NOEXCEPT;
    void set_payout(const system::chain::script& payout) NOEXCEPT;

    /// The share difficulty of jobs issued to the channel.
    double difficulty() const NOEXCEPT;
    void set_difficulty(double difficulty) NOEXCEPT;

    /// The difficulty floor of the channel, requested by the miner.
    double floor() const NOEXCEPT;
    void set_floor(double difficulty) NOEXCEPT;

    /// Set the difficulty, at or above the floor and the minimum, and reset
    /// the retarget window, false if unchanged.
    bool suggest(double difficulty, double minimum) NOEXCEPT;

    /// The difficulty last notified to the miner.
    double notified() const NOEXCEPT;
    void set_notified(double difficulty) NOEXCEPT;

    /// Retain the job, all others and their shares dropped if clean,
    /// returning its identifier.
    uint32_t issue(const job::cptr& job, bool clean) NOEXCEPT;

    /// The job of the identifier, empty if not retained.
    job::cptr find(uint32_t id) const NOEXCEPT;

    /// The identifier was issued, whether or not retained.
    bool issued(uint32_t id) const NOEXCEPT;

    /// The most recently issued job, empty if none.
    job::cptr current() const NOEXCEPT;

    /// Accept a share by its header hash, false if accepted before.
    bool accept(const system::hash_digest& hash) NOEXCEPT;

    /// Shares accepted by the channel.
    uint64_t accepted() const NOEXCEPT;

    /// Shares accepted within the retarget window.
    size_t shares() const NOEXCEPT;

    /// Seconds since the retarget window opened.
    double elapsed() const NOEXCEPT;
    void reset_window() NOEXCEPT;

private:
    using entry = std::pair<uint32_t, job::cptr>;

    const extranonces::prefix prefix_;
    std::string key_{};
    system::chain::script payout_{};
    std::deque<entry> jobs_{};
    std::unordered_set<system::hash_digest> hashes_{};
    network::steady_clock::time_point window_;
    double difficulty_;
    double floor_{};
    double notified_{};
    uint64_t accepted_{};
    size_t shares_{};
    uint32_t next_{};
};

} // namespace server
} // namespace libbitcoin

#endif
