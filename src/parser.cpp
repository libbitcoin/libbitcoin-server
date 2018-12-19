/**
 * Copyright (c) 2011-2018 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/parser.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <bitcoin/system.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parser.hpp>
#include <bitcoin/server/settings.hpp>

BC_DECLARE_CONFIG_DEFAULT_PATH("libbitcoin" / "bs.cfg")

// TODO: localize descriptions.

namespace libbitcoin {
namespace server {

using namespace boost::filesystem;
using namespace boost::program_options;
using namespace bc::network;
using namespace bc::system;
using namespace bc::system::config;

// Initialize configuration by copying the given instance.
parser::parser(const configuration& defaults)
  : configured(defaults)
{
}

// Initialize configuration using defaults of the given context.
parser::parser(config::settings context)
  : configured(context)
{
    using serve = message::version::service;

    // Logs will slow things if not rotated.
    configured.network.rotation_size = 10000000;

    // It is a public network.
    configured.network.inbound_connections = 100;

    // Optimal for sync and network penetration.
    configured.network.outbound_connections = 8;

    // A node allows 10000 host names by default.
    configured.network.host_pool_capacity = 10000;

    // Expose full node (1) and witness (8) network services by default.
    configured.network.services = serve::node_network | serve::node_witness;

    // TODO: set this independently on each public endpoint.
    configured.protocol.message_size_limit = max_block_size + 100;
}

options_metadata parser::load_options()
{
    options_metadata description("options");
    description.add_options()
    (
        BS_CONFIG_VARIABLE ",c",
        value<path>(&configured.file),
        "Specify path to a configuration settings file."
    )
    (
        BS_HELP_VARIABLE ",h",
        value<bool>(&configured.help)->
            default_value(false)->zero_tokens(),
        "Display command line options."
    )
    (
        "initchain,i",
        value<bool>(&configured.initchain)->
            default_value(false)->zero_tokens(),
        "Initialize blockchain in the configured directory."
    )
    (
        BS_SETTINGS_VARIABLE ",s",
        value<bool>(&configured.settings)->
            default_value(false)->zero_tokens(),
        "Display all configuration settings."
    )
    (
        BS_VERSION_VARIABLE ",v",
        value<bool>(&configured.version)->
            default_value(false)->zero_tokens(),
        "Display version information."
    );

    return description;
}

arguments_metadata parser::load_arguments()
{
    arguments_metadata description;
    return description
        .add(BS_CONFIG_VARIABLE, 1);
}

options_metadata parser::load_environment()
{
    options_metadata description("environment");
    description.add_options()
    (
        // For some reason po requires this to be a lower case name.
        // The case must match the other declarations for it to compose.
        // This composes with the cmdline options and inits to system path.
        BS_CONFIG_VARIABLE,
        value<path>(&configured.file)->composing()
            ->default_value(config_default_path()),
        "The path to the configuration settings file."
    );

    return description;
}

options_metadata parser::load_settings()
{
    options_metadata description("settings");
    description.add_options()

    /* [log] */
    (
        "log.debug_file",
        value<path>(&configured.network.debug_file),
        "The debug log file path, defaults to 'debug.log'."
    )
    (
        "log.error_file",
        value<path>(&configured.network.error_file),
        "The error log file path, defaults to 'error.log'."
    )
    (
        "log.archive_directory",
        value<path>(&configured.network.archive_directory),
        "The log archive directory, defaults to 'archive'."
    )
    (
        "log.rotation_size",
        value<size_t>(&configured.network.rotation_size),
        "The size at which a log is archived, defaults to 10000000 (0 disables)."
    )
    (
        "log.minimum_free_space",
        value<size_t>(&configured.network.minimum_free_space),
        "The minimum free space required in the archive directory, defaults to 0."
    )
    (
        "log.maximum_archive_size",
        value<size_t>(&configured.network.maximum_archive_size),
        "The maximum combined size of archived logs, defaults to 0 (maximum)."
    )
    (
        "log.maximum_archive_files",
        value<size_t>(&configured.network.maximum_archive_files),
        "The maximum number of logs to archive, defaults to 0 (maximum)."
    )
    (
        "log.statistics_server",
        value<config::authority>(&configured.network.statistics_server),
        "The address of the statistics collection server, defaults to none."
    )
    (
        "log.verbose",
        value<bool>(&configured.network.verbose),
        "Enable verbose logging, defaults to false."
    )

    /* [bitcoin] */
    (
        "bitcoin.retargeting_factor",
        PROPERTY(uint32_t, configured.bitcoin.retargeting_factor),
        "The difficulty retargeting factor, defaults to 4."
    )
    (
        "bitcoin.block_spacing_seconds",
        PROPERTY(uint32_t, configured.bitcoin.block_spacing_seconds),
        "The target block period in seconds, defaults to 600."
    )
    (
        "bitcoin.timestamp_limit_seconds",
        value<uint32_t>(&configured.bitcoin.timestamp_limit_seconds),
        "The future timestamp allowance in seconds, defaults to 7200."
    )
    (
        "bitcoin.retargeting_interval_seconds",
        PROPERTY(uint32_t, configured.bitcoin.retargeting_interval_seconds),
        "The difficulty retargeting period in seconds, defaults to 1209600."
    )
    (
        "bitcoin.proof_of_work_limit",
        value<uint32_t>(&configured.bitcoin.proof_of_work_limit),
        "The proof of work limit, defaults to 486604799."
    )
    (
        "bitcoin.initial_block_subsidy_bitcoin",
        PROPERTY(uint64_t, configured.bitcoin.initial_block_subsidy_bitcoin),
        "The initial block subsidy in bitcoin, defaults to 50."
    )
    (
        "bitcoin.subsidy_interval",
        PROPERTY(uint64_t, configured.bitcoin.subsidy_interval),
        "The subsidy halving period in number of blocks, defaults to 210000."
    )
    (
        "bitcoin.genesis_block",
        value<config::block>(&configured.bitcoin.genesis_block),
        "The genesis block, defaults to mainnet."
    )
    (
        "bitcoin.activation_threshold",
        value<size_t>(&configured.bitcoin.activation_threshold),
        "The number of new version blocks required for bip34 style soft fork activation, defaults to 750."
    )
    (
        "bitcoin.enforcement_threshold",
        value<size_t>(&configured.bitcoin.enforcement_threshold),
        "The number of new version blocks required for bip34 style soft fork enforcement, defaults to 950."
    )
    (
        "bitcoin.activation_sample",
        value<size_t>(&configured.bitcoin.activation_sample),
        "The number of blocks considered for bip34 style soft fork activation, defaults to 1000."
    )
    (
        "bitcoin.bip65_freeze",
        value<size_t>(&configured.bitcoin.bip65_freeze),
        "The block height to freeze the bip65 softfork as in bip90, defaults to 388381."
    )
    (
        "bitcoin.bip66_freeze",
        value<size_t>(&configured.bitcoin.bip66_freeze),
        "The block height to freeze the bip66 softfork as in bip90, defaults to 363725."
    )
    (
        "bitcoin.bip34_freeze",
        value<size_t>(&configured.bitcoin.bip34_freeze),
        "The block height to freeze the bip34 softfork as in bip90, defaults to 227931."
    )
    (
        "bitcoin.bip16_activation_time",
        value<uint32_t>(&configured.bitcoin.bip16_activation_time),
        "The activation time for bip16 in unix time, defaults to 1333238400."
    )
    (
        "bitcoin.bip34_active_checkpoint",
        value<config::checkpoint>(&configured.bitcoin.bip34_active_checkpoint),
        "The hash:height checkpoint for bip34 activation, defaults to 000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8:227931."
    )
    (
        "bitcoin.bip9_bit0_active_checkpoint",
        value<config::checkpoint>(&configured.bitcoin.bip9_bit0_active_checkpoint),
        "The hash:height checkpoint for bip9 bit0 activation, defaults to 000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5:419328."
    )
    (
        "bitcoin.bip9_bit1_active_checkpoint",
        value<config::checkpoint>(&configured.bitcoin.bip9_bit1_active_checkpoint),
        "The hash:height checkpoint for bip9 bit1 activation, defaults to 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893:481824."
    )

    /* [network] */
    (
        "network.threads",
        value<uint32_t>(&configured.network.threads),
        "The minimum number of threads in the network threadpool, defaults to 0 (physical cores)."
    )
    (
        "network.protocol_maximum",
        value<uint32_t>(&configured.network.protocol_maximum),
        "The maximum network protocol version, defaults to 70013."
    )
    (
        "network.protocol_minimum",
        value<uint32_t>(&configured.network.protocol_minimum),
        "The minimum network protocol version, defaults to 31402."
    )
    (
        "network.services",
        value<uint64_t>(&configured.network.services),
        "The services exposed by network connections, defaults to 9 (full node, witness)."
    )
    (
        "network.invalid_services",
        value<uint64_t>(&configured.network.invalid_services),
        "The advertised services that cause a peer to be dropped, defaults to 176."
    )
    (
        "network.validate_checksum",
        value<bool>(&configured.network.validate_checksum),
        "Validate the checksum of network messages, defaults to false."
    )
    (
        "network.identifier",
        value<uint32_t>(&configured.network.identifier),
        "The magic number for message headers, defaults to 3652501241."
    )
    (
        "network.inbound_port",
        value<uint16_t>(&configured.network.inbound_port),
        "The port for incoming connections, defaults to 8333."
    )
    (
        "network.inbound_connections",
        value<uint32_t>(&configured.network.inbound_connections),
        "The target number of incoming network connections, defaults to 0."
    )
    (
        "network.outbound_connections",
        value<uint32_t>(&configured.network.outbound_connections),
        "The target number of outgoing network connections, defaults to 2."
    )
    (
        "network.manual_attempt_limit",
        value<uint32_t>(&configured.network.manual_attempt_limit),
        "The attempt limit for manual connection establishment, defaults to 0 (forever)."
    )
    (
        "network.connect_batch_size",
        value<uint32_t>(&configured.network.connect_batch_size),
        "The number of concurrent attempts to establish one connection, defaults to 5."
    )
    (
        "network.connect_timeout_seconds",
        value<uint32_t>(&configured.network.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to 5."
    )
    (
        "network.channel_handshake_seconds",
        value<uint32_t>(&configured.network.channel_handshake_seconds),
        "The time limit to complete the connection handshake, defaults to 30."
    )
    (
        "network.channel_germination_seconds",
        value<uint32_t>(&configured.network.channel_germination_seconds),
        "The time limit for obtaining seed addresses, defaults to 30."
    )
    (
        "network.channel_heartbeat_minutes",
        value<uint32_t>(&configured.network.channel_heartbeat_minutes),
        "The time between ping messages, defaults to 5."
    )
    (
        "network.channel_inactivity_minutes",
        value<uint32_t>(&configured.network.channel_inactivity_minutes),
        "The inactivity time limit for any connection, defaults to 30."
    )
    (
        "network.channel_expiration_minutes",
        value<uint32_t>(&configured.network.channel_expiration_minutes),
        "The age limit for any connection, defaults to 1440."
    )
    (
        "network.host_pool_capacity",
        value<uint32_t>(&configured.network.host_pool_capacity),
        "The maximum number of peer hosts in the pool, defaults to 10000."
    )
    (
        "network.hosts_file",
        value<path>(&configured.network.hosts_file),
        "The peer hosts cache file path, defaults to 'hosts.cache'."
    )
    (
        "network.self",
        value<config::authority>(&configured.network.self),
        "The advertised public address of this node, defaults to none."
    )
    (
        "network.blacklist",
        value<config::authority::list>(&configured.network.blacklists),
        "IP address to disallow as a peer, multiple entries allowed."
    )
    (
        "network.peer",
        value<config::endpoint::list>(&configured.network.peers),
        "A persistent peer node, multiple entries allowed."
    )
    (
        "network.seed",
        value<config::endpoint::list>(&configured.network.seeds),
        "A seed node for initializing the host pool, multiple entries allowed."
    )

    /* [database] */
    (
        "database.directory",
        value<path>(&configured.database.directory),
        "The blockchain database directory, defaults to 'blockchain'."
    )
    (
        "database.flush_writes",
        value<bool>(&configured.database.flush_writes),
        "Flush each write to disk, defaults to false."
    )
    (
        "database.file_growth_rate",
        value<uint16_t>(&configured.database.file_growth_rate),
        "Full database files increase by this percentage, defaults to 5."
    )
    (
        "database.block_table_buckets",
        value<uint32_t>(&configured.database.block_table_buckets),
        "Block hash table size, defaults to 650000."
    )
    (
        "database.transaction_table_buckets",
        value<uint32_t>(&configured.database.transaction_table_buckets),
        "Transaction hash table size, defaults to 110000000."
    )
    (
        "database.address_table_buckets",
        value<uint32_t>(&configured.database.address_table_buckets),
        "Address hash table size, defaults to 107000000."
    )
    (
        "database.cache_capacity",
        value<uint32_t>(&configured.database.cache_capacity),
        "The maximum number of entries in the unspent outputs cache, defaults to 10000."
    )

    /* [blockchain] */
    (
        "blockchain.cores",
        value<uint32_t>(&configured.chain.cores),
        "The number of cores dedicated to block validation, defaults to 0 (physical cores)."
    )
    (
        "blockchain.priority",
        value<bool>(&configured.chain.priority),
        "Use high thread priority for block validation, defaults to true."
    )
    (
        "blockchain.use_libconsensus",
        value<bool>(&configured.chain.use_libconsensus),
        "Use libconsensus for script validation if integrated, defaults to false."
    )
    (
        "blockchain.reorganization_limit",
        value<uint32_t>(&configured.chain.reorganization_limit),
        "The maximum reorganization depth, defaults to 0 (unlimited)."
    )
    (
        "blockchain.checkpoint",
        value<config::checkpoint::list>(&configured.chain.checkpoints),
        "A hash:height checkpoint, multiple entries allowed."
    )

    /* [fork] */
    (
        "fork.difficult",
        value<bool>(&configured.chain.difficult),
        "Require difficult blocks, defaults to true (use false for testnet)."
    )
    (
        "fork.retarget",
        value<bool>(&configured.chain.retarget),
        "Retarget difficulty, defaults to true."
    )
    (
        "fork.bip16",
        value<bool>(&configured.chain.bip16),
        "Add pay-to-script-hash processing, defaults to true (soft fork)."
    )
    (
        "fork.bip30",
        value<bool>(&configured.chain.bip30),
        "Disallow collision of unspent transaction hashes, defaults to true (soft fork)."
    )
    (
        "fork.bip34",
        value<bool>(&configured.chain.bip34),
        "Require coinbase input includes block height, defaults to true (soft fork)."
    )
    (
        "fork.bip66",
        value<bool>(&configured.chain.bip66),
        "Require strict signature encoding, defaults to true (soft fork)."
    )
    (
        "fork.bip65",
        value<bool>(&configured.chain.bip65),
        "Add check-locktime-verify op code, defaults to true (soft fork)."
    )
    (
        "fork.bip90",
        value<bool>(&configured.chain.bip90),
        "Assume bip34, bip65, and bip66 activation if enabled, defaults to true (hard fork)."
    )
    (
        "fork.bip68",
        value<bool>(&configured.chain.bip68),
        "Add relative locktime enforcement, defaults to true (soft fork)."
    )
    (
        "fork.bip112",
        value<bool>(&configured.chain.bip112),
        "Add check-sequence-verify op code, defaults to true (soft fork)."
    )
    (
        "fork.bip113",
        value<bool>(&configured.chain.bip113),
        "Use median time past for locktime, defaults to true (soft fork)."
    )
    (
        "fork.bip141",
        value<bool>(&configured.chain.bip141),
        "Segregated witness consensus layer, defaults to true (soft fork)."
    )
    (
        "fork.bip143",
        value<bool>(&configured.chain.bip143),
        "Version 0 transaction digest, defaults to true (soft fork)."
    )
    (
        "fork.bip147",
        value<bool>(&configured.chain.bip147),
        "Prevent dummy value malleability, defaults to true (soft fork)."
    )
    (
        "fork.time_warp_patch",
        value<bool>(&configured.chain.time_warp_patch),
        "Fix time warp bug, defaults to false (hard fork)."
    )
    (
        "fork.retarget_overflow_patch",
        value<bool>(&configured.chain.retarget_overflow_patch),
        "Fix target overflow for very low difficulty, defaults to false (hard fork)."
    )
    (
        "fork.scrypt_proof_of_work",
        value<bool>(&configured.chain.scrypt_proof_of_work),
        "Use scrypt hashing for proof of work, defaults to false (hard fork)."
    )

    /* [node] */
    (
        "node.maximum_deviation",
        value<float>(&configured.node.maximum_deviation),
        "The response rate standard deviation below which a peer is dropped, defaults to 1.5."
    )
    (
        "node.block_latency_seconds",
        value<uint32_t>(&configured.node.block_latency_seconds),
        "The maximum time to wait for a requested block, defaults to 5."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.notify_limit_hours",
        value<uint32_t>(&configured.chain.notify_limit_hours),
        "Disable relay when top block age exceeds, defaults to 24 (0 disables)."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.byte_fee_satoshis",
        value<float>(&configured.chain.byte_fee_satoshis),
        "The minimum fee per byte, cumulative for conflicts, defaults to 1."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.sigop_fee_satoshis",
        value<float>(&configured.chain.sigop_fee_satoshis),
        "The minimum fee per sigop, additional to byte fee, defaults to 100."
    )
    (
        /* Internally this is blockchain, but it is conceptually a node setting. */
        "node.minimum_output_satoshis",
        value<uint64_t>(&configured.chain.minimum_output_satoshis),
        "The minimum output value, defaults to 500."
    )
    (
        /* Internally this is network, but it is conceptually a node setting. */
        "node.relay_transactions",
        value<bool>(&configured.network.relay_transactions),
        "Request that peers relay transactions, defaults to false."
    )
    (
        "node.refresh_transactions",
        value<bool>(&configured.node.refresh_transactions),
        "Request transactions on each channel start, defaults to false."
    )

    /* [server] */
    (
        /* Internally this is database, but it applies to server and not node. */
        "server.index_addresses",
        value<bool>(&configured.database.index_addresses),
        "Enable payment and stealth address indexing, defaults to true."
    )
    /* Internally this is protocol, but application to server is more intuitive. */
    (
        "server.send_high_water",
        value<uint32_t>(&configured.protocol.send_high_water),
        "Drop messages at this outgoing backlog level, defaults to 100."
    )
    /* Internally this is protocol, but application to server is more intuitive. */
    (
        "server.receive_high_water",
        value<uint32_t>(&configured.protocol.receive_high_water),
        "Drop messages at this incoming backlog level, defaults to 100."
    )
    /* Internally this is protocol, but application to server is more intuitive. */
    (
        "server.handshake_seconds",
        value<uint32_t>(&configured.protocol.handshake_seconds),
        "The time limit to complete the connection handshake, defaults to 30."
    )
    (
        "server.secure_only",
        value<bool>(&configured.server.secure_only),
        "Disable public endpoints, defaults to false."
    )
    (
        "server.query_workers",
        value<uint16_t>(&configured.server.query_workers),
        "The number of query worker threads per endpoint, defaults to 1 (0 disables service)."
    )
    (
        "server.subscription_limit",
        value<uint32_t>(&configured.server.subscription_limit),
        "The maximum number of query subscriptions, defaults to 1000 (0 disables subscribe)."
    )
    (
        "server.subscription_expiration_minutes",
        value<uint32_t>(&configured.server.subscription_expiration_minutes),
        "The query subscription expiration time, defaults to 10 (0 disables expiration)."
    )
    (
        "server.heartbeat_service_seconds",
        value<uint32_t>(&configured.server.heartbeat_service_seconds),
        "The heartbeat service interval, defaults to 5 (0 disables service)."
    )
    (
        "server.block_service_enabled",
        value<bool>(&configured.server.block_service_enabled),
        "Enable the block publishing service, defaults to false."
    )
    (
        "server.transaction_service_enabled",
        value<bool>(&configured.server.transaction_service_enabled),
        "Enable the transaction publishing service, defaults to false."
    )
    (
        "server.secure_query_endpoint",
        value<endpoint>(&configured.server.secure_query_endpoint),
        "The secure query endpoint, defaults to 'tcp://*:9081'."
    )
    (
        "server.secure_heartbeat_endpoint",
        value<endpoint>(&configured.server.secure_heartbeat_endpoint),
        "The secure heartbeat endpoint, defaults to 'tcp://*:9082'."
    )
    (
        "server.secure_block_endpoint",
        value<endpoint>(&configured.server.secure_block_endpoint),
        "The secure block publishing endpoint, defaults to 'tcp://*:9083'."
    )
    (
        "server.secure_transaction_endpoint",
        value<endpoint>(&configured.server.secure_transaction_endpoint),
        "The secure transaction publishing endpoint, defaults to 'tcp://*:9084'."
    )
    (
        "server.public_query_endpoint",
        value<endpoint>(&configured.server.public_query_endpoint),
        "The public query endpoint, defaults to 'tcp://*:9091'."
    )
    (
        "server.public_heartbeat_endpoint",
        value<endpoint>(&configured.server.public_heartbeat_endpoint),
        "The public heartbeat endpoint, defaults to 'tcp://*:9092'."
    )
    (
        "server.public_block_endpoint",
        value<endpoint>(&configured.server.public_block_endpoint),
        "The public block publishing endpoint, defaults to 'tcp://*:9093'."
    )
    (
        "server.public_transaction_endpoint",
        value<endpoint>(&configured.server.public_transaction_endpoint),
        "The public transaction publishing endpoint, defaults to 'tcp://*:9094'."
    )
    (
        "server.server_private_key",
        value<config::sodium>(&configured.server.server_private_key),
        "The Z85-encoded private key of the server, enables secure endpoints."
    )
    (
        "server.client_public_key",
        value<config::sodium::list>(&configured.server.client_public_keys),
        "Allowed Z85-encoded public key of the client, multiple entries allowed."
    )
    (
        "server.client_address",
        value<config::authority::list>(&configured.server.client_addresses),
        "Allowed client IP address, multiple entries allowed."
    )
    (
        "server.blacklist",
        value<config::authority::list>(&configured.server.blacklists),
        "Blocked client IP address, multiple entries allowed."
    );

    return description;
}

bool parser::parse(int argc, const char* argv[], std::ostream& error)
{
    try
    {
        auto file = false;
        variables_map variables;
        load_command_variables(variables, argc, argv);
        load_environment_variables(variables, BS_ENVIRONMENT_VARIABLE_PREFIX);

        // Don't load the rest if any of these options are specified.
        if (!get_option(variables, BS_VERSION_VARIABLE) &&
            !get_option(variables, BS_SETTINGS_VARIABLE) &&
            !get_option(variables, BS_HELP_VARIABLE))
        {
            // Returns true if the settings were loaded from a file.
            file = load_configuration_variables(variables, BS_CONFIG_VARIABLE);
        }

        // Update bound variables in metadata.settings.
        notify(variables);

        // Clear the config file path if it wasn't used.
        if (!file)
            configured.file.clear();
    }
    catch (const boost::program_options::error& e)
    {
        // This is obtained from boost, which circumvents our localization.
        error << format_invalid_parameter(e.what()) << std::endl;
        return false;
    }

    return true;
}

} // namespace server
} // namespace libbitcoin
