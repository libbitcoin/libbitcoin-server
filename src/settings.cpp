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
#include "settings.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/bitcoin.hpp>

// Define after boost asio, see stackoverflow.com/a/9750437/1172329.
#ifdef _MSC_VER
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace libbitcoin {
namespace server {

using boost::filesystem::path;
using namespace boost::program_options;

static std::string system_config_directory()
{
#ifdef _MSC_VER
    char path[MAX_PATH];
    const auto result = SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL,
        SHGFP_TYPE_CURRENT, path);
    if (SUCCEEDED(result))
        return path;
    return "";
#else
    // This symbol must be defined at compile for this project.
    // Therefore do not move this definition into libbitcoin.
    return std::string(SYSCONFDIR);
#endif
}

static path default_config_path()
{
    // This subdirectory and file name must stay in sync with the path
    // for the sample distributed via the build.
    return path(system_config_directory()) / "libbitcoin" / "server.cfg";
}

const options_description config_type::load_options()
{
    options_description description("options");
    description.add_options()
    (
        BS_CONFIGURATION_VARIABLE,
        value<path>(&settings.config)->default_value(default_config_path()),
        "The path to the configuration settings file."
    )
    (
        "help,h",
        value<bool>(&settings.help)->default_value(false)->zero_tokens(),
        "Get list of options for this command."
    )
    (
        "initchain,i",
        value<bool>(&settings.initchain)->default_value(false)->zero_tokens(),
        "Initialize database in the configured directory."
    )
    (
        "settings,s",
        value<bool>(&settings.settings)->default_value(false)->zero_tokens(),
        "Display the loaded configuration settings."
    );

    return description;
}

const positional_options_description config_type::load_arguments()
{
    positional_options_description description;
    return description
        .add(BS_CONFIGURATION_VARIABLE, 1);
}

const options_description config_type::load_environment()
{
    options_description description("environment");
    description.add_options()
    (
        BS_CONFIGURATION_VARIABLE,
        value<path>(),
        "The path to the configuration settings file."
    );

    return description;
}

const options_description config_type::load_settings()
{
    options_description description("settings");
    description.add_options()
    (
        "logging.log_requests",
        value<bool>(&settings.log_requests)->default_value(false),
        "Write service requests to the log, impacts performance, defaults to false."
    )
    (
        "general.listener_enabled",
        value<bool>(&settings.listener_enabled)->default_value(true),
        "Enable the listening for incoming connections, defaults to true."
    )
    (
        "general.publisher_enabled",
        value<bool>(&settings.publisher_enabled)->default_value(false),
        "Enable the publisher, defaults to false."
    )
    (
        "general.tx_pool_capacity",
        value<uint32_t>(&settings.tx_pool_capacity)->default_value(2000),
        "Set the maximum number of transactions in the pool, defaults to 2000."
    )
    (
        "general.out_connections",
        value<uint32_t>(&settings.out_connections)->default_value(8),
        "Set the maximum number of outgoing P2P network connections, defaults to 8."
    )
    (
        "general.history_height",
        value<uint32_t>(&settings.history_height)->default_value(0),
        "Set the minimum height of the history database, defaults to 0."
    )
    (
        "identity.certificate",
        value<std::string>(&settings.certificate),
        "Set the server's public certificate, not set by default."
    )
    (
        "identity.unique_name",
        value<endpoint_type>(&settings.unique_name),
        "Set the server name, must be unique if specified."
    )
    (
        "endpoints.service",
        value<endpoint_type>(&settings.service)->default_value({ "tcp://*:9091" }),
        "Set the query service endpoint, defaults to 'tcp://*:9091'."
    )
    (
        "endpoints.heartbeat",
        value<endpoint_type>(&settings.heartbeat)->default_value({ "tcp://*:9092" }),
        "Set the heartbeat endpoint, defaults to 'tcp://*:9092'."
    )
    (
        "endpoints.block_publish",
        value<endpoint_type>(&settings.block_publish)->default_value({ "tcp://*:9093" }),
        "Set the block publishing service endpoint, defaults to 'tcp://*:9093'."
    )
    (
        "endpoints.tx_publish",
        value<endpoint_type>(&settings.tx_publish)->default_value({ "tcp://*:9094" }),
        "Set the transaction publishing service endpoint, defaults to 'tcp://*:9094'."
    )
    (
        "identity.hosts_file",
        value<path>(&settings.hosts_file)->default_value("hosts"),
        "Set the path to the alternate seeds file, defaults to 'hosts'."
    )
    (
        "logging.error_file",
        value<path>(&settings.error_file)->default_value("error.log"),
        "Set the errors log file path, defaults to 'error.log'."
    )
    (
        "logging.output_file",
        value<path>(&settings.output_file)->default_value("debug.log"),
        "Set the debug log file path, defaults to 'debug.log'."
    )
    (
        "general.blockchain_path",
        value<path>(&settings.blockchain_path)->default_value("blockchain"),
        "Set the blockchain directory, defaults to 'blockchain'."
    )
    (
        "identity.client_certs_path",
        value<path>(&settings.client_certs_path),
        "Set the client certificates directory, allows anonymous clients if not set."
    )
    (
        "identity.peer",
        value<std::vector<endpoint_type>>(&settings.peers),
        "Node by host:port to augment peer discovery, multiple entries allowed."
    )
    (
        "identity.client",
        value<std::vector<endpoint_type>>(&settings.clients),
        "Allowed client IP address, all clients allowed if none set, multiple entries allowed."
    );

    return description;
}

} // namespace server
} // namespace libbitcoin

