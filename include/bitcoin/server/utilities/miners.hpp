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
#ifndef LIBBITCOIN_SERVER_UTILITIES_MINERS_HPP
#define LIBBITCOIN_SERVER_UTILITIES_MINERS_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The mining channels of the node's stratum services, their shares, hash
/// rates and the blocks they find, for administration. Thread safe.
class BCS_API miners
{
public:
    typedef std::shared_ptr<miners> ptr;

    /// Shares retained per channel, newest first.
    static constexpr size_t maximum_shares = 20;

    /// Minutes of hash rate history retained per channel.
    static constexpr size_t history_minutes = 30;

    /// Blocks retained, newest first.
    static constexpr size_t maximum_blocks = 10;

    /// Times are milliseconds since the epoch.
    struct share
    {
        uint64_t time{};
        size_t height{};
        double difficulty{};
        std::string result{};
    };

    struct sample
    {
        uint64_t time{};
        double rate{};
    };

    struct channel
    {
        std::string id{};
        std::string service{};
        std::string transport{};
        std::string address{};
        std::string client{};
        system::string_list workers{};
        uint64_t connected{};
        double difficulty{};
        double hashrate{};
        uint64_t accepted{};
        uint64_t rejected{};
        std::map<std::string, uint64_t> rejections{};
        std::optional<uint64_t> last_share{};
        std::deque<share> shares{};
        std::deque<sample> history{};
    };

    struct block
    {
        system::hash_digest hash{};
        size_t height{};
        uint64_t time{};
        std::string channel{};
    };

    using channels = std::vector<channel>;
    using blocks = std::deque<block>;

    /// The registry key of a mining channel of a connection.
    static std::string to_id(uint64_t connection, uint32_t channel) NOEXCEPT;

    /// The current time, in milliseconds since the epoch.
    static uint64_t now() NOEXCEPT;

    /// The name of a connection transport (tcp, tls, ws, wss or noise).
    static std::string to_transport(bool noised, bool websocket,
        bool secure) NOEXCEPT;

    /// Add a channel (replacing one of its key), or remove one.
    void add(channel&& value) NOEXCEPT;
    void remove(const std::string& id) NOEXCEPT;

    /// Update the share difficulty of a channel.
    void set_difficulty(const std::string& id, double difficulty) NOEXCEPT;

    /// Record a share of a channel, accepted or rejected for the result.
    void add_share(const std::string& id, share&& value,
        bool accepted) NOEXCEPT;

    /// Record a block found by a channel.
    void add_block(block&& value) NOEXCEPT;

    /// Copies of the channels (as of the time) and of the blocks found.
    channels get_channels(uint64_t time) NOEXCEPT;
    blocks get_blocks() const NOEXCEPT;

    /// Incremented as channels are added or removed.
    uint64_t generation() const NOEXCEPT;

private:
    struct entry
    {
        channel value{};
        uint64_t minute{};
        double work{};
    };

    static uint64_t to_minute(uint64_t time) NOEXCEPT;
    static void advance(entry& item, uint64_t time) NOEXCEPT;

    // These are protected by mutex.
    std::map<std::string, entry> channels_{};
    blocks blocks_{};
    uint64_t generation_{};
    mutable std::mutex mutex_{};
};

} // namespace server
} // namespace libbitcoin

#endif
