/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_SETTINGS_TYPE_HPP
#define LIBBITCOIN_SERVER_SETTINGS_TYPE_HPP

#include <cstdint>
#include <boost/filesystem.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/config/settings.hpp>

namespace libbitcoin {
namespace server {

// TODO: rename to configuration
class BCS_API settings_type
  : public node::settings_type
{
public:
    settings_type()
    {
    }

    settings_type(const bc::server::settings& server_settings,
        const bc::node::settings& node_settings,
        const bc::chain::settings& chain_settings,
        const bc::network::settings& network_settings)
      : node::settings_type({ node_settings, chain_settings,
            network_settings }), server(server_settings)
    {
    }

    // options
    bool help;
    bool initchain;
    bool settings;
    bool version;

    // options + environment vars
    boost::filesystem::path configuration;

    // settings
    bc::server::settings server;
};

} // namespace server
} // namespace libbitcoin

#endif
