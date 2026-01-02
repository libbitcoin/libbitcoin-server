/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_HTTP_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_HTTP_HPP

#include <memory>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {
    
/// Abstract base for HTTP protocols, thread safe.
class BCS_API protocol_http
  : public server::protocol,
    protected network::tracker<server::protocol_http>
{
protected:
    inline protocol_http(const auto& session,
        const network::channel::ptr& channel) NOEXCEPT
      : server::protocol(session, channel),
        network::tracker<server::protocol_http>(session->log)
    {
    }

    /// Cache request for serialization (requires strand).
    void set_request(const network::http::request_cptr& request) NOEXCEPT;

    /// Obtain cached request and clear cache (requires strand).
    network::http::request_cptr reset_request() NOEXCEPT;

private:
    // This is protected by strand.
    network::http::request_cptr request_{};
};

} // namespace server
} // namespace libbitcoin

#endif
