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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_STRATUM_V1_HPP

#include <algorithm>
#include <map>
#include <unordered_set>
#include <bitcoin/server/channels/channel_rpc.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for stratum v1 service (universal json-rpc), carrying the state of
/// its miner's subscription and the jobs issued to it.
class BCS_API channel_stratum_v1
  : public channel_rpc,
    protected network::tracker<channel_stratum_v1>
{
public:
    typedef std::shared_ptr<channel_stratum_v1> ptr;
    using options_t = settings::stratum_v1_server;

    /// Jobs retained for share submission, the oldest dropped first.
    static constexpr size_t maximum_jobs = 16;

    /// A job issued to the miner, of a held template.
    struct job
    {
        node::block_template::cptr held{};
        system::data_chunk prefix{};
        system::data_chunk suffix{};
        system::hash_digest previous{};
        uint32_t version{};
        uint32_t bits{};
        uint32_t timestamp{};
        double difficulty{};
        std::unordered_set<system::hash_digest> shares{};
    };

    inline channel_stratum_v1(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : channel_rpc(log, socket, identifier, config, options),
        network::tracker<channel_stratum_v1>(log),
        options_(options),
        extranonce1_(to_extranonce(identifier, options.extranonce1_size)),
        difficulty_(options.minimum_difficulty),
        window_(network::steady_clock::now())
    {
    }

    /// Properties (require strand).
    /// -----------------------------------------------------------------------

    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

    inline const system::data_chunk& extranonce1() const NOEXCEPT
    {
        return extranonce1_;
    }

    inline void set_subscribed() NOEXCEPT
    {
        subscribed_ = true;
    }

    inline bool subscribed() const NOEXCEPT
    {
        return subscribed_;
    }

    /// The payout of the channel is set by its first authorized worker.
    inline void authorize(const std::string& worker,
        const system::chain::script& payout) NOEXCEPT
    {
        if (workers_.empty())
            payout_ = payout;

        workers_.insert(worker);
    }

    inline bool has_payout() const NOEXCEPT
    {
        return !workers_.empty();
    }

    inline bool authorized_worker(const std::string& worker) const NOEXCEPT
    {
        return workers_.contains(worker);
    }

    inline const system::chain::script& payout() const NOEXCEPT
    {
        return payout_;
    }

    inline void set_mask(uint32_t mask) NOEXCEPT
    {
        mask_ = mask;
    }

    inline uint32_t mask() const NOEXCEPT
    {
        return mask_;
    }

    inline void set_difficulty(double difficulty) NOEXCEPT
    {
        difficulty_ = difficulty;
    }

    inline double difficulty() const NOEXCEPT
    {
        return difficulty_;
    }

    inline void set_notified(double difficulty) NOEXCEPT
    {
        notified_ = difficulty;
    }

    inline double notified() const NOEXCEPT
    {
        return notified_;
    }

    /// Jobs (require strand).
    /// -----------------------------------------------------------------------

    /// Retain the job, all others dropped if clean, returning its identifier.
    inline uint32_t issue(job&& job, bool clean) NOEXCEPT
    {
        if (clean)
            jobs_.clear();

        if (jobs_.size() >= maximum_jobs)
            jobs_.erase(jobs_.begin());

        const auto id = next_++;
        jobs_.emplace(id, std::move(job));
        return id;
    }

    inline job* find(uint32_t id) NOEXCEPT
    {
        const auto it = jobs_.find(id);
        return it == jobs_.end() ? nullptr : &it->second;
    }

    /// The most recently issued job, null if none.
    inline const job* current() const NOEXCEPT
    {
        return jobs_.empty() ? nullptr : &jobs_.rbegin()->second;
    }

    /// Shares (require strand).
    /// -----------------------------------------------------------------------

    /// Count an accepted share within the retarget window.
    inline size_t count_share() NOEXCEPT
    {
        return ++shares_;
    }

    /// Seconds since the retarget window opened.
    inline double elapsed() const NOEXCEPT
    {
        using seconds = std::chrono::duration<double>;
        return seconds{ network::steady_clock::now() - window_ }.count();
    }

    inline void reset_window() NOEXCEPT
    {
        shares_ = zero;
        window_ = network::steady_clock::now();
    }

protected:
    /// Stratum errors are positional ([code, message, data]).
    inline network::rpc::error_form form() const NOEXCEPT override
    {
        return network::rpc::error_form::array;
    }

private:
    // Extranonce of the channel identifier, little endian, zero padded.
    static inline system::data_chunk to_extranonce(uint64_t identifier,
        uint8_t size) NOEXCEPT
    {
        system::data_chunk out(size, 0x00);
        const auto bytes = system::to_little_endian(identifier);
        const auto count = std::min(out.size(), bytes.size());
        std::copy_n(bytes.begin(), count, out.begin());
        return out;
    }

    // This is thread safe.
    const options_t& options_;

    // These are protected by strand.
    const system::data_chunk extranonce1_;
    std::unordered_set<std::string> workers_{};
    system::chain::script payout_{};
    std::map<uint32_t, job> jobs_{};
    network::steady_clock::time_point window_;
    double difficulty_;
    double notified_{};
    size_t shares_{};
    uint32_t next_{};
    uint32_t mask_{};
    bool subscribed_{};
};

} // namespace server
} // namespace libbitcoin

#endif
