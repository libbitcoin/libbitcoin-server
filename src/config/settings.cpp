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
#include <bitcoin/server/config/settings.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/config/config.hpp>

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
using namespace libbitcoin::node;

static std::string system_config_directory()
{
#ifdef _MSC_VER
    char directory[MAX_PATH];
    const auto result = SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL,
        SHGFP_TYPE_CURRENT, directory);
    if (SUCCEEDED(result))
        return directory;
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
    return path(system_config_directory()) / "libbitcoin" / "bs.cfg";
}

// TODO: localize descriptions.
const options_description config_type::load_options()
{
    options_description description("options");
    description.add_options()
    (
        BS_CONFIG_VARIABLE,
        value<path>(&settings.configuration),
        "The path to the configuration settings file."
    )
    (
        BS_HELP_VARIABLE ",h",
        value<bool>(&settings.help)->default_value(false)->zero_tokens(),
        "Get list of options for this command."
    )
    (
        "initchain,i",
        value<bool>(&settings.initchain)->default_value(false)->zero_tokens(),
        "Initialize database in the configured directory."
    )
    (
        BS_SETTINGS_VARIABLE ",s",
        value<bool>(&settings.settings)->default_value(false)->zero_tokens(),
        "Display the loaded configuration settings."
    )
    (
        BS_VERSION_VARIABLE ",v",
        value<bool>(&settings.version)->default_value(false)->zero_tokens(),
        "Get version information."
    );

    return description;
}

const positional_options_description config_type::load_arguments()
{
    positional_options_description description;
    return description
        .add(BS_CONFIG_VARIABLE, 1);
}

// TODO: localize descriptions.
const options_description config_type::load_environment()
{
    options_description description("environment");
    description.add_options()
    (
        // For some reason po requires this to be a lower case name.
        // The case must match the other declarations for it to compose.
        // This composes with the cmdline options and inits to system path.
        BS_CONFIG_VARIABLE,
        value<path>(&settings.configuration)->composing()
            ->default_value(default_config_path()),
        "The path to the configuration settings file."
    );

    return description;
}

// TODO: localize descriptions.
const options_description config_type::load_settings()
{
    options_description description("settings");
    description.add_options()
    /* [node] */
    (
        "node.database_threads",
        value<uint16_t>(&settings.database_threads)->default_value(BN_DATABASE_THREADS),
        "The number of threads in the database threadpool, defaults to 6."
    )
    (
        "node.network_threads",
        value<uint16_t>(&settings.network_threads)->default_value(BN_NETWORK_THREADS),
        "The number of threads in the network threadpool, defaults to 4."
    )
    (
        "node.memory_threads",
        value<uint16_t>(&settings.memory_threads)->default_value(BN_MEMORY_THREADS),
        "The number of threads in the memory threadpool, defaults to 4."
    )
    (
        "node.host_pool_capacity",
        value<uint32_t>(&settings.host_pool_capacity)->default_value(BN_HOST_POOL_CAPACITY),
        "The maximum number of peer hosts in the pool, defaults to 1000."
    )
    (
        "node.block_pool_capacity",
        value<uint32_t>(&settings.block_pool_capacity)->default_value(BN_BLOCK_POOL_CAPACITY),
        "The maximum number of orphan blocks in the pool, defaults to 50."
    )
    (
        "node.tx_pool_capacity",
        value<uint32_t>(&settings.tx_pool_capacity)->default_value(BN_TX_POOL_CAPACITY),
        "The maximum number of transactions in the pool, defaults to 2000."
    )
    (
        "node.history_height",
        value<uint32_t>(&settings.history_height)->default_value(BN_HISTORY_START_HEIGHT),
        "The minimum height of the history database, defaults to 0."
    )
    (
        "node.checkpoint_height",
        value<uint32_t>(&settings.checkpoint_height)->default_value(0),
        "The height of the checkpoint hash, defaults to 0."
    )
    (
        "node.checkpoint_hash",
        value<btc256>(&settings.checkpoint_hash)->default_value(BN_CHECKPOINT_HASH),
        "The checkpoint hash, defaults to a null hash (no checkpoint)."
    )
    (
        "node.inbound_port",
        value<uint16_t>(&settings.p2p_inbound_port)->default_value(BN_P2P_INBOUND_PORT),
        "# The port for incoming connections, defaults to 8333 (18333 for testnet)."
    )
    (
        "node.inbound_connections",
        value<uint32_t>(&settings.p2p_inbound_connections)->default_value(BN_P2P_INBOUND_CONNECTIONS),
        "The maximum number of incoming P2P network connections, defaults to 8."
    )
    (
        "node.outbound_connections",
        value<uint32_t>(&settings.p2p_outbound_connections)->default_value(BN_P2P_OUTBOUND_CONNECTIONS),
        "The maximum number of outgoing P2P network connections, defaults to 8."
    )
    (
        "node.hosts_file",
        value<path>(&settings.hosts_file)->default_value(BN_HOSTS_FILE),
        "The peer hosts cache file path, defaults to 'hosts'."
    )
    (
        "node.blockchain_path",
        value<path>(&settings.blockchain_path)->default_value(BN_BLOCKCHAIN_DIRECTORY),
        "The blockchain directory, defaults to 'blockchain'."
    )
    (
        "node.peer",
        value<std::vector<endpoint_type>>(&settings.peers)->multitoken(),
        "Persistent host:port to augment discovered hosts, multiple entries allowed."
    )
    (
        "node.ban",
        value<std::vector<endpoint_type>>(&settings.bans)->multitoken(),
        "IP address to disallow as a peer, multiple entries allowed."
    )

    /* [server] */
    (
        "server.query_endpoint",
        value<endpoint_type>(&settings.query_endpoint)->default_value({ "tcp://*:9091" }),
        "The query service endpoint, defaults to 'tcp://*:9091'."
    )
    (
        "server.heartbeat_endpoint",
        value<endpoint_type>(&settings.heartbeat_endpoint)->default_value({ "tcp://*:9092" }),
        "The heartbeat service endpoint, defaults to 'tcp://*:9092'."
    )
    (
        "server.block_publish_endpoint",
        value<endpoint_type>(&settings.block_publish_endpoint)->default_value({ "tcp://*:9093" }),
        "The block publishing service endpoint, defaults to 'tcp://*:9093'."
    )
    (
        "server.tx_publish_endpoint",
        value<endpoint_type>(&settings.tx_publish_endpoint)->default_value({ "tcp://*:9094" }),
        "The transaction publishing service endpoint, defaults to 'tcp://*:9094'."
    )
    (
        "server.publisher_enabled",
        value<bool>(&settings.publisher_enabled)->default_value(false),
        "Enable the publisher, defaults to false."
    )

    /* [identity] */
    (
        "identity.cert_file",
        value<path>(&settings.cert_file),
        "The path to the ZPL-encoded server private certificate file."
    )
    (
        "identity.client_certs_path",
        value<path>(&settings.client_certs_path),
        "The directory for ZPL-encoded client public certificate files, allows anonymous clients if not set."
    )
    (
        "identity.client",
        value<std::vector<endpoint_type>>(&settings.clients)->multitoken(),
        "Allowed client IP address, all clients allowed if none set, multiple entries allowed."
    )

    /* [logging] */
    (
        "logging.debug_file",
        value<path>(&settings.debug_file)->default_value("debug.log"),
        "The debug log file path, defaults to 'debug.log'."
    )
    (
        "logging.error_file",
        value<path>(&settings.error_file)->default_value("error.log"),
        "The error log file path, defaults to 'error.log'."
    )
    (
        "logging.log_requests",
        value<bool>(&settings.log_requests)->default_value(false),
        "Write service requests to the log, defaults to false."
    );

    return description;
}

} // namespace server
} // namespace libbitcoin

