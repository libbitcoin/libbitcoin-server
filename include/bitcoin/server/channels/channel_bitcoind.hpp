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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BITCOIND_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_BITCOIND_HPP

#include <bitcoin/server/channels/channel_rpc.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/error/error.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Channel for the bitcoind service (universal json-rpc).
class BCS_API channel_bitcoind
  : public channel_rpc,
    protected network::tracker<channel_bitcoind>
{
public:
    typedef std::shared_ptr<channel_bitcoind> ptr;
    using options_t = settings::bitcoind_server;

    inline channel_bitcoind(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options,
        bool in_band=false) NOEXCEPT
      : channel_rpc(log, socket, identifier, config, options, in_band),
        network::tracker<channel_bitcoind>(log),
        error_status_(options.error_status)
    {
    }

    /// The http status of an unbatched json-rpc response, as bitcoind.
    static inline network::http::status to_status(
        const network::rpc::response_t& model) NOEXCEPT
    {
        using namespace network::http;
        if (!model.error.has_value() ||
            (model.jsonrpc == network::rpc::version::v2))
            return status::ok;

        switch (model.error->code)
        {
            case error::bitcoind::invalid_request:
                return status::bad_request;
            case error::bitcoind::method_not_found:
                return status::not_found;
            default:
                return status::internal_server_error;
        }
    }

    inline network::http::status to_rpc_status(
        const network::rpc::response_t& model) const NOEXCEPT override
    {
        return error_status_ ? to_status(model) : network::http::status::ok;
    }

protected:
    /// bitcoind clients send named params with json-rpc v1 (tolerated laxness).
    inline bool lax_named() const NOEXCEPT override
    {
        return true;
    }

private:
    // This is thread safe.
    const bool error_status_;
};

} // namespace server
} // namespace libbitcoin

#endif
