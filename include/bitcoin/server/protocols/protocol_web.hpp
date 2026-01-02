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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_WEB_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_WEB_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_html.hpp>

namespace libbitcoin {
namespace server {

/// Administrative web site for the node (currently just page server).
class BCS_API protocol_web
  : public protocol_html,
    protected network::tracker<protocol_web>
{
public:
    typedef std::shared_ptr<protocol_web> ptr;

    inline protocol_web(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_html(session, channel, options),
        network::tracker<protocol_web>(session->log)
    {
    }
};

} // namespace server
} // namespace libbitcoin

#endif
