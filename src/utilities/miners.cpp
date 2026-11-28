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
#include <bitcoin/server/utilities/miners.hpp>

#include <chrono>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

constexpr uint64_t minute = 60'000;

// The hashes expected of a share of difficulty one.
constexpr double hashes_per_difficulty = 4'294'967'296.0;

std::string miners::to_id(uint64_t connection, uint32_t channel) NOEXCEPT
{
    return encode_base16(to_big_endian(connection)) + ":" +
        std::to_string(channel);
}

uint64_t miners::now() NOEXCEPT
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

std::string miners::to_transport(bool noised, bool websocket,
    bool secure) NOEXCEPT
{
    if (noised)
        return "noise";

    if (websocket)
        return secure ? "wss" : "ws";

    return secure ? "tls" : "tcp";
}

uint64_t miners::to_minute(uint64_t time) NOEXCEPT
{
    return time - (time % minute);
}

// Closes each whole minute since the last, as the rate (hashes per second) of
// its work, and sets the channel rate to the mean of its history.
void miners::advance(entry& item, uint64_t time) NOEXCEPT
{
    const auto current = to_minute(time);
    auto& history = item.value.history;
    while (item.minute < current)
    {
        history.push_back({ item.minute, item.work * 1'000.0 /
            to_floating(minute) });
        item.work = 0.0;
        item.minute += minute;
    }

    while (history.size() > history_minutes)
        history.pop_front();

    double total{};
    for (const auto& sample: history)
        total += sample.rate;

    item.value.hashrate = history.empty() ? 0.0 :
        total / to_floating(history.size());
}

void miners::add(channel&& value) NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    const auto id = value.id;
    const auto minute = to_minute(value.connected);
    channels_[id] = entry{ std::move(value), minute, 0.0 };
    ++generation_;
}

void miners::remove(const std::string& id) NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    if (is_nonzero(channels_.erase(id)))
        ++generation_;
}

void miners::set_difficulty(const std::string& id, double difficulty) NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    if (const auto it = channels_.find(id); it != channels_.end())
        it->second.value.difficulty = difficulty;
}

void miners::add_share(const std::string& id, share&& value,
    bool accepted) NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    const auto it = channels_.find(id);
    if (it == channels_.end())
        return;

    auto& item = it->second;
    advance(item, value.time);

    auto& channel = item.value;
    if (accepted)
    {
        ++channel.accepted;
        item.work += value.difficulty * hashes_per_difficulty;
    }
    else
    {
        ++channel.rejected;
        ++channel.rejections[value.result];
    }

    channel.last_share = value.time;
    channel.shares.push_front(std::move(value));
    while (channel.shares.size() > maximum_shares)
        channel.shares.pop_back();
}

void miners::add_block(block&& value) NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    blocks_.push_front(std::move(value));
    while (blocks_.size() > maximum_blocks)
        blocks_.pop_back();
}

miners::channels miners::get_channels(uint64_t time) NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    channels out{};
    out.reserve(channels_.size());
    for (auto& item: channels_)
    {
        advance(item.second, time);
        out.push_back(item.second.value);
    }

    return out;
}

miners::blocks miners::get_blocks() const NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    return blocks_;
}

uint64_t miners::generation() const NOEXCEPT
{
    std::unique_lock lock{ mutex_ };
    return generation_;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
