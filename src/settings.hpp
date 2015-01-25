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
#include "endpoint.hpp"

// Not localizable.
#define BS_CONFIGURATION_VARIABLE "CONFIGURATION"
#define BS_ENVIRONMENT_VARIABLE_PREFIX "BS"

namespace libbitcoin {
namespace server {

struct settings_type
{
    // options
    bool help;
    bool initchain;
    bool settings;

    // options + environment vars
    boost::filesystem::path config;

    // settings
    bool log_requests;
    bool listener_enabled;
    bool publisher_enabled;
    uint32_t tx_pool_capacity;
    uint32_t out_connections;
    uint32_t history_height;
    std::string certificate;
    endpoint_type unique_name;
    endpoint_type service;
    endpoint_type heartbeat;
    endpoint_type tx_publish;
    endpoint_type block_publish;
    boost::filesystem::path hosts_file;
    boost::filesystem::path error_file;
    boost::filesystem::path output_file;
    boost::filesystem::path blockchain_path;
    boost::filesystem::path client_certs_path;
    std::vector<endpoint_type> peers;
    std::vector<endpoint_type> clients;
};

class config_type
{
public:
    const boost::program_options::options_description load_settings();
    const boost::program_options::options_description load_environment();
    const boost::program_options::options_description load_options();
    const boost::program_options::positional_options_description 
        load_arguments();

    settings_type settings;
};

} // namespace server
} // namespace libbitcoin

#endif
