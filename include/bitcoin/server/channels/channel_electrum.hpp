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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ELECTRUM_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ELECTRUM_HPP

#include <bitcoin/server/channels/channel_rpc.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for the electrum service (universal json-rpc), carrying the
/// negotiated protocol version and client name.
class BCS_API channel_electrum
  : public channel_rpc,
    protected network::tracker<channel_electrum>
{
public:
    typedef std::shared_ptr<channel_electrum> ptr;
    using options_t = settings::electrum_server;

    inline channel_electrum(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : channel_rpc(log, socket, identifier, config, options),
        options_(options),
        network::tracker<channel_electrum>(log)
    {
    }

    /// Properties.
    /// -----------------------------------------------------------------------

    inline void set_client(const std::string& name) NOEXCEPT
    {
        name_ = name;
    }

    inline const std::string& client() const NOEXCEPT
    {
        return name_;
    }

    inline void set_version(server::electrum::version version) NOEXCEPT
    {
        version_ = version;
    }

    inline server::electrum::version version() const NOEXCEPT
    {
        return version_;
    }

    /// A non-version opener restricts negotiation (pre-1.6 client).
    inline void set_restricted() NOEXCEPT
    {
        restricted_ = true;
    }

    inline bool restricted() const NOEXCEPT
    {
        return restricted_;
    }

    /// A server.version has been negotiated (not the opener default).
    inline void set_negotiated() NOEXCEPT
    {
        negotiated_ = true;
    }

    inline bool negotiated() const NOEXCEPT
    {
        return negotiated_;
    }

    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

protected:
    /// Electrum clients send single value params (tolerated laxness).
    inline bool lax_params() const NOEXCEPT override
    {
        return true;
    }

    /// Overridden to normalize the request for the negotiated version.
    inline void dispatch(
        const network::http::request_ptr& request) NOEXCEPT override
    {
        BC_ASSERT(stranded());

        auto& body = request->body();
        if (body.contains<network::rpc::request>())
            electrum_request(body.get<network::rpc::request>().message,
                version_, options_.protocol_minimum, options_.protocol_maximum);

        channel_rpc::dispatch(request);
    }

private:
    // This is thread safe.
    const options_t& options_;

    // These are protected by strand.
    server::electrum::version version_{ server::electrum::version::v0_0 };
    std::string name_{};
    bool restricted_{};
    bool negotiated_{};
};

} // namespace server
} // namespace libbitcoin

#endif
