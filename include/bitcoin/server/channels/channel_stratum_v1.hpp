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

#include <bitcoin/server/channels/channel_rpc.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for stratum v1 service (universal json-rpc), carrying the state of
/// its miner's subscription and authorization.
class BCS_API channel_stratum_v1
  : public channel_rpc,
    protected network::tracker<channel_stratum_v1>
{
public:
    typedef std::shared_ptr<channel_stratum_v1> ptr;
    using options_t = settings::stratum_v1_server;

    inline channel_stratum_v1(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : channel_rpc(log, socket, identifier, config, options),
        network::tracker<channel_stratum_v1>(log),
        options_(options),
        extranonce1_(to_extranonce(identifier, options.extranonce1_size))
    {
    }

    /// Authorization.
    /// -----------------------------------------------------------------------

    /// A raw tcp channel has no http headers, so it carries its credential in
    /// mining.authorize and passes the http gate.
    inline bool authorized() const NOEXCEPT override
    {
        return downgraded() || channel_rpc::authorized();
    }

    /// Before its credential a raw tcp channel is permitted only the methods
    /// that precede mining.authorize.
    inline bool permitted(const std::string& method) const NOEXCEPT override
    {
        if (downgraded() && !credentialed())
            return std::ranges::find(handshake, method) != handshake.end();

        return channel_rpc::permitted(method);
    }

    /// The credential is satisfied (not required, by header or in-band).
    inline bool credentialed() const NOEXCEPT
    {
        return channel_rpc::authorized();
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

    inline void authorize(const std::string& worker) NOEXCEPT
    {
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

    inline void set_mask(uint32_t mask) NOEXCEPT
    {
        mask_ = mask;
    }

    inline uint32_t mask() const NOEXCEPT
    {
        return mask_;
    }

protected:
    /// Stratum errors are positional ([code, message, data]).
    inline network::rpc::error_form form() const NOEXCEPT override
    {
        return network::rpc::error_form::array;
    }

private:
    static constexpr std::array<std::string_view, 6> handshake
    {
        "mining.subscribe",
        "mining.authorize",
        "mining.configure",
        "mining.suggest_difficulty",
        "mining.suggest_target",
        "mining.extranonce.subscribe"
    };

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
    uint32_t mask_{};
    bool subscribed_{};
};

} // namespace server
} // namespace libbitcoin

#endif
