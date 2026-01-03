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
#include <bitcoin/server/parser.hpp>

#include <filesystem>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/settings.hpp>

#define BC_HTTP_SERVER_NAME "libbitcoin/4.0"

////std::filesystem::path config_default_path() NOEXCEPT
////{
////    return { "libbitcoin/bn.cfg" };
////}

namespace libbitcoin {
namespace server {

using namespace bc::system;
using namespace bc::system::config;
using namespace boost::program_options;

// Initialize configuration using defaults of the given context.
parser::parser(system::chain::selection context,
    const server::settings::embedded_pages& explore,
    const server::settings::embedded_pages& web) NOEXCEPT
  : configured(context, explore, web)
{
    // node

    configured.node.threads = 32;

    // network

    using level = network::messages::peer::level;
    using service = network::messages::peer::service;

    configured.network.threads = 16;
    configured.network.enable_relay = true;
    configured.network.enable_address = true;
    configured.network.enable_address_v2 = false;
    configured.network.enable_witness_tx = false;
    configured.network.enable_compact = false;
    configured.network.outbound.host_pool_capacity = 10000;
    configured.network.outbound.connections = 100;
    configured.network.inbound.connections = 100;
    configured.network.maximum_skew_minutes = 120;
    configured.network.protocol_minimum = level::headers_protocol;
    configured.network.protocol_maximum = level::bip130;

    // services_minimum must be node_witness to be a witness node.
    configured.network.services_minimum = service::node_network | service::node_witness;
    configured.network.services_maximum = service::node_network | service::node_witness;

    // TODO: from bitcoind, revert to defaults when seeds are up.
    configured.network.outbound.seeds.clear();
    configured.network.outbound.seeds.emplace_back("seed.bitcoin.sipa.be", 8333_u16);
    configured.network.outbound.seeds.emplace_back("dnsseed.bluematt.me", 8333_u16);
    ////configured.network.outbound.seeds.emplace_back("dnsseed.bitcoin.dashjr-list-of-p2p-nodes.us", 8333_u16);
    configured.network.outbound.seeds.emplace_back("seed.bitcoin.jonasschnelli.ch", 8333_u16);
    configured.network.outbound.seeds.emplace_back("seed.btc.petertodd.net", 8333_u16);
    configured.network.outbound.seeds.emplace_back("seed.bitcoin.sprovoost.nl", 8333_u16);
    configured.network.outbound.seeds.emplace_back("dnsseed.emzy.de", 8333_u16);
    configured.network.outbound.seeds.emplace_back("seed.bitcoin.wiz.biz", 8333_u16);
    configured.network.outbound.seeds.emplace_back("seed.mainnet.achownodes.xyz", 8333_u16);

    // admin
    configured.server.web.binds.emplace_back(asio::address{}, 8080_u16);
    configured.server.explore.binds.emplace_back(asio::address{}, 8180_u16);
    configured.server.bitcoind.binds.emplace_back(asio::address{}, 8380_u16);
    configured.server.electrum.binds.emplace_back(asio::address{}, 8480_u16);
    configured.server.stratum_v1.binds.emplace_back(asio::address{}, 8580_u16);
    configured.server.stratum_v2.binds.emplace_back(asio::address{}, 8680_u16);

    // SCALE: LF2.2 @ 850K.

    // database (archive)

    configured.database.header_buckets = 386'364;
    configured.database.header_size = 21'000'000;
    configured.database.header_rate = 5;

    configured.database.input_size = 92'500'000'000;
    configured.database.input_rate = 5;

    configured.database.output_size = 25'300'000'000;
    configured.database.output_rate = 5;

    // point table set to 2.2LF @ ~900k.
    configured.database.point_buckets = 1'365'977'136;
    configured.database.point_size = 25'700'000'000;
    configured.database.point_rate = 5;

    configured.database.ins_size = 8'550'000'000;
    configured.database.ins_rate = 5;

    configured.database.outs_size = 3'700'000'000;
    configured.database.outs_rate = 5;

    configured.database.tx_buckets = 469'222'525;
    configured.database.tx_size = 17'000'000'000;
    configured.database.tx_rate = 5;

    configured.database.txs_buckets = 900'001;
    configured.database.txs_size = 1'050'000'000;
    configured.database.txs_rate = 5;

    // database (indexes)

    configured.database.candidate_size = 2'575'500;
    configured.database.candidate_rate = 5;

    configured.database.confirmed_size = 2'575'500;
    configured.database.confirmed_rate = 5;

    configured.database.strong_tx_buckets = 469'222'525;
    configured.database.strong_tx_size = 2'900'000'000;
    configured.database.strong_tx_rate = 5;

    // database (caches)

    configured.database.duplicate_buckets = 1024;
    configured.database.duplicate_size = 44;
    configured.database.duplicate_rate = 5;

    configured.database.prevout_buckets = 0;
    configured.database.prevout_size = 1;
    configured.database.prevout_rate = 5;

    configured.database.validated_bk_buckets = 900'001;
    configured.database.validated_bk_size = 1'700'000;
    configured.database.validated_bk_rate = 5;

    configured.database.validated_tx_buckets = 1;
    configured.database.validated_tx_size = 1;
    configured.database.validated_tx_rate = 5;

    // database (optionals)

    configured.database.address_buckets = 1;
    configured.database.address_size = 1;
    configured.database.address_rate = 5;

    // also disabled by filter_tx
    configured.database.filter_bk_buckets = 0;
    configured.database.filter_bk_size = 1;
    configured.database.filter_bk_rate = 5;

    // also disabled by filter_bk
    configured.database.filter_tx_buckets = 0;
    configured.database.filter_tx_size = 1;
    configured.database.filter_tx_rate = 5;
}

options_metadata parser::load_options() THROWS
{
    options_metadata description("options");
    description.add_options()
    (
        BS_CONFIG_VARIABLE ",c",
        value<std::filesystem::path>(&configured.file),
        "Specify path to a configuration settings file."
    )
    // Information.
    (
        BS_HELP_VARIABLE ",h",
        value<bool>(&configured.help)->
            default_value(false)->zero_tokens(),
        "Display command line options."
    )
    (
        BS_HARDWARE_VARIABLE ",d",
        value<bool>(&configured.hardware)->
            default_value(false)->zero_tokens(),
        "Display hardware compatibility."
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
    )
    // Actions.
    (
        BS_NEWSTORE_VARIABLE ",n",
        value<bool>(&configured.newstore)->
            default_value(false)->zero_tokens(),
        "Create new store in configured directory."
    )
    (
        BS_BACKUP_VARIABLE ",b",
        value<bool>(&configured.backup)->
            default_value(false)->zero_tokens(),
        "Backup to a snapshot (can also do live)."
    )
    (
        BS_RESTORE_VARIABLE ",r",
        value<bool>(&configured.restore)->
            default_value(false)->zero_tokens(),
        "Restore from most recent snapshot."
    )
    // Chain scans.
    (
        BS_FLAGS_VARIABLE ",f",
        value<bool>(&configured.flags)->
            default_value(false)->zero_tokens(),
        "Scan and display all flag transitions."
    )
    (
        BS_SLABS_VARIABLE ",a",
        value<bool>(&configured.slabs)->
            default_value(false)->zero_tokens(),
        "Scan and display store slab measures."
    )
    (
        BS_BUCKETS_VARIABLE ",k",
        value<bool>(&configured.buckets)->
            default_value(false)->zero_tokens(),
        "Scan and display all bucket densities."
    )
    (
        BS_COLLISIONS_VARIABLE ",l",
        value<bool>(&configured.collisions)->
            default_value(false)->zero_tokens(),
        "Scan and display hashmap collision stats (may exceed RAM and result in SIGKILL)."
    )
    (
        BS_INFORMATION_VARIABLE ",i",
        value<bool>(&configured.information)->
            default_value(false)->zero_tokens(),
        "Scan and display store information."
    )
    // Ad-hoc Testing.
    (
        BS_READ_VARIABLE ",t",
        value<config::hash256>(&configured.test)->
            default_value(system::null_hash),
        "Run built-in read test and display."
    )
    (
        BS_WRITE_VARIABLE ",w",
        value<config::hash256>(&configured.write)->
            default_value(system::null_hash),
        "Run built-in write test and display."
    );

    return description;
}

arguments_metadata parser::load_arguments() THROWS
{
    arguments_metadata description;
    return description
        .add(BS_CONFIG_VARIABLE, 1);
}

options_metadata parser::load_environment() THROWS
{
    options_metadata description("environment");
    description.add_options()
    (
        // For some reason po requires this to be a lower case name.
        // The case must match the other declarations for it to compose.
        // This composes with the cmdline options and inits to default path.
        BS_CONFIG_VARIABLE,
        value<std::filesystem::path>(&configured.file)->composing()
            /*->default_value(config_default_path())*/,
        "The path to the configuration settings file."
    );

    return description;
}

options_metadata parser::load_settings() THROWS
{
    options_metadata description("settings");
    description.add_options()

    /* [forks] */
    (
        "forks.difficult",
        value<bool>(&configured.bitcoin.forks.difficult),
        "Require difficult blocks, defaults to 'true' (use false for testnet)."
    )
    (
        "forks.retarget",
        value<bool>(&configured.bitcoin.forks.retarget),
        "Retarget difficulty, defaults to 'true'."
    )
    (
        "forks.bip16",
        value<bool>(&configured.bitcoin.forks.bip16),
        "Add pay-to-script-hash processing, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip30",
        value<bool>(&configured.bitcoin.forks.bip30),
        "Disallow collision of unspent transaction hashes, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip34",
        value<bool>(&configured.bitcoin.forks.bip34),
        "Require coinbase input includes block height, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip42",
        value<bool>(&configured.bitcoin.forks.bip42),
        "Finite monetary supply, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip66",
        value<bool>(&configured.bitcoin.forks.bip66),
        "Require strict signature encoding, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip65",
        value<bool>(&configured.bitcoin.forks.bip65),
        "Add check-locktime-verify op code, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip90",
        value<bool>(&configured.bitcoin.forks.bip90),
        "Assume bip34, bip65, and bip66 activation if enabled, defaults to 'true' (hard fork)."
    )
    (
        "forks.bip68",
        value<bool>(&configured.bitcoin.forks.bip68),
        "Add relative locktime enforcement, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip112",
        value<bool>(&configured.bitcoin.forks.bip112),
        "Add check-sequence-verify op code, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip113",
        value<bool>(&configured.bitcoin.forks.bip113),
        "Use median time past for locktime, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip141",
        value<bool>(&configured.bitcoin.forks.bip141),
        "Segregated witness consensus layer, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip143",
        value<bool>(&configured.bitcoin.forks.bip143),
        "Witness version 0 (segwit), defaults to 'true' (soft fork)."
    )
    (
        "forks.bip147",
        value<bool>(&configured.bitcoin.forks.bip147),
        "Prevent dummy value malleability, defaults to 'true' (soft fork)."
    )
    (
        "forks.bip341",
        value<bool>(&configured.bitcoin.forks.bip341),
        "Witness version 1 (taproot), defaults to 'true' (soft fork)."
    )
    (
        "forks.bip342",
        value<bool>(&configured.bitcoin.forks.bip342),
        "Validation of taproot script, defaults to 'true' (soft fork)."
    )
    (
        "forks.time_warp_patch",
        value<bool>(&configured.bitcoin.forks.time_warp_patch),
        "Fix time warp bug, defaults to 'false' (hard fork)."
    )
    (
        "forks.retarget_overflow_patch",
        value<bool>(&configured.bitcoin.forks.retarget_overflow_patch),
        "Fix target overflow for very low difficulty, defaults to 'false' (hard fork)."
    )
    (
        "forks.scrypt_proof_of_work",
        value<bool>(&configured.bitcoin.forks.scrypt_proof_of_work),
        "Use scrypt hashing for proof of work, defaults to 'false' (hard fork)."
    )

    /* [bitcoin] */
    (
        "bitcoin.initial_block_subsidy_bitcoin",
        value<uint64_t>(&configured.bitcoin.initial_subsidy_bitcoin),
        "The initial block subsidy, defaults to '50'."
    )
    (
        "bitcoin.subsidy_interval",
        value<uint32_t>(&configured.bitcoin.subsidy_interval_blocks),
        "The subsidy halving period, defaults to '210000'."
    )
    (
        "bitcoin.timestamp_limit_seconds",
        value<uint32_t>(&configured.bitcoin.timestamp_limit_seconds),
        "The future timestamp allowance, defaults to '7200'."
    )
    (
        "bitcoin.retargeting_factor",
        value<uint32_t>(&configured.bitcoin.retargeting_factor),
        "The difficulty retargeting factor, defaults to '4'."
    )
    (
        "bitcoin.retargeting_interval_seconds",
        value<uint32_t>(&configured.bitcoin.retargeting_interval_seconds),
        "The difficulty retargeting period, defaults to '1209600'."
    )
    (
        "bitcoin.block_spacing_seconds",
        value<uint32_t>(&configured.bitcoin.block_spacing_seconds),
        "The target block period, defaults to '600'."
    )
    (
        "bitcoin.proof_of_work_limit",
        value<uint32_t>(&configured.bitcoin.proof_of_work_limit),
        "The proof of work limit, defaults to '486604799'."
    )
    (
        "bitcoin.genesis_block",
        value<config::block>(&configured.bitcoin.genesis_block),
        "The hexideciaml encoding of the genesis block, defaults to mainnet."
    )
    (
        "bitcoin.checkpoint",
        value<chain::checkpoints>(&configured.bitcoin.checkpoints),
        "The blockchain checkpoints, defaults to the consensus set."
    )
    // [version properties excluded here]
    (
        "bitcoin.bip34_activation_threshold",
        value<size_t>(&configured.bitcoin.bip34_activation_threshold),
        "The number of new version blocks required for bip34 style soft fork activation, defaults to '750'."
    )
    (
        "bitcoin.bip34_enforcement_threshold",
        value<size_t>(&configured.bitcoin.bip34_enforcement_threshold),
        "The number of new version blocks required for bip34 style soft fork enforcement, defaults to '950'."
    )
    (
        "bitcoin.bip34_activation_sample",
        value<size_t>(&configured.bitcoin.bip34_activation_sample),
        "The number of blocks considered for bip34 style soft fork activation, defaults to '1000'."
    )
    (
        "bitcoin.bip65_freeze",
        value<size_t>(&configured.bitcoin.bip90_bip65_height),
        "The block height to freeze the bip65 softfork for bip90, defaults to '388381'."
    )
    (
        "bitcoin.bip66_freeze",
        value<size_t>(&configured.bitcoin.bip90_bip66_height),
        "The block height to freeze the bip66 softfork for bip90, defaults to '363725'."
    )
    (
        "bitcoin.bip34_freeze",
        value<size_t>(&configured.bitcoin.bip90_bip34_height),
        "The block height to freeze the bip34 softfork for bip90, defaults to '227931'."
    )
    (
        "bitcoin.bip16_activation_time",
        value<uint32_t>(&configured.bitcoin.bip16_activation_time),
        "The activation time for bip16 in unix time, defaults to '1333238400'."
    )
    (
        "bitcoin.bip9_bit0_active_checkpoint",
        value<chain::checkpoint>(&configured.bitcoin.bip9_bit0_active_checkpoint),
        "The hash:height checkpoint for bip9 bit0 activation, defaults to '000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5:419328'."
    )
    (
        "bitcoin.milestone",
        value<chain::checkpoint>(&configured.bitcoin.milestone),
        "A block presumed to be valid but not required to be present, defaults to '000000000000000000010538edbfd2d5b809a33dd83f284aeea41c6d0d96968a:900000'."
    )
    (
        "bitcoin.minimum_work",
        value<config::hash256>(&configured.bitcoin.minimum_work),
        "The minimum work for any branch to be considered valid, defaults to '000000000000000000000000000000000000000052b2559353df4117b7348b64'."
    )

    /* [network] */
    (
        "network.threads",
        value<uint32_t>(&configured.network.threads),
        "The minimum number of threads in the network threadpool, defaults to '16'."
    )
    (
        "network.address_upper",
        value<uint16_t>(&configured.network.address_upper),
        "The upper bound for address selection divisor, defaults to '10'."
    )
    (
        "network.address_lower",
        value<uint16_t>(&configured.network.address_lower),
        "The lower bound for address selection divisor, defaults to '5'."
    )
    (
        "network.protocol_maximum",
        value<uint32_t>(&configured.network.protocol_maximum),
        "The maximum network protocol version, defaults to '70012'."
    )
    (
        "network.protocol_minimum",
        value<uint32_t>(&configured.network.protocol_minimum),
        "The minimum network protocol version, defaults to '31800'."
    )
    (
        "network.services_maximum",
        value<uint64_t>(&configured.network.services_maximum),
        "The maximum services exposed by network connections, defaults to '9' (full node, witness)."
    )
    (
        "network.services_minimum",
        value<uint64_t>(&configured.network.services_minimum),
        "The minimum services exposed by network connections, defaults to '9' (full node, witness)."
    )
    (
        "network.invalid_services",
        value<uint64_t>(&configured.network.invalid_services),
        "The advertised services that cause a peer to be dropped, defaults to '176'."
    )
    (
        "network.enable_address",
        value<bool>(&configured.network.enable_address),
        "Enable address messages, defaults to 'true'."
    )
    (
        "network.enable_address_v2",
        value<bool>(&configured.network.enable_address_v2),
        "Enable address v2 messages, defaults to 'false'."
    )
    (
        "network.enable_witness_tx",
        value<bool>(&configured.network.enable_witness_tx),
        "Enable witness transaction identifier relay, defaults to 'false'."
    )
    (
        "network.enable_compact",
        value<bool>(&configured.network.enable_compact),
        "Enable enable compact block messages, defaults to 'false'."
    )
    (
        "network.enable_alert",
        value<bool>(&configured.network.enable_alert),
        "Enable alert messages, defaults to 'false'."
    )
    (
        "network.enable_reject",
        value<bool>(&configured.network.enable_reject),
        "Enable reject messages, defaults to 'false'."
    )
    (
        "network.enable_relay",
        value<bool>(&configured.network.enable_relay),
        "Enable transaction relay, defaults to 'true'."
    )
    (
        "network.validate_checksum",
        value<bool>(&configured.network.validate_checksum),
        "Validate the checksum of network messages, defaults to 'false'."
    )
    (
        "network.identifier",
        value<uint32_t>(&configured.network.identifier),
        "The magic number for message headers, defaults to '3652501241'."
    )
    (
        "network.retry_timeout_seconds",
        value<uint32_t>(&configured.network.retry_timeout_seconds),
        "The time delay for failed connection retry, defaults to '1'."
    )
    (
        "network.connect_timeout_seconds",
        value<uint32_t>(&configured.network.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to '5'."
    )
    (
        "network.handshake_timeout_seconds",
        value<uint32_t>(&configured.network.handshake_timeout_seconds),
        "The time limit to complete the connection handshake, defaults to '15'."
    )
    (
        "network.channel_heartbeat_minutes",
        value<uint32_t>(&configured.network.channel_heartbeat_minutes),
        "The time between ping messages, defaults to '5'."
    )
    (
        "network.maximum_skew_minutes",
        value<uint32_t>(&configured.network.maximum_skew_minutes),
        "The maximum allowable channel clock skew, defaults to '120'."
    )
    (
        "network.rate_limit",
        value<uint32_t>(&configured.network.rate_limit),
        "The peer download rate limit in bytes per second, defaults to 1024 (not implemented)."
    )
    (
        "network.user_agent",
        value<std::string>(&configured.network.user_agent),
        "The node user agent string, defaults to '" BC_USER_AGENT "'."
    )
    (
        "network.path",
        value<std::filesystem::path>(&configured.network.path),
        "The peer address cache file directory, defaults to empty."
    )
    (
        "network.blacklist",
        value<network::config::authorities>(&configured.network.blacklists),
        "IP address to disallow as a peer, multiple allowed."
    )
    (
        "network.whitelist",
        value<network::config::authorities>(&configured.network.whitelists),
        "IP address to allow as a peer, multiple allowed."
    )

    /* [outbound] */
    ////(
    ////    "outbound.secure",
    ////    value<bool>(&configured.network.outbound.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    ////(
    ////    "outbound.bind",
    ////    value<network::config::authorities>(&configured.network.outbound.binds),
    ////    "IP address to bind for load balancing, multiple allowed (not implemented)."
    ////)
    (
        "outbound.connections",
        value<uint16_t>(&configured.network.outbound.connections),
        "The target number of outgoing network connections, defaults to '100'."
    )
    (
        "outbound.inactivity_minutes",
        value<uint32_t>(&configured.network.outbound.inactivity_minutes),
        "The inactivity time limit for any connection, defaults to '10'."
    )
    (
        "outbound.expiration_minutes",
        value<uint32_t>(&configured.network.outbound.expiration_minutes),
        "The age limit for any connection, defaults to '60'."
    )
    (
        "outbound.minimum_buffer",
        value<uint32_t>(&configured.network.outbound.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "outbound.maximum_request",
        value<uint32_t>(&configured.network.outbound.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )
    (
        "outbound.use_ipv6",
        value<bool>(&configured.network.outbound.use_ipv6),
        "Use internet protocol version 6 (IPv6) addresses, defaults to 'false'."
    )
    (
        "outbound.seed",
        value<network::config::endpoints>(&configured.network.outbound.seeds),
        "A seed node for initializing the host pool, multiple allowed."
    )
    (
        "outbound.connect_batch_size",
        value<uint16_t>(&configured.network.outbound.connect_batch_size),
        "The number of concurrent attempts to establish one connection, defaults to '5'."
    )
    (
        "outbound.host_pool_capacity",
        value<uint32_t>(&configured.network.outbound.host_pool_capacity),
        "The maximum number of peer hosts in the pool, defaults to '10000'."
    )
    (
        "outbound.seeding_timeout_seconds",
        value<uint32_t>(&configured.network.outbound.seeding_timeout_seconds),
        "The time limit for obtaining seed connections and addresses, defaults to '30'."
    )
    (
        "outbound.username",
        value<std::string>(&configured.network.outbound.username),
        "The socks5 proxy username (optional)."
    )
    (
        "outbound.password",
        value<std::string>(&configured.network.outbound.password),
        "The socks5 proxy username (optional)."
    )
    (
        "outbound.socks",
        value<config::endpoint>(&configured.network.outbound.socks),
        "The socks5 proxy endpoint (port required)."
    )

    /* [inbound] */
    ////(
    ////    "inbound.secure",
    ////    value<bool>(&configured.network.inbound.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "inbound.bind",
        value<network::config::authorities>(&configured.network.inbound.binds),
        "IP address to bind for listening, multiple allowed, defaults to '0.0.0.0:8333' (all IPv4)."
    )
    (
        "inbound.connections",
        value<uint16_t>(&configured.network.inbound.connections),
        "The target number of incoming network connections, defaults to '100'."
    )
    (
        "inbound.inactivity_minutes",
        value<uint32_t>(&configured.network.inbound.inactivity_minutes),
        "The inactivity time limit for any connection, defaults to '10'."
    )
    (
        "inbound.expiration_minutes",
        value<uint32_t>(&configured.network.inbound.expiration_minutes),
        "The age limit for any connection, defaults to '60'."
    )
    (
        "inbound.minimum_buffer",
        value<uint32_t>(&configured.network.inbound.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "inbound.maximum_request",
        value<uint32_t>(&configured.network.inbound.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )
    (
        "inbound.enable_loopback",
        value<bool>(&configured.network.inbound.enable_loopback),
        "Allow connections from the node to itself, defaults to 'false'."
    )
    (
        "inbound.self",
        value<network::config::authorities>(&configured.network.inbound.selfs),
        "IP address to advertise, multiple allowed."
    )

    /* [manual] */
    ////(
    ////    "manual.secure",
    ////    value<bool>(&configured.network.manual.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    ////(
    ////    "manual.bind",
    ////    value<network::config::authorities>(&configured.network.manual.binds),
    ////    "IP address to bind for load balancing, multiple allowed (not implemented)."
    ////)
    ////(
    ////    "manual.connections",
    ////    value<uint16_t>(&configured.network.manual.connections),
    ////    "The target number of outgoing manual connections (not implemented)."
    ////)
    (
        "manual.inactivity_minutes",
        value<uint32_t>(&configured.network.manual.inactivity_minutes),
        "The inactivity time limit for any connection, defaults to '10' (will attempt reconnect)."
    )
    (
        "manual.expiration_minutes",
        value<uint32_t>(&configured.network.manual.expiration_minutes),
        "The age limit for any connection, defaults to '60' (will attempt reconnect)."
    )
    (
        "manual.minimum_buffer",
        value<uint32_t>(&configured.network.manual.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "manual.maximum_request",
        value<uint32_t>(&configured.network.manual.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )
    (
        "manual.peer",
        value<network::config::endpoints>(&configured.network.manual.peers),
        "A persistent peer node, multiple allowed."
    )
    (
        "manual.username",
        value<std::string>(&configured.network.manual.username),
        "The socks5 proxy username (optional)."
    )
    (
        "manual.password",
        value<std::string>(&configured.network.manual.password),
        "The socks5 proxy username (optional)."
    )
    (
        "manual.socks",
        value<config::endpoint>(&configured.network.manual.socks),
        "The socks5 proxy endpoint (port required)."
    )

    /* [web] */
    ////(
    ////    "web.secure",
    ////    value<bool>(&configured.network.web.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "web.bind",
        value<network::config::authorities>(&configured.server.web.binds),
        "IP address to bind, multiple allowed, defaults to '0.0.0.0:8080' (all IPv4)."
    )
    (
        "web.connections",
        value<uint16_t>(&configured.server.web.connections),
        "The required maximum number of connections, defaults to '0'."
    )
    (
        "web.inactivity_minutes",
        value<uint32_t>(&configured.server.web.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to '10'."
    )
    (
        "web.expiration_minutes",
        value<uint32_t>(&configured.server.web.expiration_minutes),
        "The idle timeout (http keep-alive), defaults to '60'."
    )
    (
        "web.minimum_buffer",
        value<uint32_t>(&configured.server.web.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "web.maximum_request",
        value<uint32_t>(&configured.server.web.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )
    (
        "web.server",
        value<std::string>(&configured.server.web.server),
        "The server name (http header), defaults to '" BC_HTTP_SERVER_NAME "'."
    )
    (
        "web.host",
        value<network::config::endpoints>(&configured.server.web.hosts),
        "The host name (http verification), multiple allowed, defaults to empty (disabled)."
    )
    (
        "web.origin",
        value<network::config::endpoints>(&configured.server.web.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to empty (disabled)."
    )
    (
        "web.allow_opaque_origin",
        value<bool>(&configured.server.web.allow_opaque_origin),
        "Allow requests from opaue origin (see CORS), multiple allowed, defaults to false."
    )
    (
        "web.path",
        value<std::filesystem::path>(&configured.server.web.path),
        "The required root path of source files to be served, defaults to empty."
    )
    (
        "web.default",
        value<std::string>(&configured.server.web.default_),
        "The path of the default source page, defaults to 'index.html'."
    )

    /* [explore] */
    ////(
    ////    "explore.secure",
    ////    value<bool>(&configured.network.explore.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "explore.bind",
        value<network::config::authorities>(&configured.server.explore.binds),
        "IP address to bind, multiple allowed, defaults to '0.0.0.0:8180' (all IPv4)."
    )
    (
        "explore.connections",
        value<uint16_t>(&configured.server.explore.connections),
        "The required maximum number of connections, defaults to '0'."
    )
    (
        "explore.inactivity_minutes",
        value<uint32_t>(&configured.server.explore.inactivity_minutes),
        "The idle timeout (http keep-server), defaults to '60'."
    )
    (
        "explore.expiration_minutes",
        value<uint32_t>(&configured.server.explore.expiration_minutes),
        "The idle timeout (http keep-alive), defaults to '60'."
    )
    (
        "explore.minimum_buffer",
        value<uint32_t>(&configured.server.explore.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "explore.maximum_request",
        value<uint32_t>(&configured.server.explore.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )
    (
        "explore.server",
        value<std::string>(&configured.server.explore.server),
        "The server name (http header), defaults to '" BC_HTTP_SERVER_NAME "'."
    )
    (
        "explore.host",
        value<network::config::endpoints>(&configured.server.explore.hosts),
        "The host name (http verification), multiple allowed, defaults to empty (disabled)."
    )
    (
        "explore.origin",
        value<network::config::endpoints>(&configured.server.explore.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to empty (disabled)."
    )
    (
        "explore.allow_opaque_origin",
        value<bool>(&configured.server.explore.allow_opaque_origin),
        "Allow requests from opaue origin (see CORS), multiple allowed, defaults to false."
    )
    (
        "explore.path",
        value<std::filesystem::path>(&configured.server.explore.path),
        "The required root path of source files to be served, defaults to empty."
    )
    (
        "explore.default",
        value<std::string>(&configured.server.explore.default_),
        "The path of the default source page, defaults to 'index.html'."
    )
    (
        "explore.websocket",
        value<bool>(&configured.server.explore.websocket),
        "Enable websocket interface, defaults to true."
    )

    /* [bitcoind] */
    ////(
    ////    "bitcoind.secure",
    ////    value<bool>(&configured.network.bitcoind.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "bitcoind.bind",
        value<network::config::authorities>(&configured.server.bitcoind.binds),
        "IP address to bind, multiple allowed, defaults to '0.0.0.0:8380' (all IPv4)."
    )
    (
        "bitcoind.connections",
        value<uint16_t>(&configured.server.bitcoind.connections),
        "The required maximum number of connections, defaults to '0'."
    )
    (
        "bitcoind.inactivity_minutes",
        value<uint32_t>(&configured.server.bitcoind.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to '10'."
    )
    (
        "bitcoind.expiration_minutes",
        value<uint32_t>(&configured.server.bitcoind.expiration_minutes),
        "The idle timeout (http keep-alive), defaults to '60'."
    )
    (
        "bitcoind.minimum_buffer",
        value<uint32_t>(&configured.server.bitcoind.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "bitcoind.maximum_request",
        value<uint32_t>(&configured.server.bitcoind.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )
    (
        "bitcoind.server",
        value<std::string>(&configured.server.bitcoind.server),
        "The server name (http header), defaults to '" BC_HTTP_SERVER_NAME "'."
    )
    (
        "bitcoind.host",
        value<network::config::endpoints>(&configured.server.bitcoind.hosts),
        "The host name (http verification), multiple allowed, defaults to empty (disabled)."
    )
    (
        "bitcoind.origin",
        value<network::config::endpoints>(&configured.server.bitcoind.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to empty (disabled)."
    )
    (
        "bitcoind.allow_opaque_origin",
        value<bool>(&configured.server.bitcoind.allow_opaque_origin),
        "Allow requests from opaue origin (see CORS), multiple allowed, defaults to false."
    )

    /* [electrum] */
    ////(
    ////    "electrum.secure",
    ////    value<bool>(&configured.network.electrum.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "electrum.bind",
        value<network::config::authorities>(&configured.server.electrum.binds),
        "IP address to bind, multiple allowed, defaults to '0.0.0.0:8480' (all IPv4)."
    )
    (
        "electrum.connections",
        value<uint16_t>(&configured.server.electrum.connections),
        "The required maximum number of connections, defaults to '0'."
    )
    (
        "electrum.inactivity_minutes",
        value<uint32_t>(&configured.server.electrum.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to '10'."
    )
    (
        "electrum.expiration_minutes",
        value<uint32_t>(&configured.server.electrum.expiration_minutes),
        "The idle timeout (http keep-alive), defaults to '60'."
    )
    (
        "electrum.minimum_buffer",
        value<uint32_t>(&configured.server.electrum.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "electrum.maximum_request",
        value<uint32_t>(&configured.server.electrum.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )

    /* [stratum_v1] */
    ////(
    ////    "stratum_v1.secure",
    ////    value<bool>(&configured.network.stratum_v1.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "stratum_v1.bind",
        value<network::config::authorities>(&configured.server.stratum_v1.binds),
        "IP address to bind, multiple allowed, defaults to '0.0.0.0:8580' (all IPv4)."
    )
    (
        "stratum_v1.connections",
        value<uint16_t>(&configured.server.stratum_v1.connections),
        "The required maximum number of connections, defaults to '0'."
    )
    (
        "stratum_v1.inactivity_minutes",
        value<uint32_t>(&configured.server.stratum_v1.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to '10'."
    )
    (
        "stratum_v1.expiration_minutes",
        value<uint32_t>(&configured.server.stratum_v1.expiration_minutes),
        "The idle timeout (http keep-alive), defaults to '60'."
    )
    (
        "stratum_v1.minimum_buffer",
        value<uint32_t>(&configured.server.stratum_v1.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "stratum_v1.maximum_request",
        value<uint32_t>(&configured.server.stratum_v1.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )

    /* [stratum_v2] */
    ////(
    ////    "stratum_v2.secure",
    ////    value<bool>(&configured.network.stratum_v2.secure),
    ////    "Require transport layer security, defaults to 'false' (not implemented)."
    ////)
    (
        "stratum_v2.bind",
        value<network::config::authorities>(&configured.server.stratum_v2.binds),
        "IP address to bind, multiple allowed, defaults to '0.0.0.0:8680' (all IPv4)."
    )
    (
        "stratum_v2.connections",
        value<uint16_t>(&configured.server.stratum_v2.connections),
        "The required maximum number of connections, defaults to '0'."
    )
    (
        "stratum_v2.inactivity_minutes",
        value<uint32_t>(&configured.server.stratum_v2.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to '10'."
    )
    (
        "stratum_v2.expiration_minutes",
        value<uint32_t>(&configured.server.stratum_v2.expiration_minutes),
        "The idle timeout (http keep-alive), defaults to '60'."
    )
    (
        "stratum_v2.minimum_buffer",
        value<uint32_t>(&configured.server.stratum_v2.minimum_buffer),
        "The minimum retained read buffer size, defaults to '4000000'."
    )
    (
        "stratum_v2.maximum_request",
        value<uint32_t>(&configured.server.stratum_v2.maximum_request),
        "The maximum allowed request size, defaults to '4000000'."
    )

    /* [node] */
    (
        "node.threads",
        value<uint32_t>(&configured.node.threads),
        "The number of threads in the validation threadpool, defaults to '32'."
    )
    (
        "node.thread_priority",
        value<bool>(&configured.node.thread_priority),
        "Set validation threads to high processing priority, defaults to 'true'."
    )
    (
        "node.memory_priority",
        value<bool>(&configured.node.memory_priority),
        "Set the process to high memory priority, defaults to 'true'."
    )
    (
        "node.delay_inbound",
        value<bool>(&configured.node.delay_inbound),
        "Delay accepting inbound connections until node is current, defaults to 'true'."
    )
    (
        "node.defer_validation",
        value<bool>(&configured.node.defer_validation),
        "Defer validation, defaults to 'false'."
    )
    (
        "node.defer_confirmation",
        value<bool>(&configured.node.defer_confirmation),
        "Defer confirmation, defaults to 'false'."
    )
    ////(
    ////    "node.headers_first",
    ////    value<bool>(&configured.node.headers_first),
    ////    "Obtain current header chain before obtaining associated blocks, defaults to 'true'."
    ////)
    (
        "node.allowed_deviation",
        value<float>(&configured.node.allowed_deviation),
        "Allowable underperformance standard deviation, defaults to '1.5' (0 disables)."
    )
    (
        "node.announcement_cache",
        value<uint16_t>(&configured.node.announcement_cache),
        "Limit of per channel cached peer block and tx announcements, to avoid replaying, defaults to '42'."
    )
    (
        "node.allocation_multiple",
        value<uint16_t>(&configured.node.allocation_multiple),
        "Block deserialization buffer multiple of wire size, defaults to '20' (0 disables)."
    )
    (
        "node.maximum_height",
        value<uint32_t>(&configured.node.maximum_height),
        "Maximum block height to populate, defaults to 0 (unlimited)."
    )
    (
        "node.maximum_concurrency",
        value<uint32_t>(&configured.node.maximum_concurrency),
        "Maximum number of blocks to download concurrently, defaults to '50000' (0 disables)."
    )
    ////(
    ////    "node.snapshot_bytes",
    ////    value<uint64_t>(&configured.node.snapshot_bytes),
    ////    "Downloaded bytes that triggers snapshot, defaults to '0' (0 disables)."
    ////)
    ////(
    ////    "node.snapshot_valid",
    ////    value<uint32_t>(&configured.node.snapshot_valid),
    ////    "Completed validations that trigger snapshot, defaults to '0' (0 disables)."
    ////)
    ////(
    ////    "node.snapshot_confirm",
    ////    value<uint32_t>(&configured.node.snapshot_confirm),
    ////    "Completed confirmations that trigger snapshot, defaults to '0' (0 disables)."
    ////)
    (
        "node.sample_period_seconds",
        value<uint16_t>(&configured.node.sample_period_seconds),
        "Sampling period for drop of stalled channels, defaults to '10' (0 disables)."
    )
    (
        "node.currency_window_minutes",
        value<uint32_t>(&configured.node.currency_window_minutes),
        "Time from present that blocks are considered current, defaults to '60' (0 disables)."
    )
    // #######################
    ////(
    ////    "node.notify_limit_hours",
    ////    value<uint32_t>(&configured.node.notify_limit_hours),
    ////    "Disable relay when top block age exceeds, defaults to '24' (0 disables)."
    ////)
    ////(
    ////    "node.byte_fee_satoshis",
    ////    value<float>(&configured.node.byte_fee_satoshis),
    ////    "The minimum fee per byte, cumulative for conflicts, defaults to '1'."
    ////)
    ////(
    ////    "node.sigop_fee_satoshis",
    ////    value<float>(&configured.node.sigop_fee_satoshis),
    ////    "The minimum fee per sigop, additional to byte fee, defaults to' 100'."
    ////)
    ////(
    ////    "node.minimum_output_satoshis",
    ////    value<uint64_t>(&configured.node.minimum_output_satoshis),
    ////    "The minimum output value, defaults to '500'."
    ////)

    /* [database] */
    (
        "database.path",
        value<std::filesystem::path>(&configured.database.path),
        "The blockchain database directory, defaults to 'blockchain'."
    )
    (
        "database.turbo",
        value<bool>(&configured.database.turbo),
        "Allow indiviudal non-validation queries to use all CPUs, defaults to false."
    )

    /* header */
    (
        "database.header_buckets",
        value<uint32_t>(&configured.database.header_buckets),
        "The number of buckets in the header table head, defaults to '386364'."
    )
    (
        "database.header_size",
        value<uint64_t>(&configured.database.header_size),
        "The minimum allocation of the header table body, defaults to '21000000'."
    )
    (
        "database.header_rate",
        value<uint16_t>(&configured.database.header_rate),
        "The percentage expansion of the header table body, defaults to '5'."
    )

    /* input */
    (
        "database.input_size",
        value<uint64_t>(&configured.database.input_size),
        "The minimum allocation of the input table body, defaults to '92500000000'."
    )
    (
        "database.input_rate",
        value<uint16_t>(&configured.database.input_rate),
        "The percentage expansion of the input table body, defaults to '5'."
    )

    /* output */
    (
        "database.output_size",
        value<uint64_t>(&configured.database.output_size),
        "The minimum allocation of the output table body, defaults to '25300000000'."
    )
    (
        "database.output_rate",
        value<uint16_t>(&configured.database.output_rate),
        "The percentage expansion of the output table body, defaults to '5'."
    )

    /* point */
    (
        "database.point_buckets",
        value<uint32_t>(&configured.database.point_buckets),
        "The number of buckets in the spend table head, defaults to '1365977136'."
    )
    (
        "database.point_size",
        value<uint64_t>(&configured.database.point_size),
        "The minimum allocation of the point table body, defaults to '25700000000'."
    )
    (
        "database.point_rate",
        value<uint16_t>(&configured.database.point_rate),
        "The percentage expansion of the point table body, defaults to '5'."
    )

    /* ins */
    (
        "database.ins_size",
        value<uint64_t>(&configured.database.ins_size),
        "The minimum allocation of the point table body, defaults to '8550000000'."
    )
    (
        "database.ins_rate",
        value<uint16_t>(&configured.database.ins_rate),
        "The percentage expansion of the ins table body, defaults to '5'."
    )

    /* outs */
    (
        "database.outs_size",
        value<uint64_t>(&configured.database.outs_size),
        "The minimum allocation of the puts table body, defaults to '3700000000'."
    )
    (
        "database.outs_rate",
        value<uint16_t>(&configured.database.outs_rate),
        "The percentage expansion of the puts table body, defaults to '5'."
    )

    /* tx */
    (
        "database.tx_buckets",
        value<uint32_t>(&configured.database.tx_buckets),
        "The number of buckets in the tx table head, defaults to '469222525'."
    )
    (
        "database.tx_size",
        value<uint64_t>(&configured.database.tx_size),
        "The minimum allocation of the tx table body, defaults to '17000000000'."
    )
    (
        "database.tx_rate",
        value<uint16_t>(&configured.database.tx_rate),
        "The percentage expansion of the tx table body, defaults to '5'."
    )

    /* txs */
    (
        "database.txs_buckets",
        value<uint32_t>(&configured.database.txs_buckets),
        "The number of buckets in the txs table head, defaults to '900001'."
    )
    (
        "database.txs_size",
        value<uint64_t>(&configured.database.txs_size),
        "The minimum allocation of the txs table body, defaults to '1050000000'."
    )
    (
        "database.txs_rate",
        value<uint16_t>(&configured.database.txs_rate),
        "The percentage expansion of the txs table body, defaults to '5'."
    )

    /* candidate */
    (
        "database.candidate_size",
        value<uint64_t>(&configured.database.candidate_size),
        "The minimum allocation of the candidate table body, defaults to '2575500'."
    )
    (
        "database.candidate_rate",
        value<uint16_t>(&configured.database.candidate_rate),
        "The percentage expansion of the candidate table body, defaults to '5'."
    )

    /* confirmed */
    (
        "database.confirmed_size",
        value<uint64_t>(&configured.database.confirmed_size),
        "The minimum allocation of the candidate table body, defaults to '2575500'."
    )
    (
        "database.confirmed_rate",
        value<uint16_t>(&configured.database.confirmed_rate),
        "The percentage expansion of the candidate table body, defaults to '5'."
    )

    /* strong_tx */
    (
        "database.strong_tx_buckets",
        value<uint32_t>(&configured.database.strong_tx_buckets),
        "The number of buckets in the strong_tx table head, defaults to '469222525'."
    )
    (
        "database.strong_tx_size",
        value<uint64_t>(&configured.database.strong_tx_size),
        "The minimum allocation of the strong_tx table body, defaults to '2900000000'."
    )
    (
        "database.strong_tx_rate",
        value<uint16_t>(&configured.database.strong_tx_rate),
        "The percentage expansion of the strong_tx table body, defaults to '5'."
    )

    /* duplicate */
    (
        "database.duplicate_buckets",
        value<uint16_t>(&configured.database.duplicate_buckets),
        "The minimum number of buckets in the duplicate table head, defaults to '1024'."
    )
    (
        "database.duplicate_size",
        value<uint64_t>(&configured.database.duplicate_size),
        "The minimum allocation of the duplicate table body, defaults to '44'."
    )
    (
        "database.duplicate_rate",
        value<uint16_t>(&configured.database.duplicate_rate),
        "The percentage expansion of the duplicate table, defaults to '5'."
    )

    /* prevout */
    (
        "database.prevout_buckets",
        value<uint32_t>(&configured.database.prevout_buckets),
        "The minimum number of buckets in the prevout table head, defaults to '0'."
    )
    (
        "database.prevout_size",
        value<uint64_t>(&configured.database.prevout_size),
        "The minimum allocation of the prevout table body, defaults to '1'."
    )
    (
        "database.prevout_rate",
        value<uint16_t>(&configured.database.prevout_rate),
        "The percentage expansion of the prevout table, defaults to '5'."
    )

    /* validated_bk */
    (
        "database.validated_bk_buckets",
        value<uint32_t>(&configured.database.validated_bk_buckets),
        "The number of buckets in the validated_bk table head, defaults to '900001'."
    )
    (
        "database.validated_bk_size",
        value<uint64_t>(&configured.database.validated_bk_size),
        "The minimum allocation of the validated_bk table body, defaults to '1700000'."
    )
    (
        "database.validated_bk_rate",
        value<uint16_t>(&configured.database.validated_bk_rate),
        "The percentage expansion of the validated_bk table body, defaults to '5'."
    )

    /* validated_tx */
    (
        "database.validated_tx_buckets",
        value<uint32_t>(&configured.database.validated_tx_buckets),
        "The number of buckets in the validated_tx table head, defaults to '1'."
    )
    (
        "database.validated_tx_size",
        value<uint64_t>(&configured.database.validated_tx_size),
        "The minimum allocation of the validated_tx table body, defaults to '1'."
    )
    (
        "database.validated_tx_rate",
        value<uint16_t>(&configured.database.validated_tx_rate),
        "The percentage expansion of the validated_tx table body, defaults to '5'."
    )

    /* address */
    (
        "database.address_buckets",
        value<uint32_t>(&configured.database.address_buckets),
        "The number of buckets in the address table head, defaults to '1' (0|1 disables)."
    )
    (
        "database.address_size",
        value<uint64_t>(&configured.database.address_size),
        "The minimum allocation of the address table body, defaults to '1'."
    )
    (
        "database.address_rate",
        value<uint16_t>(&configured.database.address_rate),
        "The percentage expansion of the address table body, defaults to '5'."
    )

    /* filter_bk */
    (
        "database.filter_bk_buckets",
        value<uint32_t>(&configured.database.filter_bk_buckets),
        "The number of buckets in the filter_bk table head, defaults to '0' (0 disables)."
    )
    (
        "database.filter_bk_size",
        value<uint64_t>(&configured.database.filter_bk_size),
        "The minimum allocation of the filter_bk table body, defaults to '1'."
    )
    (
        "database.filter_bk_rate",
        value<uint16_t>(&configured.database.filter_bk_rate),
        "The percentage expansion of the filter_bk table body, defaults to '5'."
    )

    /* filter_tx */
    (
        "database.filter_tx_buckets",
        value<uint32_t>(&configured.database.filter_tx_buckets),
        "The number of buckets in the filter_tx table head, defaults to '0' (0 disables)."
    )
    (
        "database.filter_tx_size",
        value<uint64_t>(&configured.database.filter_tx_size),
        "The minimum allocation of the filter_tx table body, defaults to '1'."
    )
    (
        "database.filter_tx_rate",
        value<uint16_t>(&configured.database.filter_tx_rate),
        "The percentage expansion of the filter_tx table body, defaults to '5'."
    )

    /* [log] */
#if defined(HAVE_LOGA)
    (
        "log.application",
        value<bool>(&configured.log.application),
        "Enable application logging, defaults to 'true'."
    )
#endif
#if defined(HAVE_LOGN)
    (
        "log.news",
        value<bool>(&configured.log.news),
        "Enable news logging, defaults to 'true'."
    )
#endif
#if defined(HAVE_LOGS)
    (
        "log.session",
        value<bool>(&configured.log.session),
        "Enable session logging, defaults to 'true'."
    )
#endif
#if defined(HAVE_LOGP)
    (
        "log.protocol",
        value<bool>(&configured.log.protocol),
        "Enable protocol logging, defaults to 'false'."
    )
#endif
#if defined(HAVE_LOGX)
    (
        "log.proxy",
        value<bool>(&configured.log.proxy),
        "Enable proxy logging, defaults to 'false'."
    )
#endif
#if defined(HAVE_LOGR)
    (
        "log.remote",
        value<bool>(&configured.log.remote),
        "Enable remote fault logging, defaults to 'true'."
    )
#endif
#if defined(HAVE_LOGF)
    (
        "log.fault",
        value<bool>(&configured.log.fault),
        "Enable local fault logging, defaults to 'true'."
    )
#endif
#if defined(HAVE_LOGQ)
    (
        "log.quitting",
        value<bool>(&configured.log.quitting),
        "Enable quitting logging, defaults to 'false'."
    )
#endif
#if defined(HAVE_LOGO)
    (
        "log.objects",
        value<bool>(&configured.log.objects),
        "Enable objects logging, defaults to 'false'."
    )
#endif
#if defined(HAVE_LOGV)
    (
        "log.verbose",
        value<bool>(&configured.log.verbose),
        "Enable verbose logging, defaults to 'false'."
    )
#endif
    (
        "log.maximum_size",
        value<uint32_t>(&configured.log.maximum_size),
        "The maximum byte size of each pair of rotated log files, defaults to 1000000."
    )
#if defined (HAVE_MSC)
    (
        "log.symbols",
        value<std::filesystem::path>(&configured.log.symbols),
        "Path to windows debug build symbols file (.pdb)."
    )
#endif
    (
        "log.path",
        value<std::filesystem::path>(&configured.log.path),
        "The log files directory, defaults to empty."
    );

    return description;
}

bool parser::parse(int argc, const char* argv[], std::ostream& error) THROWS
{
    try
    {
        auto file = false;
        variables_map variables;
        load_command_variables(variables, argc, argv);
        load_environment_variables(variables, BS_ENVIRONMENT_VARIABLE_PREFIX);

        // Don't load config file if any of these options are specified.
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
