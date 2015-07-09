/*
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
#ifndef LIBBITCOIN_SERVER_SETTINGS_HPP
#define LIBBITCOIN_SERVER_SETTINGS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

class BCS_API settings_type
  : public node::settings
{
public:

    // options
    bool help;
    bool initchain;
    bool settings;
    bool version;

    // options + environment vars
    boost::filesystem::path configuration;

    // settings
    bool log_requests;
    bool publisher_enabled;
    node::endpoint_type unique_name;
    node::endpoint_type query_endpoint;
    node::endpoint_type heartbeat_endpoint;
    node::endpoint_type tx_publish_endpoint;
    node::endpoint_type block_publish_endpoint;
    boost::filesystem::path debug_file;
    boost::filesystem::path error_file;
    boost::filesystem::path cert_file;
    boost::filesystem::path client_certs_path;
    std::vector<node::endpoint_type> clients;
    std::vector<node::endpoint_type> peers;
};

} // namespace server
} // namespace libbitcoin

#endif
