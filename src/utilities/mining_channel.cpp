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
#include <bitcoin/server/utilities/mining_channel.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

mining_channel::mining_channel(const extranonces::prefix& prefix,
    double difficulty) NOEXCEPT
  : prefix_(prefix), window_(network::steady_clock::now()),
    difficulty_(difficulty)
{
    BC_ASSERT(prefix_);
}

const std::string& mining_channel::key() const NOEXCEPT
{
    return key_;
}

void mining_channel::set_key(const std::string& key) NOEXCEPT
{
    key_ = key;
}

const data_chunk& mining_channel::extranonce() const NOEXCEPT
{
    return *prefix_;
}

const chain::script& mining_channel::payout() const NOEXCEPT
{
    return payout_;
}

void mining_channel::set_payout(const chain::script& payout) NOEXCEPT
{
    payout_ = payout;
}

double mining_channel::difficulty() const NOEXCEPT
{
    return difficulty_;
}

void mining_channel::set_difficulty(double difficulty) NOEXCEPT
{
    difficulty_ = difficulty;
}

double mining_channel::floor() const NOEXCEPT
{
    return floor_;
}

void mining_channel::set_floor(double difficulty) NOEXCEPT
{
    floor_ = difficulty;
}

double mining_channel::notified() const NOEXCEPT
{
    return notified_;
}

void mining_channel::set_notified(double difficulty) NOEXCEPT
{
    notified_ = difficulty;
}

uint32_t mining_channel::issue(const job::cptr& job, bool clean) NOEXCEPT
{
    if (clean)
    {
        jobs_.clear();
        hashes_.clear();
    }

    if (jobs_.size() >= maximum_jobs)
        jobs_.pop_front();

    const auto id = next_++;
    jobs_.emplace_back(id, job);
    return id;
}

mining_channel::job::cptr mining_channel::find(uint32_t id) const NOEXCEPT
{
    const auto it = std::ranges::find(jobs_, id, &entry::first);
    return it == jobs_.end() ? job::cptr{} : it->second;
}

bool mining_channel::issued(uint32_t id) const NOEXCEPT
{
    return id < next_;
}

mining_channel::job::cptr mining_channel::current() const NOEXCEPT
{
    return jobs_.empty() ? job::cptr{} : jobs_.back().second;
}

bool mining_channel::accept(const hash_digest& hash) NOEXCEPT
{
    if (!hashes_.insert(hash).second)
        return false;

    ++accepted_;
    ++shares_;
    return true;
}

uint64_t mining_channel::accepted() const NOEXCEPT
{
    return accepted_;
}

size_t mining_channel::shares() const NOEXCEPT
{
    return shares_;
}

double mining_channel::elapsed() const NOEXCEPT
{
    using seconds = std::chrono::duration<double>;
    return seconds{ network::steady_clock::now() - window_ }.count();
}

void mining_channel::reset_window() NOEXCEPT
{
    shares_ = zero;
    window_ = network::steady_clock::now();
}

bool mining_channel::suggest(double difficulty, double minimum) NOEXCEPT
{
    const auto floored = std::max({ minimum, floor_, difficulty });
    if (floored == difficulty_)
        return false;

    difficulty_ = floored;
    reset_window();
    return true;
}

} // namespace server
} // namespace libbitcoin
