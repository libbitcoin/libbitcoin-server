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
#include <bitcoin/server/parser.hpp>

#include <filesystem>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

////std::filesystem::path config_default_path() NOEXCEPT
////{
////    return { "libbitcoin/bn.cfg" };
////}

namespace libbitcoin {
namespace server {

using namespace bc::system;
using namespace bc::system::config;
namespace table = bc::database::table;
using namespace boost::program_options;

// Initialize configuration using defaults of the given context.
parser::parser(system::chain::selection context,
    const server::settings::embedded_pages& native,
    const server::settings::embedded_pages& admin) NOEXCEPT
  : configured(context, native, admin)
{
    // network

    using level = network::messages::peer::level;

    configured.network.enable_relay = true;
    configured.network.enable_address = true;
    configured.network.enable_not_found = true;
    configured.network.enable_address_v2 = true;
    configured.network.enable_witness_tx = false;
    configured.network.enable_compact = true;
    configured.network.outbound.host_pool_capacity = 10000;
    configured.network.outbound.connections = 100;
    configured.network.inbound.connections = 100;
    configured.network.maximum_skew_minutes = 120;
    configured.network.protocol_minimum = level::headers_protocol;
    configured.network.protocol_maximum = level::maximum_protocol;

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

    // server

    // Admin may accumulate a large amount of log backlog.
    configured.server.admin.maximum_backlog = 10 * network::megabyte;

    // The largest btcd notification is a filtered block.
    configured.server.btcd.maximum_backlog = 20 * network::megabyte;

    // Raw block publications drop beyond the bound, eight blocks deep.
    configured.server.bitcoind_zmq.maximum_backlog = 32 * network::megabyte;

    // A submitted block is base16 encoded within a json envelope.
    configured.server.bitcoind.maximum_request = 2 * network::max_payload + 4 * network::kilobyte;
    configured.server.btcd.maximum_request = 2 * network::max_payload + 4 * network::kilobyte;

    // A broadcast transaction package is base16 encoded within a json envelope.
    configured.server.electrum.maximum_request = 2 * network::max_payload + 4 * network::kilobyte;
    configured.server.sparrow.maximum_request = 2 * network::max_payload + 4 * network::kilobyte;
    configured.server.esplora.maximum_request = 2 * network::max_payload + 4 * network::kilobyte;

    ////configured.server.admin.binds.emplace_back(asio::address{}, 8080_u16);
    ////configured.server.admin.safes.emplace_back(asio::address{}, 8043_u16);
    ////configured.server.native.binds.emplace_back(asio::address{}, 8180_u16);
    ////configured.server.native.safes.emplace_back(asio::address{}, 8143_u16);
    ////configured.server.bitcoind.binds.emplace_back(asio::address{}, 8280_u16);
    ////configured.server.bitcoind.safes.emplace_back(asio::address{}, 8243_u16);
    ////configured.server.electrum.binds.emplace_back(asio::address{}, 8380_u16);
    ////configured.server.electrum.safes.emplace_back(asio::address{}, 8343_u16);
    ////configured.server.stratum_v1.binds.emplace_back(asio::address{}, 8480_u16);
    ////configured.server.stratum_v1.safes.emplace_back(asio::address{}, 8443_u16);
    ////configured.server.stratum_v2.binds.emplace_back(asio::address{}, 8580_u16);
    ////configured.server.bitcoind_zmq.binds.emplace_back(asio::address{}, 8680_u16);

    // node

    configured.node.minimum_fee_rate = 0.000001;
    configured.node.batch_signatures = 1'000'000;

    // database

    configured.database.turbo = true;

    // expected are element counts @ 950K, deriving filter k at create.
    // sizes are set to 1% of measured pruned body @ 950K (tiny tables 100%).

    // Only used for electrum queries (255 is optimal otherwise).
    configured.database.interval_depth = 11;

    // archive

    configured.database.header.expected = 962'953;
    configured.database.header.size = 124'220'679;
    configured.database.header.rate = 1;

    configured.database.txs.buckets = 950'001;
    configured.database.txs.size = 54'471'017;
    configured.database.txs.rate = 1;

    configured.database.tx.expected = 1'359'871'695;
    configured.database.tx.size = 870'317'885;
    configured.database.tx.rate = 1;

    // ins (required)
    configured.database.ins.expected = 3'363'467'251;
    configured.database.ins.size = 1'749'002'971;
    configured.database.ins.rate = 1;

    // outs (optional, disabled by a configured bucket count of zero)
    configured.database.outs.expected = 3'741'929'086;
    configured.database.outs.size = 336'773'618;
    configured.database.outs.rate = 1;

    configured.database.input.size = 67'269'346;
    configured.database.input.rate = 1;
    configured.database.output.size = 1'278'023'220;
    configured.database.output.rate = 1;

    // indexes

    configured.database.candidate.buckets = 950'001;
    configured.database.candidate.rate = 1;
    configured.database.confirmed.buckets = 950'001;
    configured.database.confirmed.rate = 1;

    configured.database.strong_tx.expected = 1'359'871'695;
    configured.database.strong_tx.size = 149'585'887;
    configured.database.strong_tx.rate = 1;
    configured.database.scan.size = 0;
    configured.database.scan.rate = 1;

    // caches

    configured.database.ecdsa.size = 0;
    configured.database.ecdsa.rate = 1;
    configured.database.schnorr.size = 0;
    configured.database.schnorr.rate = 1;
    configured.database.silent.size = 0;
    configured.database.silent.rate = 1;
    configured.database.prevalid.size = 0;
    configured.database.prevalid.rate = 1;

    configured.database.prevout.buckets = 0;
    configured.database.prevout.size = 0;
    configured.database.prevout.rate = 1;

    configured.database.duplicate.buckets = 1024;
    configured.database.duplicate.expected = 0;
    configured.database.duplicate.size = 0;
    configured.database.duplicate.rate = 1;

    configured.database.state.buckets = 950'001;
    configured.database.state.size = 0;
    configured.database.state.rate = 1;

    // pool and spends (disabled by zero buckets)
    configured.database.pool.expected = 10'000'000;
    configured.database.pool.size = 0;
    configured.database.pool.rate = 1;
    configured.database.spends.size = 0;
    configured.database.spends.rate = 1;

    // optional

    // also disabled by filter_tx
    configured.database.filter_bk.buckets = 0;
    configured.database.filter_bk.size = 0;
    configured.database.filter_bk.rate = 1;

    // also disabled by filter_bk
    configured.database.filter_tx.buckets = 0;
    configured.database.filter_tx.size = 0;
    configured.database.filter_tx.rate = 1;
}

// Hashmap buckets derive from expected and installed memory; a configured
// count governs, and zero disables an optional table.
void parser::derive_buckets() NOEXCEPT
{
    constexpr uint32_t contested = 75;
    constexpr uint32_t target = 25;
    constexpr uint32_t address_contested = 500;
    constexpr uint32_t address_target = 50;
    auto& database = configured.database;

    if (!is_configured("table.header.buckets"))
        database.header.buckets = table::header::derive_buckets(
            database.header.expected, contested, target);

    if (!is_configured("table.tx.buckets"))
        database.tx.buckets = table::transaction::derive_buckets(
            database.tx.expected, contested, target);

    if (!is_configured("table.ins.buckets"))
        database.ins.buckets = table::ins::derive_buckets(
            database.ins.expected, contested, target);

    if (!is_configured("table.outs.buckets"))
        database.outs.buckets = table::outs::derive_buckets(
            database.outs.expected, address_contested, address_target);

    if (!is_configured("table.strong.buckets"))
        database.strong_tx.buckets = table::strong_tx::derive_buckets(
            database.strong_tx.expected, contested, target);

    if (!is_configured("table.pool.buckets"))
        database.pool.buckets = table::pool::derive_buckets(
            database.pool.expected, contested, target);
}

// Composes a variable name with its command line shortcut.
static std::string alias(const std::string& variable, char shortcut)
{
    return variable + ',' + shortcut;
}

options_metadata parser::load_options() THROWS
{
    options_metadata description("options");
    description.add_options()
    (
        alias(config_variable, 'c').c_str(),
        value<config::path>(&configured.file),
        "Specify path to a configuration settings file."
    )
    // Prompts.
    (
        alias(accept_variable, 'a').c_str(),
        value<bool>(&configured.accept)->
            default_value(false)->zero_tokens(),
        "Accept startup prompts without interaction."
    )
    // Information.
    (
        alias(help_variable, 'h').c_str(),
        value<bool>(&configured.help)->
            default_value(false)->zero_tokens(),
        "Display command line options."
    )
    (
        alias(hardware_variable, 'w').c_str(),
        value<bool>(&configured.hardware)->
            default_value(false)->zero_tokens(),
        "Display hardware compatibility."
    )
    (
        alias(settings_variable, 's').c_str(),
        value<bool>(&configured.settings)->
            default_value(false)->zero_tokens(),
        "Display all configuration settings."
    )
    (
        alias(version_variable, 'v').c_str(),
        value<bool>(&configured.version)->
            default_value(false)->zero_tokens(),
        "Display version information."
    )
    // Actions.
    (
        alias(newstore_variable, 'n').c_str(),
        value<bool>(&configured.newstore)->
            default_value(false)->zero_tokens(),
        "Create new store in configured directory."
    )
    (
        alias(backup_variable, 'b').c_str(),
        value<bool>(&configured.backup)->
            default_value(false)->zero_tokens(),
        "Backup to a snapshot (can also do live)."
    )
    (
        alias(restore_variable, 'r').c_str(),
        value<bool>(&configured.restore)->
            default_value(false)->zero_tokens(),
        "Restore from most recent snapshot."
    )
    // Service.
    (
        alias(daemon_variable, 'd').c_str(),
        value<bool>()->implicit_value(true)->
            notifier([&](bool value) { configured.daemon = value; }),
        "Install ('true') or uninstall ('false') as a daemon."
    )
    (
        alias(user_variable, 'u').c_str(),
        value<network::config::credential>()->
            notifier([&](const network::config::credential& value)
                { configured.user = value; }),
        "Daemon logon credential, defaults to system account."
    )
    // Chain scans.
    (
        alias(flags_variable, 'f').c_str(),
        value<bool>(&configured.flags)->
            default_value(false)->zero_tokens(),
        "Scan and display all flag transitions."
    )
    (
        alias(buckets_variable, 'k').c_str(),
        value<bool>(&configured.buckets)->
            default_value(false)->zero_tokens(),
        "Scan and display all bucket densities."
    )
    (
        alias(collisions_variable, 'l').c_str(),
        value<bool>(&configured.collisions)->
            default_value(false)->zero_tokens(),
        "Scan and show hashmap collision stats (may SIGKILL)."
    )
    (
        alias(information_variable, 'i').c_str(),
        value<bool>(&configured.information)->
            default_value(false)->zero_tokens(),
        "Scan and display store information."
    )
    // Ad-hoc Testing.
    (
        alias(get_variable, 'g').c_str(),
        value<config::hash256>(&configured.get)->
            default_value(system::null_hash),
        "Run built-in read test and display."
    )
    (
        alias(put_variable, 'p').c_str(),
        value<config::hash256>(&configured.put)->
            default_value(system::null_hash),
        "Run built-in write test and display."
    );

    return description;
}

arguments_metadata parser::load_arguments() THROWS
{
    // There are no positional arguments, the config path requires --config.
    return arguments_metadata{};
}

options_metadata parser::load_environment() THROWS
{
    options_metadata description("environment");
    description.add_options()
    (
        // For some reason po requires this to be a lower case name.
        // The case must match the other declarations for it to compose.
        // This composes with the cmdline options and inits to default path.
        config_variable,
        value<config::path>(&configured.file)->composing()
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
        setting<bool>(&configured.bitcoin.forks.difficult),
        "Require difficult blocks, defaults to {} (use false for testnet)."
    )
    (
        "forks.retarget",
        setting<bool>(&configured.bitcoin.forks.retarget),
        "Retarget difficulty, defaults to {} (use false for regtest)."
    )
    (
        "forks.bip16",
        setting<bool>(&configured.bitcoin.forks.bip16),
        "Add pay-to-script-hash processing, defaults to {} (soft fork)."
    )
    (
        "forks.bip30",
        setting<bool>(&configured.bitcoin.forks.bip30),
        "Disallow collision of unspent transaction hashes, defaults to {} (soft fork)."
    )
    (
        "forks.bip34",
        setting<bool>(&configured.bitcoin.forks.bip34),
        "Require coinbase input includes block height, defaults to {} (soft fork)."
    )
    (
        "forks.bip42",
        setting<bool>(&configured.bitcoin.forks.bip42),
        "Finite monetary supply, defaults to {} (soft fork)."
    )
    (
        "forks.bip66",
        setting<bool>(&configured.bitcoin.forks.bip66),
        "Require strict signature encoding, defaults to {} (soft fork)."
    )
    (
        "forks.bip65",
        setting<bool>(&configured.bitcoin.forks.bip65),
        "Add check-locktime-verify op code, defaults to {} (soft fork)."
    )
    (
        "forks.bip90",
        setting<bool>(&configured.bitcoin.forks.bip90),
        "Assume bip34, bip65, and bip66 activation if enabled, defaults to {} (hard fork)."
    )
    (
        "forks.bip68",
        setting<bool>(&configured.bitcoin.forks.bip68),
        "Add relative locktime enforcement, defaults to {} (soft fork)."
    )
    (
        "forks.bip112",
        setting<bool>(&configured.bitcoin.forks.bip112),
        "Add check-sequence-verify op code, defaults to {} (soft fork)."
    )
    (
        "forks.bip113",
        setting<bool>(&configured.bitcoin.forks.bip113),
        "Use median time past for locktime, defaults to {} (soft fork)."
    )
    (
        "forks.bip141",
        setting<bool>(&configured.bitcoin.forks.bip141),
        "Segregated witness consensus layer, defaults to {} (soft fork)."
    )
    (
        "forks.bip143",
        setting<bool>(&configured.bitcoin.forks.bip143),
        "Witness version 0 (segwit), defaults to {} (soft fork)."
    )
    (
        "forks.bip147",
        setting<bool>(&configured.bitcoin.forks.bip147),
        "Prevent dummy value malleability, defaults to {} (soft fork)."
    )
    (
        "forks.bip341",
        setting<bool>(&configured.bitcoin.forks.bip341),
        "Witness version 1 (taproot), defaults to {} (soft fork)."
    )
    (
        "forks.bip342",
        setting<bool>(&configured.bitcoin.forks.bip342),
        "Validation of taproot script, defaults to {} (soft fork)."
    )
    (
        "forks.time_warp_patch",
        setting<bool>(&configured.bitcoin.forks.time_warp_patch),
        "Assume time_warp_patch activation if enabled, defaults to {} (testnet4)."
    )
    (
        "forks.block_storm_patch",
        setting<bool>(&configured.bitcoin.forks.block_storm_patch),
        "Assume block_storm_patch activation if enabled, defaults to {} (testnet4)."
    )
    (
        "forks.ltc_time_warp_patch",
        setting<bool>(&configured.bitcoin.forks.ltc_time_warp_patch),
        "Fix time warp bug, defaults to {} (litecoin)."
    )
    (
        "forks.ltc_retarget_overflow_patch",
        setting<bool>(&configured.bitcoin.forks.ltc_retarget_overflow_patch),
        "Fix target overflow for very low difficulty, defaults to {} (litecoin)."
    )
    (
        "forks.ltc_scrypt_proof_of_work",
        setting<bool>(&configured.bitcoin.forks.ltc_scrypt_proof_of_work),
        "Use scrypt hashing for proof of work, defaults to {} (litecoin)."
    )

    /* [bitcoin] */
    (
        "bitcoin.initial_block_subsidy_bitcoin",
        setting<uint64_t>(&configured.bitcoin.initial_subsidy_bitcoin),
        "The initial block subsidy, defaults to {}."
    )
    (
        "bitcoin.subsidy_interval",
        setting<uint32_t>(&configured.bitcoin.subsidy_interval_blocks),
        "The subsidy halving period, defaults to {}."
    )
    (
        "bitcoin.timestamp_limit_seconds",
        setting<uint32_t>(&configured.bitcoin.timestamp_limit_seconds),
        "The future timestamp allowance, defaults to {}."
    )
    (
        "bitcoin.retargeting_factor",
        setting<uint32_t>(&configured.bitcoin.retargeting_factor),
        "The difficulty retargeting factor, defaults to {}."
    )
    (
        "bitcoin.retargeting_interval_seconds",
        setting<uint32_t>(&configured.bitcoin.retargeting_interval_seconds),
        "The difficulty retargeting period, defaults to {}."
    )
    (
        "bitcoin.block_spacing_seconds",
        setting<uint32_t>(&configured.bitcoin.block_spacing_seconds),
        "The target block period, defaults to {}."
    )
    (
        "bitcoin.proof_of_work_limit",
        setting<uint32_t>(&configured.bitcoin.proof_of_work_limit),
        "The proof of work limit, defaults to {}."
    )
    (
        "bitcoin.genesis_block",
        setting<config::block>(&configured.bitcoin.genesis_block),
        "The hexadecimal encoding of the genesis block, defaults to mainnet."
    )
    (
        "bitcoin.checkpoint",
        setting<chain::checkpoints>(&configured.bitcoin.checkpoints),
        "The blockchain checkpoints, defaults to the consensus set."
    )
    // [version properties excluded here]
    (
        "bitcoin.bip16_activation_time",
        setting<uint32_t>(&configured.bitcoin.bip16_activation_time),
        "The activation time for bip16 in unix time, defaults to {}."
    )
    (
        "bitcoin.bip34_activation_threshold",
        setting<size_t>(&configured.bitcoin.bip34_activation_threshold),
        "The number of new version blocks required for bip34 style soft fork activation, defaults to {}."
    )
    (
        "bitcoin.bip34_enforcement_threshold",
        setting<size_t>(&configured.bitcoin.bip34_enforcement_threshold),
        "The number of new version blocks required for bip34 style soft fork enforcement, defaults to {}."
    )
    (
        "bitcoin.bip34_activation_sample",
        setting<size_t>(&configured.bitcoin.bip34_activation_sample),
        "The number of blocks considered for bip34 style soft fork activation, defaults to {}."
    )
    (
        "bitcoin.bip34_freeze",
        setting<size_t>(&configured.bitcoin.bip90_bip34_height),
        "The block height to freeze the bip34 softfork for bip90, defaults to {}."
    )
    (
        "bitcoin.bip65_freeze",
        setting<size_t>(&configured.bitcoin.bip90_bip65_height),
        "The block height to freeze the bip65 softfork for bip90, defaults to {}."
    )
    (
        "bitcoin.bip66_freeze",
        setting<size_t>(&configured.bitcoin.bip90_bip66_height),
        "The block height to freeze the bip66 softfork for bip90, defaults to {}."
    )
    (
        "bitcoin.bip30_reactivate_height",
        setting<size_t>(&configured.bitcoin.bip30_reactivate_height),
        "The height for bip30 reactivation, defaults to {}."
    )
    (
        "bitcoin.bip30_deactivate_checkpoint",
        setting<chain::checkpoint>(&configured.bitcoin.bip30_deactivate_checkpoint),
        "The hash:height checkpoint for bip30 deactivation, defaults to {}."
    )
    (
        "bitcoin.bip9_bit0_active_checkpoint",
        setting<chain::checkpoint>(&configured.bitcoin.bip9_bit0_active_checkpoint),
        "The hash:height checkpoint for bip9 bit0 activation, defaults to {}."
    )
    (
        "bitcoin.bip9_bit1_active_checkpoint",
        setting<chain::checkpoint>(&configured.bitcoin.bip9_bit1_active_checkpoint),
        "The hash:height checkpoint for bip9 bit1 activation, defaults to {}."
    )
    (
        "bitcoin.bip9_bit2_active_checkpoint",
        setting<chain::checkpoint>(&configured.bitcoin.bip9_bit2_active_checkpoint),
        "The hash:height checkpoint for bip9 bit2 activation, defaults to {}."
    )
    (
        settings::milestone,
        setting<chain::checkpoint>(&configured.bitcoin.milestone),
        "A block presumed to be valid but not required to be present, defaults to {}."
    )
    (
        "bitcoin.minimum_work",
        setting<config::hash256>(&configured.bitcoin.minimum_work),
        "The minimum work for any branch to be considered valid, defaults to {}."
    )

    /* [network] */
    (
        "network.threads",
        setting<uint32_t>(&configured.network.threads),
        "The minimum number of threads in the network threadpool, defaults to {} (hardware threads, at most 32)."
    )
    (
        "network.retry_timeout_seconds",
        setting<uint32_t>(&configured.network.retry_timeout_seconds),
        "The time delay for failed connection retry, defaults to {}."
    )
    (
        "network.connect_timeout_seconds",
        setting<uint32_t>(&configured.network.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {}."
    )
    (
        "network.rate_limit",
        setting<uint32_t>(&configured.network.rate_limit),
        "The per channel send rate limit in bytes per second, defaults to {} (unlimited)."
    )
    (
        "network.blacklist",
        setting<network::config::authorities>(&configured.network.blacklists),
        "IP address to disallow, allows all others, multiple allowed."
    )
    (
        "network.whitelist",
        setting<network::config::authorities>(&configured.network.whitelists),
        "IP address to allow, prohibits all others, multiple allowed."
    )

    /* [peer] */
    (
        "peer.address_upper",
        setting<uint16_t>(&configured.network.address_upper),
        "The upper bound for address selection divisor, defaults to {}."
    )
    (
        "peer.address_lower",
        setting<uint16_t>(&configured.network.address_lower),
        "The lower bound for address selection divisor, defaults to {}."
    )
    (
        "peer.protocol_maximum",
        setting<uint32_t>(&configured.network.protocol_maximum),
        "The maximum network protocol version, defaults to {}."
    )
    (
        "peer.protocol_minimum",
        setting<uint32_t>(&configured.network.protocol_minimum),
        "The minimum network protocol version, defaults to {}."
    )
    (
        "peer.invalid_services",
        setting<uint64_t>(&configured.network.invalid_services),
        "The advertised services that cause a peer to be dropped, defaults to {}."
    )
    (
        "peer.enable_address",
        setting<bool>(&configured.network.enable_address),
        "Enable address gossip, defaults to {}."
    )
    (
        "peer.enable_address_v2",
        setting<bool>(&configured.network.enable_address_v2),
        "Enable privacy network (Tor and I2P) address gossip, defaults to {}."
    )
    (
        "peer.enable_witness_tx",
        setting<bool>(&configured.network.enable_witness_tx),
        "Enable witness transaction identifier relay, defaults to {}."
    )
    (
        "peer.enable_compact",
        setting<bool>(&configured.network.enable_compact),
        "Enable compact block messages, defaults to {}."
    )
    (
        "peer.enable_alert",
        setting<bool>(&configured.network.enable_alert),
        "Enable alert messages, defaults to {}."
    )
    (
        "peer.enable_reject",
        setting<bool>(&configured.network.enable_reject),
        "Enable reject messages, defaults to {}."
    )
    (
        "peer.enable_not_found",
        setting<bool>(&configured.network.enable_not_found),
        "Enable not found messages, defaults to {}."
    )
    (
        "peer.enable_memory_pool",
        setting<bool>(&configured.network.enable_memory_pool),
        "Enable memory pool messages, defaults to {}."
    )
    (
        "peer.enable_relay",
        setting<bool>(&configured.network.enable_relay),
        "Enable transaction relay, defaults to {}."
    )
    (
        "peer.validate_checksum",
        setting<bool>(&configured.network.validate_checksum),
        "Validate the checksum of network messages, defaults to {}."
    )
    (
        "peer.gossip_ipv4",
        setting<bool>(&configured.network.gossip_ipv4),
        "Gossip internet protocol version 4 (IPv4) addresses, defaults to {}."
    )
    (
        "peer.gossip_ipv6",
        setting<bool>(&configured.network.gossip_ipv6),
        "Gossip internet protocol version 6 (IPv6) addresses, defaults to {}."
    )
    (
        "peer.gossip_tor",
        setting<bool>(&configured.network.gossip_tor),
        "Gossip tor (onion) addresses, defaults to {}."
    )
    (
        "peer.gossip_i2p",
        setting<bool>(&configured.network.gossip_i2p),
        "Gossip i2p addresses, defaults to {}."
    )
    (
        "peer.identifier",
        setting<uint32_t>(&configured.network.identifier),
        "The magic number for message headers, defaults to {}."
    )
    (
        "peer.handshake_timeout_seconds",
        setting<uint32_t>(&configured.network.handshake_timeout_seconds),
        "The time limit to complete the connection handshake, defaults to {} (0 disables)."
    )
    (
        "peer.channel_heartbeat_minutes",
        setting<uint32_t>(&configured.network.channel_heartbeat_minutes),
        "The time between ping messages, defaults to {} (0 disables)."
    )
    (
        "peer.maximum_skew_minutes",
        setting<uint32_t>(&configured.network.maximum_skew_minutes),
        "The maximum allowable channel clock skew, defaults to {}."
    )
    (
        "peer.user_agent",
        setting<std::string>(&configured.network.user_agent),
        "The node user agent string, defaults to {}."
    )
    (
        "peer.path",
        setting<config::path>(&configured.network.path),
        "The peer address cache file directory, defaults to {}."
    )

    /* [outbound] */
    (
        "outbound.bind",
        setting<network::config::authorities>(&configured.network.outbound.binds),
        "IP address to bind outbound connections, multiple allowed, defaults to {}."
    )
    (
        "outbound.connections",
        setting<uint16_t>(&configured.network.outbound.connections),
        "The target number of outgoing network connections, defaults to {}."
    )
    (
        "outbound.current_connections",
        setting<uint16_t>(&configured.node.current_connections),
        "The target number of outgoing connections when current, defaults to {} (0 disables)."
    )
    (
        "outbound.rate_limit",
        setting<uint32_t>(&configured.network.outbound.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "outbound.connect_timeout_seconds",
        setting<uint32_t>(&configured.network.outbound.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "outbound.inactivity_minutes",
        setting<uint32_t>(&configured.network.outbound.inactivity_minutes),
        "The inactivity time limit for any connection, defaults to {} (0 disables)."
    )
    (
        "outbound.expiration_minutes",
        setting<uint32_t>(&configured.network.outbound.expiration_minutes),
        "The age limit for any connection, defaults to {} (0 disables)."
    )
    (
        "outbound.minimum_buffer",
        setting<uint32_t>(&configured.network.outbound.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "outbound.maximum_backlog",
        setting<uint32_t>(&configured.network.outbound.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "outbound.maximum_request",
        setting<uint32_t>(&configured.network.outbound.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "outbound.seed",
        setting<network::config::endpoints>(&configured.network.outbound.seeds),
        "A seed node for initializing the host pool, multiple allowed."
    )
    (
        "outbound.connect_batch_size",
        setting<uint16_t>(&configured.network.outbound.connect_batch_size),
        "The number of concurrent attempts to establish one connection, defaults to {}."
    )
    (
        "outbound.host_pool_capacity",
        setting<uint32_t>(&configured.network.outbound.host_pool_capacity),
        "The maximum number of peer hosts in the pool, defaults to {}."
    )
    (
        "outbound.seeding_timeout_seconds",
        setting<uint32_t>(&configured.network.outbound.seeding_timeout_seconds),
        "The time limit for obtaining seed connections and addresses, defaults to {}."
    )
    (
        "outbound.username",
        setting<std::string>(&configured.network.outbound.username),
        "The socks5 proxy username (optional)."
    )
    (
        "outbound.password",
        secret<std::string>(&configured.network.outbound.password),
        "The socks5 proxy password (optional)."
    )
    (
        "outbound.socks",
        setting<config::endpoint>(&configured.network.outbound.socks),
        "The socks5 proxy endpoint (port required)."
    )

    /* [inbound] */
    ////(
    ////    "inbound.secure",
    ////    setting<bool>(&configured.network.inbound.secure),
    ////    "Require transport layer security, defaults to {} (not implemented)."
    ////)
    (
        "inbound.bind",
        setting<network::config::authorities>(&configured.network.inbound.binds),
        "IP address to bind for listening, multiple allowed, defaults to {} (all IPv4)."
    )
    (
        "inbound.connections",
        setting<uint16_t>(&configured.network.inbound.connections),
        "The target number of incoming network connections, defaults to {}."
    )
    (
        "inbound.rate_limit",
        setting<uint32_t>(&configured.network.inbound.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "inbound.connect_timeout_seconds",
        setting<uint32_t>(&configured.network.inbound.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "inbound.inactivity_minutes",
        setting<uint32_t>(&configured.network.inbound.inactivity_minutes),
        "The inactivity time limit for any connection, defaults to {} (0 disables)."
    )
    (
        "inbound.expiration_minutes",
        setting<uint32_t>(&configured.network.inbound.expiration_minutes),
        "The age limit for any connection, defaults to {} (0 disables)."
    )
    (
        "inbound.minimum_buffer",
        setting<uint32_t>(&configured.network.inbound.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "inbound.maximum_backlog",
        setting<uint32_t>(&configured.network.inbound.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "inbound.maximum_request",
        setting<uint32_t>(&configured.network.inbound.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "inbound.enable_loopback",
        setting<bool>(&configured.network.inbound.enable_loopback),
        "Allow connections from the node to itself, defaults to {}."
    )
    (
        "inbound.self",
        setting<network::config::addresses>(&configured.network.inbound.selfs),
        "Address to advertise, multiple allowed."
    )
    (
        "inbound.username",
        setting<std::string>(&configured.network.inbound.username),
        "The sam bridge username (optional)."
    )
    (
        "inbound.password",
        secret<std::string>(&configured.network.inbound.password),
        "The sam bridge password (optional)."
    )
    (
        "inbound.sam",
        setting<config::endpoint>(&configured.network.inbound.bridge),
        "The i2p sam bridge endpoint (port required)."
    )
    (
        "inbound.key_path",
        setting<config::path>(&configured.network.inbound.key_path),
        "The i2p destination private key file path, created if not existing."
    )

    /* [manual] */
    ////(
    ////    "manual.secure",
    ////    setting<bool>(&configured.network.manual.secure),
    ////    "Require transport layer security, defaults to {} (not implemented)."
    ////)
    (
        "manual.bind",
        setting<network::config::authorities>(&configured.network.manual.binds),
        "IP address to bind manual connections, multiple allowed, defaults to {}."
    )
    ////(
    ////    "manual.connections",
    ////    setting<uint16_t>(&configured.network.manual.connections),
    ////    "The target number of outgoing manual connections (not implemented)."
    ////)
    (
        "manual.rate_limit",
        setting<uint32_t>(&configured.network.manual.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "manual.connect_timeout_seconds",
        setting<uint32_t>(&configured.network.manual.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "manual.inactivity_minutes",
        setting<uint32_t>(&configured.network.manual.inactivity_minutes),
        "The inactivity time limit for any connection, defaults to {} (0 disables, will attempt reconnect)."
    )
    (
        "manual.expiration_minutes",
        setting<uint32_t>(&configured.network.manual.expiration_minutes),
        "The age limit for any connection, defaults to {} (0 disables, will attempt reconnect)."
    )
    (
        "manual.minimum_buffer",
        setting<uint32_t>(&configured.network.manual.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "manual.maximum_backlog",
        setting<uint32_t>(&configured.network.manual.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "manual.maximum_request",
        setting<uint32_t>(&configured.network.manual.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "manual.peer",
        setting<network::config::endpoints>(&configured.network.manual.peers),
        "A persistent peer node, multiple allowed."
    )
    (
        "manual.username",
        setting<std::string>(&configured.network.manual.username),
        "The socks5 proxy username (optional)."
    )
    (
        "manual.password",
        secret<std::string>(&configured.network.manual.password),
        "The socks5 proxy password (optional)."
    )
    (
        "manual.socks",
        setting<config::endpoint>(&configured.network.manual.socks),
        "The socks5 proxy endpoint (port required)."
    )

    /* [wallet] */
    (
        "wallet.p2kh_prefix",
        setting<config::byte>(&configured.server.wallet.p2kh_prefix),
        "The pay-to-public-key-hash address prefix, defaults to {} (use '111' for testnet)."
    )
    (
        "wallet.p2sh_prefix",
        setting<config::byte>(&configured.server.wallet.p2sh_prefix),
        "The pay-to-script-hash address prefix, defaults to {} (use '196' for testnet)."
    )
    (
        "wallet.wif_prefix",
        setting<config::byte>(&configured.server.wallet.wif_prefix),
        "The wallet import format prefix, defaults to {} (use '239' for testnet)."
    )
    (
        "wallet.witness_prefix",
        setting<std::string>(&configured.server.wallet.witness_prefix),
        "The witness address prefix, defaults to {} (use 'tb' for testnet)."
    )
    (
        "wallet.silent_prefix",
        setting<std::string>(&configured.server.wallet.silent_prefix),
        "The silent payment address prefix, defaults to {} (use 'tsp' for testnet)."
    )
    (
        "wallet.hd_private_prefix",
        setting<uint32_t>(&configured.server.wallet.hd_private_prefix),
        "The extended private key prefix, defaults to {} (use '70615956' for testnet)."
    )
    (
        "wallet.hd_public_prefix",
        setting<uint32_t>(&configured.server.wallet.hd_public_prefix),
        "The extended public key prefix, defaults to {} (use '71979618' for testnet)."
    )

    /* [admin] */
    (
        "admin.bind",
        setting<network::config::authorities>(&configured.server.admin.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "admin.safe",
        setting<network::config::authorities>(&configured.server.admin.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "admin.cert_auth",
        setting<config::path>(&configured.server.admin.cert_auth),
        "The certificate authority directory (*.PEM), enables client authentication."
    )
    (
        "admin.cert_path",
        setting<config::path>(&configured.server.admin.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "admin.key_path",
        setting<config::path>(&configured.server.admin.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "admin.key_pass",
        secret<std::string>(&configured.server.admin.key_pass),
        "The password to decrypt the server private key file (.PEM), optional."
    )
    (
        "admin.connections",
        setting<uint16_t>(&configured.server.admin.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "admin.rate_limit",
        setting<uint32_t>(&configured.server.admin.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "admin.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.admin.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "admin.inactivity_minutes",
        setting<uint32_t>(&configured.server.admin.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "admin.expiration_minutes",
        setting<uint32_t>(&configured.server.admin.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "admin.minimum_buffer",
        setting<uint32_t>(&configured.server.admin.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "admin.maximum_backlog",
        setting<uint32_t>(&configured.server.admin.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "admin.maximum_buffer",
        setting<uint32_t>(&configured.server.admin.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "admin.maximum_request",
        setting<uint32_t>(&configured.server.admin.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "admin.server",
        setting<std::string>(&configured.server.admin.server),
        "The server name (http header), defaults to {}."
    )
    (
        "admin.host",
        setting<network::config::endpoints>(&configured.server.admin.hosts),
        "The host name (http verification), multiple allowed, defaults to {} (disabled)."
    )
    (
        "admin.origin",
        setting<network::config::endpoints>(&configured.server.admin.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to {} (disabled)."
    )
    (
        "admin.allow_opaque_origin",
        setting<bool>(&configured.server.admin.allow_opaque_origin),
        "Allow requests from opaque origin (see CORS), defaults to {}."
    )
    (
        "admin.path",
        setting<config::path>(&configured.server.admin.path),
        "The required root path of source files to be served, defaults to {}."
    )
    (
        "admin.default",
        setting<std::string>(&configured.server.admin.default_),
        "The path of the default source page, defaults to {}."
    )

    /* [native] */
    (
        "native.bind",
        setting<network::config::authorities>(&configured.server.native.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "native.safe",
        setting<network::config::authorities>(&configured.server.native.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "native.cert_auth",
        setting<config::path>(&configured.server.native.cert_auth),
        "The certificate authority directory (*.PEM), enables client authentication."
    )
    (
        "native.cert_path",
        setting<config::path>(&configured.server.native.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "native.key_path",
        setting<config::path>(&configured.server.native.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "native.key_pass",
        secret<std::string>(&configured.server.native.key_pass),
        "The password to decrypt the server private key file (.PEM), optional."
    )
    (
        "native.connections",
        setting<uint16_t>(&configured.server.native.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "native.rate_limit",
        setting<uint32_t>(&configured.server.native.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "native.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.native.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "native.inactivity_minutes",
        setting<uint32_t>(&configured.server.native.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "native.expiration_minutes",
        setting<uint32_t>(&configured.server.native.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "native.minimum_buffer",
        setting<uint32_t>(&configured.server.native.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "native.maximum_backlog",
        setting<uint32_t>(&configured.server.native.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "native.maximum_buffer",
        setting<uint32_t>(&configured.server.native.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "native.maximum_request",
        setting<uint32_t>(&configured.server.native.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "native.server",
        setting<std::string>(&configured.server.native.server),
        "The server name (http header), defaults to {}."
    )
    (
        "native.host",
        setting<network::config::endpoints>(&configured.server.native.hosts),
        "The host name (http verification), multiple allowed, defaults to {} (disabled)."
    )
    (
        "native.origin",
        setting<network::config::endpoints>(&configured.server.native.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to {} (disabled)."
    )
    (
        "native.allow_opaque_origin",
        setting<bool>(&configured.server.native.allow_opaque_origin),
        "Allow requests from opaque origin (see CORS), defaults to {}."
    )
    (
        "native.path",
        setting<config::path>(&configured.server.native.path),
        "The required root path of source files to be served, defaults to {}."
    )
    (
        "native.default",
        setting<std::string>(&configured.server.native.default_),
        "The path of the default source page, defaults to {}."
    )
    (
        "native.websocket",
        setting<bool>(&configured.server.native.websocket),
        "Enable websocket interface, defaults to {}."
    )

    /* [bitcoind] */
    (
        "bitcoind.bind",
        setting<network::config::authorities>(&configured.server.bitcoind.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "bitcoind.safe",
        setting<network::config::authorities>(&configured.server.bitcoind.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "bitcoind.cert_auth",
        setting<config::path>(&configured.server.bitcoind.cert_auth),
        "The certificate authority directory (*.PEM), enables client authentication."
    )
    (
        "bitcoind.cert_path",
        setting<config::path>(&configured.server.bitcoind.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "bitcoind.key_path",
        setting<config::path>(&configured.server.bitcoind.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "bitcoind.key_pass",
        secret<std::string>(&configured.server.bitcoind.key_pass),
        "The password to decrypt the server private key file (.PEM), optional."
    )
    (
        "bitcoind.credential",
        secret<network::config::credentials>(&configured.server.bitcoind.credentials),
        "The 'username:password[:method,...]' authorization (not secure), multiple allowed."
    )
    (
        "bitcoind.connections",
        setting<uint16_t>(&configured.server.bitcoind.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "bitcoind.rate_limit",
        setting<uint32_t>(&configured.server.bitcoind.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "bitcoind.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.bitcoind.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "bitcoind.inactivity_minutes",
        setting<uint32_t>(&configured.server.bitcoind.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "bitcoind.expiration_minutes",
        setting<uint32_t>(&configured.server.bitcoind.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "bitcoind.minimum_buffer",
        setting<uint32_t>(&configured.server.bitcoind.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "bitcoind.maximum_backlog",
        setting<uint32_t>(&configured.server.bitcoind.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "bitcoind.maximum_buffer",
        setting<uint32_t>(&configured.server.bitcoind.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "bitcoind.maximum_request",
        setting<uint32_t>(&configured.server.bitcoind.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "bitcoind.server",
        setting<std::string>(&configured.server.bitcoind.server),
        "The server name (http header), defaults to {}."
    )
    (
        "bitcoind.version",
        setting<version>(&configured.server.bitcoind.version),
        "The version identity (getnetworkinfo), defaults to {}."
    )
    (
        "bitcoind.subversion",
        setting<std::string>(&configured.server.bitcoind.subversion),
        "The subversion identity (getnetworkinfo), defaults to {}."
    )
    (
        "bitcoind.host",
        setting<network::config::endpoints>(&configured.server.bitcoind.hosts),
        "The host name (http verification), multiple allowed, defaults to {} (disabled)."
    )
    (
        "bitcoind.origin",
        setting<network::config::endpoints>(&configured.server.bitcoind.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to {} (disabled)."
    )
    (
        "bitcoind.allow_opaque_origin",
        setting<bool>(&configured.server.bitcoind.allow_opaque_origin),
        "Allow requests from opaque origin (see CORS), defaults to {}."
    )

    /* [btcd] */
    (
        "btcd.bind",
        setting<network::config::authorities>(&configured.server.btcd.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "btcd.safe",
        setting<network::config::authorities>(&configured.server.btcd.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "btcd.cert_auth",
        setting<config::path>(&configured.server.btcd.cert_auth),
        "The certificate authority directory (*.PEM), enables client authentication."
    )
    (
        "btcd.cert_path",
        setting<config::path>(&configured.server.btcd.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "btcd.key_path",
        setting<config::path>(&configured.server.btcd.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "btcd.key_pass",
        secret<std::string>(&configured.server.btcd.key_pass),
        "The password to decrypt the server private key file (.PEM), optional."
    )
    (
        "btcd.credential",
        secret<network::config::credentials>(&configured.server.btcd.credentials),
        "The 'username:password[:method,...]' authorization (not secure), multiple allowed."
    )
    (
        "btcd.connections",
        setting<uint16_t>(&configured.server.btcd.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "btcd.rate_limit",
        setting<uint32_t>(&configured.server.btcd.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "btcd.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.btcd.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "btcd.inactivity_minutes",
        setting<uint32_t>(&configured.server.btcd.inactivity_minutes),
        "The idle timeout (http/ws keep-alive), defaults to {}."
    )
    (
        "btcd.expiration_minutes",
        setting<uint32_t>(&configured.server.btcd.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "btcd.minimum_buffer",
        setting<uint32_t>(&configured.server.btcd.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "btcd.maximum_backlog",
        setting<uint32_t>(&configured.server.btcd.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "btcd.maximum_buffer",
        setting<uint32_t>(&configured.server.btcd.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "btcd.maximum_request",
        setting<uint32_t>(&configured.server.btcd.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "btcd.server",
        setting<std::string>(&configured.server.btcd.server),
        "The server name (http header), defaults to {}."
    )
    (
        "btcd.maximum_filters",
        setting<uint32_t>(&configured.server.btcd.maximum_filters),
        "The maximum number of loadtxfilter watches, defaults to {}."
    )
    (
        "btcd.maximum_history",
        setting<uint32_t>(&configured.server.btcd.maximum_history),
        "The maximum number of address history entries, defaults to {}."
    )
    (
        "btcd.host",
        setting<network::config::endpoints>(&configured.server.btcd.hosts),
        "The host name (http verification), multiple allowed, defaults to {} (disabled)."
    )
    (
        "btcd.origin",
        setting<network::config::endpoints>(&configured.server.btcd.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to {} (disabled)."
    )
    (
        "btcd.allow_opaque_origin",
        setting<bool>(&configured.server.btcd.allow_opaque_origin),
        "Allow requests from opaque origin (see CORS), defaults to {}."
    )

    /* [electrum] */
    (
        "electrum.bind",
        setting<network::config::authorities>(&configured.server.electrum.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "electrum.safe",
        setting<network::config::authorities>(&configured.server.electrum.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "electrum.cert_path",
        setting<config::path>(&configured.server.electrum.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "electrum.key_path",
        setting<config::path>(&configured.server.electrum.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "electrum.connections",
        setting<uint16_t>(&configured.server.electrum.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "electrum.rate_limit",
        setting<uint32_t>(&configured.server.electrum.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "electrum.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.electrum.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "electrum.inactivity_minutes",
        setting<uint32_t>(&configured.server.electrum.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "electrum.expiration_minutes",
        setting<uint32_t>(&configured.server.electrum.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "electrum.minimum_buffer",
        setting<uint32_t>(&configured.server.electrum.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "electrum.maximum_backlog",
        setting<uint32_t>(&configured.server.electrum.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "electrum.maximum_buffer",
        setting<uint32_t>(&configured.server.electrum.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "electrum.maximum_request",
        setting<uint32_t>(&configured.server.electrum.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "electrum.maximum_headers",
        setting<uint32_t>(&configured.server.electrum.maximum_headers),
        "The maximum allowed headers returned per request, defaults to {}."
    )
    (
        "electrum.maximum_history",
        setting<uint32_t>(&configured.server.electrum.maximum_history),
        "The maximum number of address history entries upon one subscription, defaults to {}."
    )
    (
        "electrum.maximum_subscriptions",
        setting<uint32_t>(&configured.server.electrum.maximum_subscriptions),
        "The maximum allowed address subscriptions per channel, defaults to {}."
    )
    (
        "electrum.ping_interval_seconds",
        setting<uint32_t>(&configured.server.electrum.ping_interval_seconds),
        "The seconds between unrequested pings, defaults to {} (disabled)."
    )
    (
        "electrum.ping_size",
        setting<uint32_t>(&configured.server.electrum.ping_size),
        "The hex characters of unrequested ping data, defaults to {}."
    )
    (
        "electrum.protocol_minimum",
        setting<version>(&configured.server.electrum.protocol_minimum),
        "Minimum protocol version, defaults to {}."
    )
    (
        "electrum.protocol_maximum",
        setting<version>(&configured.server.electrum.protocol_maximum),
        "Maximum protocol version, defaults to {}."
    )
    (
        "electrum.server_name",
        setting<std::string>(&configured.server.electrum.server_name),
        "String returned by server.version, defaults to {}."
    )
    (
        "electrum.donation_address",
        setting<std::string>(&configured.server.electrum.donation_address),
        "String returned by server.donation_address, defaults to {}."
    )
    (
        "electrum.banner_message",
        setting<std::string>(&configured.server.electrum.banner_message),
        "String returned by server.banner, defaults to {}."
    )
    (
        "electrum.self_bind",
        setting<network::config::endpoints>(&configured.server.electrum.self_binds),
        "Advertised host:port at which this server can be reached (defaults to {})."
    )
    (
        "electrum.self_safe",
        setting<network::config::endpoints>(&configured.server.electrum.self_safes),
        "Advertised secure host:port at which this server can be reached (defaults to {})."
    )
    (
        "electrum.more_bind",
        setting<network::config::endpoints>(&configured.server.electrum.more_binds),
        "Advertised host:port at which another server can be reached (defaults to {})."
    )
    (
        "electrum.more_safe",
        setting<network::config::endpoints>(&configured.server.electrum.more_safes),
        "Advertised secure host:port at which another server can be reached (defaults to {})."
    )

    /* [sparrow] */
    (
        "sparrow.bind",
        setting<network::config::authorities>(&configured.server.sparrow.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "sparrow.safe",
        setting<network::config::authorities>(&configured.server.sparrow.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "sparrow.cert_path",
        setting<config::path>(&configured.server.sparrow.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "sparrow.key_path",
        setting<config::path>(&configured.server.sparrow.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "sparrow.connections",
        setting<uint16_t>(&configured.server.sparrow.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "sparrow.rate_limit",
        setting<uint32_t>(&configured.server.sparrow.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "sparrow.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.sparrow.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "sparrow.inactivity_minutes",
        setting<uint32_t>(&configured.server.sparrow.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "sparrow.expiration_minutes",
        setting<uint32_t>(&configured.server.sparrow.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "sparrow.minimum_buffer",
        setting<uint32_t>(&configured.server.sparrow.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "sparrow.maximum_backlog",
        setting<uint32_t>(&configured.server.sparrow.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "sparrow.maximum_buffer",
        setting<uint32_t>(&configured.server.sparrow.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "sparrow.maximum_request",
        setting<uint32_t>(&configured.server.sparrow.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "sparrow.maximum_headers",
        setting<uint32_t>(&configured.server.sparrow.maximum_headers),
        "The maximum allowed headers returned per request, defaults to {}."
    )
    (
        "sparrow.maximum_history",
        setting<uint32_t>(&configured.server.sparrow.maximum_history),
        "The maximum number of address history entries upon one subscription, defaults to {}."
    )
    (
        "sparrow.maximum_subscriptions",
        setting<uint32_t>(&configured.server.sparrow.maximum_subscriptions),
        "The maximum allowed address subscriptions per channel, defaults to {}."
    )
    (
        "sparrow.ping_interval_seconds",
        setting<uint32_t>(&configured.server.sparrow.ping_interval_seconds),
        "The seconds between unrequested pings, defaults to {} (disabled)."
    )
    (
        "sparrow.ping_size",
        setting<uint32_t>(&configured.server.sparrow.ping_size),
        "The hex characters of unrequested ping data, defaults to {}."
    )
    (
        "sparrow.protocol_minimum",
        setting<version>(&configured.server.sparrow.protocol_minimum),
        "Minimum protocol version, defaults to {}."
    )
    (
        "sparrow.protocol_maximum",
        setting<version>(&configured.server.sparrow.protocol_maximum),
        "Maximum protocol version, defaults to {}."
    )
    (
        "sparrow.server_name",
        setting<std::string>(&configured.server.sparrow.server_name),
        "String returned by server.version, defaults to {}."
    )
    (
        "sparrow.donation_address",
        setting<std::string>(&configured.server.sparrow.donation_address),
        "String returned by server.donation_address, defaults to {}."
    )
    (
        "sparrow.banner_message",
        setting<std::string>(&configured.server.sparrow.banner_message),
        "String returned by server.banner, defaults to {}."
    )
    (
        "sparrow.self_bind",
        setting<network::config::endpoints>(&configured.server.sparrow.self_binds),
        "Advertised host:port at which this server can be reached (defaults to {})."
    )
    (
        "sparrow.self_safe",
        setting<network::config::endpoints>(&configured.server.sparrow.self_safes),
        "Advertised secure host:port at which this server can be reached (defaults to {})."
    )
    (
        "sparrow.more_bind",
        setting<network::config::endpoints>(&configured.server.sparrow.more_binds),
        "Advertised host:port at which another server can be reached (defaults to {})."
    )
    (
        "sparrow.more_safe",
        setting<network::config::endpoints>(&configured.server.sparrow.more_safes),
        "Advertised secure host:port at which another server can be reached (defaults to {})."
    )
    /* [esplora] */
    (
        "esplora.bind",
        setting<network::config::authorities>(&configured.server.esplora.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "esplora.safe",
        setting<network::config::authorities>(&configured.server.esplora.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "esplora.cert_auth",
        setting<config::path>(&configured.server.esplora.cert_auth),
        "The certificate authority directory (*.PEM), enables client authentication."
    )
    (
        "esplora.cert_path",
        setting<config::path>(&configured.server.esplora.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "esplora.key_path",
        setting<config::path>(&configured.server.esplora.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "esplora.key_pass",
        secret<std::string>(&configured.server.esplora.key_pass),
        "The password to decrypt the server private key file (.PEM), optional."
    )
    (
        "esplora.connections",
        setting<uint16_t>(&configured.server.esplora.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "esplora.rate_limit",
        setting<uint32_t>(&configured.server.esplora.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "esplora.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.esplora.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "esplora.inactivity_minutes",
        setting<uint32_t>(&configured.server.esplora.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "esplora.expiration_minutes",
        setting<uint32_t>(&configured.server.esplora.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "esplora.minimum_buffer",
        setting<uint32_t>(&configured.server.esplora.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "esplora.maximum_backlog",
        setting<uint32_t>(&configured.server.esplora.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "esplora.maximum_buffer",
        setting<uint32_t>(&configured.server.esplora.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "esplora.maximum_request",
        setting<uint32_t>(&configured.server.esplora.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "esplora.server",
        setting<std::string>(&configured.server.esplora.server),
        "The server name (http header), defaults to {}."
    )
    (
        "esplora.maximum_history",
        setting<uint32_t>(&configured.server.esplora.maximum_history),
        "The maximum number of address history entries, defaults to {}."
    )
    (
        "esplora.host",
        setting<network::config::endpoints>(&configured.server.esplora.hosts),
        "The host name (http verification), multiple allowed, defaults to {} (disabled)."
    )
    (
        "esplora.origin",
        setting<network::config::endpoints>(&configured.server.esplora.origins),
        "The allowed origin (see CORS), multiple allowed, defaults to {} (disabled)."
    )
    (
        "esplora.allow_opaque_origin",
        setting<bool>(&configured.server.esplora.allow_opaque_origin),
        "Allow requests from opaque origin (see CORS), defaults to {}."
    )
    /* [stratum_v1] */
    (
        "stratum_v1.bind",
        setting<network::config::authorities>(&configured.server.stratum_v1.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "stratum_v1.safe",
        setting<network::config::authorities>(&configured.server.stratum_v1.safes),
        "IP address to secure bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "stratum_v1.cert_path",
        setting<config::path>(&configured.server.stratum_v1.cert_path),
        "The path to the server certificate file (.PEM), defaults to {}."
    )
    (
        "stratum_v1.key_path",
        setting<config::path>(&configured.server.stratum_v1.key_path),
        "The path to the server private key file (.PEM), defaults to {}."
    )
    (
        "stratum_v1.connections",
        setting<uint16_t>(&configured.server.stratum_v1.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "stratum_v1.rate_limit",
        setting<uint32_t>(&configured.server.stratum_v1.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "stratum_v1.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.stratum_v1.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "stratum_v1.inactivity_minutes",
        setting<uint32_t>(&configured.server.stratum_v1.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "stratum_v1.expiration_minutes",
        setting<uint32_t>(&configured.server.stratum_v1.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "stratum_v1.minimum_buffer",
        setting<uint32_t>(&configured.server.stratum_v1.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "stratum_v1.maximum_backlog",
        setting<uint32_t>(&configured.server.stratum_v1.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "stratum_v1.maximum_buffer",
        setting<uint32_t>(&configured.server.stratum_v1.maximum_buffer),
        "The maximum json response buffer size, defaults to {}."
    )
    (
        "stratum_v1.maximum_request",
        setting<uint32_t>(&configured.server.stratum_v1.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )

    /* [stratum_v2] */
    (
        "stratum_v2.bind",
        setting<network::config::authorities>(&configured.server.stratum_v2.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "stratum_v2.connections",
        setting<uint16_t>(&configured.server.stratum_v2.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "stratum_v2.rate_limit",
        setting<uint32_t>(&configured.server.stratum_v2.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "stratum_v2.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.stratum_v2.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "stratum_v2.inactivity_minutes",
        setting<uint32_t>(&configured.server.stratum_v2.inactivity_minutes),
        "The idle timeout (http keep-alive), defaults to {}."
    )
    (
        "stratum_v2.expiration_minutes",
        setting<uint32_t>(&configured.server.stratum_v2.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "stratum_v2.minimum_buffer",
        setting<uint32_t>(&configured.server.stratum_v2.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "stratum_v2.maximum_backlog",
        setting<uint32_t>(&configured.server.stratum_v2.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "stratum_v2.maximum_request",
        setting<uint32_t>(&configured.server.stratum_v2.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )

    /* [bitcoind_zmq] */
    (
        "bitcoind_zmq.bind",
        setting<network::config::authorities>(&configured.server.bitcoind_zmq.binds),
        "IP address to bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "bitcoind_zmq.safe",
        setting<network::config::authorities>(&configured.server.bitcoind_zmq.safes),
        "IP address to secure (CurveZMQ) bind, multiple allowed, defaults to {} (disabled)."
    )
    (
        "bitcoind_zmq.cert",
        setting<std::vector<config::base85>>(&configured.server.bitcoind_zmq.certs),
        "The Z85 encoded public key of an authorized client, multiple allowed, defaults to {} (all)."
    )
    (
        "bitcoind_zmq.connections",
        setting<uint16_t>(&configured.server.bitcoind_zmq.connections),
        "The required maximum number of connections, defaults to {}."
    )
    (
        "bitcoind_zmq.rate_limit",
        setting<uint32_t>(&configured.server.bitcoind_zmq.rate_limit),
        "The send rate limit in bytes per second, defaults to {} (network controls)."
    )
    (
        "bitcoind_zmq.connect_timeout_seconds",
        setting<uint32_t>(&configured.server.bitcoind_zmq.connect_timeout_seconds),
        "The time limit for connection establishment, defaults to {} (network controls)."
    )
    (
        "bitcoind_zmq.inactivity_minutes",
        setting<uint32_t>(&configured.server.bitcoind_zmq.inactivity_minutes),
        "The idle timeout, defaults to {}."
    )
    (
        "bitcoind_zmq.expiration_minutes",
        setting<uint32_t>(&configured.server.bitcoind_zmq.expiration_minutes),
        "The maximum connection duration, defaults to {}."
    )
    (
        "bitcoind_zmq.minimum_buffer",
        setting<uint32_t>(&configured.server.bitcoind_zmq.minimum_buffer),
        "The minimum retained read buffer size, defaults to {}."
    )
    (
        "bitcoind_zmq.maximum_backlog",
        setting<uint32_t>(&configured.server.bitcoind_zmq.maximum_backlog),
        "The maximum write backlog of a channel, defaults to {}."
    )
    (
        "bitcoind_zmq.maximum_request",
        setting<uint32_t>(&configured.server.bitcoind_zmq.maximum_request),
        "The maximum allowed request size, defaults to {}."
    )
    (
        "bitcoind_zmq.maximum_subscriptions",
        setting<uint32_t>(&configured.server.bitcoind_zmq.maximum_subscriptions),
        "The maximum topic subscriptions per connection, defaults to {}."
    )
    (
        "bitcoind_zmq.key",
        secret<config::base85>(&configured.server.bitcoind_zmq.key),
        "The Z85 encoded CurveZMQ server secret key, defaults to none (unencrypted)."
    )

    /* [node] */
    (
        "node.threads",
        setting<uint32_t>(&configured.node.threads),
        "The number of threads in the validation threadpool, defaults to {} (hardware threads)."
    )
    (
        "node.thread_priority",
        setting<bool>(&configured.node.thread_priority),
        "Set validation threads to high processing priority, defaults to {}."
    )
    (
        "node.memory_priority",
        setting<bool>(&configured.node.memory_priority),
        "Set the process to high memory priority, defaults to {}."
    )
    (
        "node.delay_inbound",
        setting<bool>(&configured.node.delay_inbound),
        "Block inbound peer/client (excluding admin/native) until current, defaults to {}."
    )
    (
        "node.provide_blocks",
        setting<bool>(&configured.node.provide_blocks),
        "Serve blocks to network connections, defaults to {}."
    )
    (
        "node.limited_blocks",
        setting<bool>(&configured.node.limited_blocks),
        "Limit block service to recent blocks, defaults to {}."
    )
    (
        "node.require_blocks",
        setting<bool>(&configured.node.require_blocks),
        "Require block service of outbound connections, defaults to {}."
    )
    (
        "node.provide_witness",
        setting<bool>(&configured.node.provide_witness),
        "Serve witness data to network connections, defaults to {}."
    )
    (
        "node.require_witness",
        setting<bool>(&configured.node.require_witness),
        "Require witness service of outbound connections, defaults to {}."
    )
    (
        "node.provide_filters",
        setting<bool>(&configured.node.provide_filters),
        "Serve client filters to network connections, defaults to {}."
    )
    (
        "node.provide_privacy",
        setting<bool>(&configured.node.provide_privacy),
        "Provide opportunistic connection encryption, defaults to {}."
    )
    (
        "node.batch_signatures",
        setting<uint64_t>(&configured.node.batch_signatures),
        "Count of signatures to verify in each GPU batch (as available), defaults to {} (0 disables)."
    )
    (
        "node.fee_estimate_horizon",
        setting<uint16_t>(&configured.node.fee_estimate_horizon),
        "Fee estimation horizon, limited to 1008, defaults to {} (0 disables)."
    )
    (
        "node.minimum_fee_rate",
        setting<double>(&configured.node.minimum_fee_rate),
        "Minimum fee rate for non-conflicting tx acceptance, defaults to {}."
    )
    (
        "node.minimum_bump_rate",
        setting<double>(&configured.node.minimum_bump_rate),
        "Minimum fee rate increment for conflicting tx acceptance, defaults to {}."
    )
    (
        "node.allowed_deviation",
        setting<float>(&configured.node.allowed_deviation),
        "Allowable underperformance standard deviation, defaults to {} (0 disables)."
    )
    (
        "node.announcement_cache",
        setting<uint16_t>(&configured.node.announcement_cache),
        "Limit of per channel cached peer block and tx announcements, to avoid replay, defaults to {}."
    )
    (
        "node.maximum_height",
        setting<uint32_t>(&configured.node.maximum_height),
        "Maximum block height to populate, defaults to {} (unlimited)."
    )
    (
        "node.silent_start_height",
        setting<uint32_t>(&configured.node.silent_start_height),
        "Minimum height of silent payment indexation, defaults to {}."
    )
    (
        "node.maximum_concurrency",
        setting<uint32_t>(&configured.node.maximum_concurrency),
        "Maximum number of blocks to download concurrently, defaults to {} (0 disables)."
    )
    (
        "node.sample_period_seconds",
        setting<uint16_t>(&configured.node.sample_period_seconds),
        "Sampling period for drop of stalled channels, defaults to {} (0 disables)."
    )
    (
        "node.compact_timeout_seconds",
        setting<uint16_t>(&configured.node.compact_timeout_seconds),
        "Time to await a compact block fill before downloading the block, defaults to {} (0 disables)."
    )
    (
        "node.compact_missing_percent",
        setting<uint16_t>(&configured.node.compact_missing_percent),
        "Maximum percent of a compact block not pooled to fill it, defaults to {}."
    )
    (
        "node.currency_window_minutes",
        setting<uint32_t>(&configured.node.currency_window_minutes),
        "Time from present that blocks are considered current, defaults to {} (0 disables)."
    )
    ////(
    ////    "node.snapshot_bytes",
    ////    setting<uint64_t>(&configured.node.snapshot_bytes),
    ////    "Downloaded bytes that triggers snapshot, defaults to {} (0 disables)."
    ////)
    ////(
    ////    "node.snapshot_valid",
    ////    setting<uint32_t>(&configured.node.snapshot_valid),
    ////    "Completed validations that trigger snapshot, defaults to {} (0 disables)."
    ////)
    ////(
    ////    "node.snapshot_confirm",
    ////    setting<uint32_t>(&configured.node.snapshot_confirm),
    ////    "Completed confirmations that trigger snapshot, defaults to {} (0 disables)."
    ////)

    /* [database] */
    (
        "database.path",
        setting<config::path>(&configured.database.path),
        "The blockchain database directory, defaults to {}."
    )
    (
        "database.turbo",
        setting<bool>(&configured.database.turbo),
        "Allow individual non-validation queries to use all CPUs, defaults to {}."
    )
    (
        "database.mark_unconfirmable",
        setting<bool>(&configured.database.mark_unconfirmable),
        "Save unconfirmable block state (prevents revalidation), defaults to {}."
    )
    (
        "database.interval_depth",
        setting<uint16_t>(&configured.database.interval_depth),
        "The interval depth for merkle proof optimization, defaults to {}."
    )

    /* [table.*] */

    /* table.header */
    (
        "table.header.buckets",
        setting<uint32_t>(&configured.database.header.buckets),
        "The number of buckets in the archive_header table head, dynamic default."
    )
    (
        "table.header.expected",
        setting<uint64_t>(&configured.database.header.expected),
        "The expected element count of the archive_header table, defaults to {}."
    )
    (
        "table.header.size",
        setting<uint64_t>(&configured.database.header.size),
        "The minimum allocation of the archive_header table body, defaults to {}."
    )
    (
        "table.header.rate",
        setting<uint16_t>(&configured.database.header.rate),
        "The percentage expansion of the archive_header table body, defaults to {}."
    )

    /* table.input */
    (
        "table.input.size",
        setting<uint64_t>(&configured.database.input.size),
        "The minimum allocation of the archive_input table body, defaults to {}."
    )
    (
        "table.input.rate",
        setting<uint16_t>(&configured.database.input.rate),
        "The percentage expansion of the archive_input table body, defaults to {}."
    )

    /* table.output */
    (
        "table.output.size",
        setting<uint64_t>(&configured.database.output.size),
        "The minimum allocation of the archive_output table body, defaults to {}."
    )
    (
        "table.output.rate",
        setting<uint16_t>(&configured.database.output.rate),
        "The percentage expansion of the archive_output table body, defaults to {}."
    )

    /* table.ins */
    (
        "table.ins.buckets",
        setting<uint32_t>(&configured.database.ins.buckets),
        "The number of buckets in the archive_ins table head, dynamic default."
    )
    (
        "table.ins.expected",
        setting<uint64_t>(&configured.database.ins.expected),
        "The expected element count of the archive_ins table, defaults to {}."
    )
    (
        "table.ins.size",
        setting<uint64_t>(&configured.database.ins.size),
        "The minimum allocation of the archive_ins table body, defaults to {}."
    )
    (
        "table.ins.rate",
        setting<uint16_t>(&configured.database.ins.rate),
        "The percentage expansion of the archive_ins table body, defaults to {}."
    )

    /* table.outs */
    (
        "table.outs.buckets",
        setting<uint32_t>(&configured.database.outs.buckets),
        "The number of buckets in the archive_outs table head, dynamic default (0 disables address index)."
    )
    (
        "table.outs.expected",
        setting<uint64_t>(&configured.database.outs.expected),
        "The expected element count of the archive_outs table, defaults to {}."
    )
    (
        "table.outs.size",
        setting<uint64_t>(&configured.database.outs.size),
        "The minimum allocation of the archive_outs table body, defaults to {}."
    )
    (
        "table.outs.rate",
        setting<uint16_t>(&configured.database.outs.rate),
        "The percentage expansion of the archive_outs table body, defaults to {}."
    )

    /* table.tx */
    (
        "table.tx.buckets",
        setting<uint32_t>(&configured.database.tx.buckets),
        "The number of buckets in the archive_tx table head, dynamic default."
    )
    (
        "table.tx.expected",
        setting<uint64_t>(&configured.database.tx.expected),
        "The expected element count of the archive_tx table, defaults to {}."
    )
    (
        "table.tx.size",
        setting<uint64_t>(&configured.database.tx.size),
        "The minimum allocation of the archive_tx table body, defaults to {}."
    )
    (
        "table.tx.rate",
        setting<uint16_t>(&configured.database.tx.rate),
        "The percentage expansion of the archive_tx table body, defaults to {}."
    )

    /* table.txs */
    (
        "table.txs.buckets",
        setting<uint32_t>(&configured.database.txs.buckets),
        "The number of buckets in the archive_txs table head, defaults to {}."
    )
    (
        "table.txs.size",
        setting<uint64_t>(&configured.database.txs.size),
        "The minimum allocation of the archive_txs table body, defaults to {}."
    )
    (
        "table.txs.rate",
        setting<uint16_t>(&configured.database.txs.rate),
        "The percentage expansion of the archive_txs table body, defaults to {}."
    )

    /* table.candidate */
    (
        "table.candidate.buckets",
        setting<uint32_t>(&configured.database.candidate.buckets),
        "The number of buckets provisioned in the index_candidate table head, defaults to {}."
    )
    (
        "table.candidate.rate",
        setting<uint16_t>(&configured.database.candidate.rate),
        "The percentage expansion of the index_candidate table head, defaults to {}."
    )

    /* table.confirmed */
    (
        "table.confirmed.buckets",
        setting<uint32_t>(&configured.database.confirmed.buckets),
        "The number of buckets provisioned in the index_confirmed table head, defaults to {}."
    )
    (
        "table.confirmed.rate",
        setting<uint16_t>(&configured.database.confirmed.rate),
        "The percentage expansion of the index_confirmed table head, defaults to {}."
    )

    /* table.strong */
    (
        "table.strong.buckets",
        setting<uint32_t>(&configured.database.strong_tx.buckets),
        "The number of buckets in the index_strong table head, dynamic default."
    )
    (
        "table.strong.expected",
        setting<uint64_t>(&configured.database.strong_tx.expected),
        "The expected element count of the index_strong table, defaults to {}."
    )
    (
        "table.strong.size",
        setting<uint64_t>(&configured.database.strong_tx.size),
        "The minimum allocation of the index_strong table body, defaults to {}."
    )
    (
        "table.strong.rate",
        setting<uint16_t>(&configured.database.strong_tx.rate),
        "The percentage expansion of the index_strong table body, defaults to {}."
    )

    /* table.scan */
    (
        "table.scan.size",
        setting<uint64_t>(&configured.database.scan.size),
        "The minimum allocation of the index_scan table body, defaults to {}."
    )
    (
        "table.scan.rate",
        setting<uint16_t>(&configured.database.scan.rate),
        "The percentage expansion of the index_scan table body, defaults to {}."
    )

    /* table.ecdsa */
    (
        "table.ecdsa.size",
        setting<uint64_t>(&configured.database.ecdsa.size),
        "The minimum allocation of each batch_ecdsa table body, defaults to {}."
    )
    (
        "table.ecdsa.rate",
        setting<uint16_t>(&configured.database.ecdsa.rate),
        "The percentage expansion of each batch_ecdsa table body, defaults to {}."
    )

    /* table.schnorr */
    (
        "table.schnorr.size",
        setting<uint64_t>(&configured.database.schnorr.size),
        "The minimum allocation of each batch_schnorr table body, defaults to {}."
    )
    (
        "table.schnorr.rate",
        setting<uint16_t>(&configured.database.schnorr.rate),
        "The percentage expansion of each batch_schnorr table body, defaults to {}."
    )

    /* table.silent */
    (
        "table.silent.size",
        setting<uint64_t>(&configured.database.silent.size),
        "The minimum allocation of each batch_silent table body, defaults to {}."
    )
    (
        "table.silent.rate",
        setting<uint16_t>(&configured.database.silent.rate),
        "The percentage expansion of each batch_silent table body, defaults to {}."
    )

    /* table.prevalid */
    (
        "table.prevalid.size",
        setting<uint64_t>(&configured.database.prevalid.size),
        "The minimum allocation of each batch_prevalid table body, defaults to {}."
    )
    (
        "table.prevalid.rate",
        setting<uint16_t>(&configured.database.prevalid.rate),
        "The percentage expansion of each batch_prevalid table body, defaults to {}."
    )

    /* table.prevout */
    (
        "table.prevout.buckets",
        setting<uint32_t>(&configured.database.prevout.buckets),
        "The minimum number of buckets in the cache_prevout table head, defaults to {}."
    )
    (
        "table.prevout.size",
        setting<uint64_t>(&configured.database.prevout.size),
        "The minimum allocation of the cache_prevout table body, defaults to {}."
    )
    (
        "table.prevout.rate",
        setting<uint16_t>(&configured.database.prevout.rate),
        "The percentage expansion of the cache_prevout table, defaults to {}."
    )

    /* table.duplicate */
    (
        "table.duplicate.buckets",
        setting<uint32_t>(&configured.database.duplicate.buckets),
        "The minimum number of buckets in the cache_duplicate table head, defaults to {}."
    )
    (
        "table.duplicate.expected",
        setting<uint64_t>(&configured.database.duplicate.expected),
        "The expected element count of the cache_duplicate table, defaults to {}."
    )
    (
        "table.duplicate.size",
        setting<uint64_t>(&configured.database.duplicate.size),
        "The minimum allocation of the cache_duplicate table body, defaults to {}."
    )
    (
        "table.duplicate.rate",
        setting<uint16_t>(&configured.database.duplicate.rate),
        "The percentage expansion of the cache_duplicate table, defaults to {}."
    )

    /* table.state */
    (
        "table.state.buckets",
        setting<uint32_t>(&configured.database.state.buckets),
        "The number of buckets in the state table head, defaults to {}."
    )
    (
        "table.state.size",
        setting<uint64_t>(&configured.database.state.size),
        "The minimum allocation of the state table body, defaults to {}."
    )
    (
        "table.state.rate",
        setting<uint16_t>(&configured.database.state.rate),
        "The percentage expansion of the state table body, defaults to {}."
    )

    /* table.pool */
    (
        "table.pool.buckets",
        setting<uint32_t>(&configured.database.pool.buckets),
        "The number of buckets in the pool table head, dynamic default (0 disables)."
    )
    (
        "table.pool.expected",
        setting<uint64_t>(&configured.database.pool.expected),
        "The expected element count of the pool table, defaults to {}."
    )
    (
        "table.pool.size",
        setting<uint64_t>(&configured.database.pool.size),
        "The minimum allocation of the pool table body, defaults to {}."
    )
    (
        "table.pool.rate",
        setting<uint16_t>(&configured.database.pool.rate),
        "The percentage expansion of the pool table body, defaults to {}."
    )

    /* table.spends */
    (
        "table.spends.size",
        setting<uint64_t>(&configured.database.spends.size),
        "The minimum allocation of the spends table body, defaults to {}."
    )
    (
        "table.spends.rate",
        setting<uint16_t>(&configured.database.spends.rate),
        "The percentage expansion of the spends table body, defaults to {}."
    )

    /* table.filter_bk */
    (
        "table.filter_bk.buckets",
        setting<uint32_t>(&configured.database.filter_bk.buckets),
        "The number of buckets in the option_filter_bk table head, defaults to {} (0 disables)."
    )
    (
        "table.filter_bk.size",
        setting<uint64_t>(&configured.database.filter_bk.size),
        "The minimum allocation of the option_filter_bk table body, defaults to {}."
    )
    (
        "table.filter_bk.rate",
        setting<uint16_t>(&configured.database.filter_bk.rate),
        "The percentage expansion of the option_filter_bk table body, defaults to {}."
    )

    /* table.filter_tx */
    (
        "table.filter_tx.buckets",
        setting<uint32_t>(&configured.database.filter_tx.buckets),
        "The number of buckets in the option_filter_tx table head, defaults to {} (0 disables)."
    )
    (
        "table.filter_tx.size",
        setting<uint64_t>(&configured.database.filter_tx.size),
        "The minimum allocation of the option_filter_tx table body, defaults to {}."
    )
    (
        "table.filter_tx.rate",
        setting<uint16_t>(&configured.database.filter_tx.rate),
        "The percentage expansion of the option_filter_tx table body, defaults to {}."
    )

    /* [log] */
#if defined(HAVE_LOGA)
    (
        "log.application",
        setting<bool>(&configured.log.application),
        "Enable application logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGN)
    (
        "log.news",
        setting<bool>(&configured.log.news),
        "Enable news logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGS)
    (
        "log.session",
        setting<bool>(&configured.log.session),
        "Enable session logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGP)
    (
        "log.protocol",
        setting<bool>(&configured.log.protocol),
        "Enable protocol logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGX)
    (
        "log.proxy",
        setting<bool>(&configured.log.proxy),
        "Enable proxy logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGR)
    (
        "log.remote",
        setting<bool>(&configured.log.remote),
        "Enable remote fault logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGF)
    (
        "log.fault",
        setting<bool>(&configured.log.fault),
        "Enable local fault logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGQ)
    (
        "log.quitting",
        setting<bool>(&configured.log.quitting),
        "Enable quitting logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGO)
    (
        "log.objects",
        setting<bool>(&configured.log.objects),
        "Enable objects logging, defaults to {}."
    )
#endif
#if defined(HAVE_LOGV)
    (
        "log.verbose",
        setting<bool>(&configured.log.verbose),
        "Enable verbose logging, defaults to {}."
    )
#endif
    (
        "log.maximum_size",
        setting<uint32_t>(&configured.log.maximum_size),
        "The maximum byte size of each pair of rotated log files, defaults to {}."
    )
#if defined (HAVE_MSC)
    (
        "log.symbols",
        setting<config::path>(&configured.log.symbols),
        "Path to a directory containing windows symbols (.pdb) files, defaults to {}."
    )
#endif
    (
        "log.path",
        setting<config::path>(&configured.log.path),
        "The log files directory, defaults to {}."
    );

    return description;
}

BC_PUSH_WARNING(NO_ARRAY_TO_POINTER_DECAY)
bool parser::parse(int argc, const char* argv[], std::ostream& error) THROWS
BC_POP_WARNING()
{
    try
    {
        auto file = false;
        load_command_variables(argc, argv);
        load_environment_variables(environment_prefix);

        // Don't load config file if help is specified.
        if (!get_option(help_variable))
        {
            // Returns true if the settings were loaded from a file.
            file = load_configuration_variables(config_variable);
        }

        // Update bound variables in metadata.settings.
        notify(variables_);
        derive_buckets();

        // Clear the config file path if it wasn't used.
        if (!file)
            configured.file = {};
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
