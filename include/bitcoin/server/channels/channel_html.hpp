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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_HTML_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_HTML_HPP

#include <bitcoin/server/channels/channel_http.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Channel for html services (admin/native), reads a plain text body.
class BCS_API channel_html
  : public channel_http,
    protected network::tracker<channel_html>
{
public:
    typedef std::shared_ptr<channel_html> ptr;

    inline channel_html(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : channel_http(log, socket, identifier, config, options),
        network::tracker<channel_html>(log)
    {
    }

protected:
    /// Overridden to set the preselected reader body type.
    inline value_type default_body() const NOEXCEPT override
    {
        return to_body<network::http::string_value>();
    }
};

} // namespace server
} // namespace libbitcoin

#endif
