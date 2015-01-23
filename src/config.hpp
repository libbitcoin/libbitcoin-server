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
#ifndef LIBBITCOIN_SERVER_CONFIG_HPP
#define LIBBITCOIN_SERVER_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace libbitcoin {
namespace server {

struct node_config_type
{
    std::string hostname;
    uint16_t port;
};

struct config_type
{
    typedef std::vector<std::string> ipaddress_list;
    typedef std::vector<node_config_type> nodes_list;

    bool log_requests;
    bool listener_enabled;
    bool publisher_enabled;

    uint32_t txpool_capacity;
    uint32_t outgoing_connections;
    uint32_t history_db_active_height;

    std::string name;
    std::string service;
    std::string heartbeat;
    std::string hosts_file;
    std::string error_file;
    std::string output_file;
    std::string blockchain_path;
    std::string tx_publish;
    std::string block_publish;
    std::string certificate;
    std::string client_allowed_certs;

    nodes_list nodes;
    ipaddress_list whitelist;
};

bool load_config(config_type& config, std::string& message, 
    boost::filesystem::path& config_path, int argc, const char* argv[]);

} // namespace server
} // namespace libbitcoin

#endif
