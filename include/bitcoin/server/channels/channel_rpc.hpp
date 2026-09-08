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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_RPC_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_RPC_HPP

#include <bitcoin/server/channels/channel_http.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Universal json-rpc channel, served over tcp/s (by downgrade), http/s and
/// ws/s (by upgrade). Preselecting the json-rpc body is what implies the
/// downgrade detection, so a service channel derives this to speak json-rpc
/// on any of the three, adding only its own channel state.
class BCS_API channel_rpc
  : public channel_http
{
public:
    typedef std::shared_ptr<channel_rpc> ptr;
    using channel_http::channel_http;

protected:
    /// Overridden to set the preselected reader body type.
    inline value_type default_body() const NOEXCEPT override
    {
        return to_body<network::rpc::request>();
    }
};

} // namespace server
} // namespace libbitcoin

#endif
