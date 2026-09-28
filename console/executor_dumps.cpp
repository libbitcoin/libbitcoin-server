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
#include "executor.hpp"
#include "localize.hpp"

namespace libbitcoin {
namespace server {

using format = boost_format;

constexpr double to_double(auto integer)
{
    return 1.0 * integer;
}

// Store dumps.
// ----------------------------------------------------------------------------

// version information for libbitcoin libraries
void executor::dump_version() const
{
    const auto thumb = [](std::string_view hash, bool dirty)
    {
        return std::string{ hash.substr(0, 8) } + (dirty ? "-dirty" : "");
    };

    logger(format(BS_VERSION_HEADER));
    logger(format("boost................ %1%.%2%.%3%") % (BOOST_VERSION / 100000) % (BOOST_VERSION / 100 % 1000) % (BOOST_VERSION % 100));
    logger(format("secp256k1............ %1%") % system::secp256k1_library());
    logger(format("openssl (tls)........ %1%") % network::tls_library());
    logger(format("libbitcoin-system.... %1% %2%") % LIBBITCOIN_SYSTEM_VERSION   % thumb(LIBBITCOIN_SYSTEM_COMMIT_HASH, LIBBITCOIN_SYSTEM_IS_DIRTY));
    logger(format("libbitcoin-database.. %1% %2%") % LIBBITCOIN_DATABASE_VERSION % thumb(LIBBITCOIN_DATABASE_COMMIT_HASH, LIBBITCOIN_DATABASE_IS_DIRTY));
    logger(format("libbitcoin-network... %1% %2%") % LIBBITCOIN_NETWORK_VERSION  % thumb(LIBBITCOIN_NETWORK_COMMIT_HASH, LIBBITCOIN_NETWORK_IS_DIRTY));
    logger(format("libbitcoin-node...... %1% %2%") % LIBBITCOIN_NODE_VERSION     % thumb(LIBBITCOIN_NODE_COMMIT_HASH, LIBBITCOIN_NODE_IS_DIRTY));
    logger(format("libbitcoin-server.... %1% %2%") % LIBBITCOIN_SERVER_VERSION   % thumb(LIBBITCOIN_SERVER_COMMIT_HASH, LIBBITCOIN_SERVER_IS_DIRTY));
    logger(format("compiled schema...... %1%") % database::envelope::compiled);
    logger(format("database schema...... %1%") % query_.envelope().schema);
}

// The "try" functions are safe for instructions not compiled in.
void executor::dump_hardware() const
{
    using namespace system;
    using namespace database;

#if defined(HAVE_ARM)
    logger(BS_HARDWARE_HEADER_ARM64);
    logger(format("crypto...... " BS_HARDWARE_COMPILED)  % try_crypto()     % have_crypto);
    logger(format("sha3........ " BS_HARDWARE_COMPILED)  % try_sha3()       % have_sha3);
    logger(format("neon........ " BS_HARDWARE_COMPILED)  % try_neon()       % have_neon);
#else
    logger(BS_HARDWARE_HEADER_X64);
    logger(format("aesni....... " BS_HARDWARE_COMPILED)  % try_aesni()      % have_aesni);
    logger(format("vaes........ " BS_HARDWARE_COMPILED)  % try_vaes()       % have_vaes);
    logger(format("shani....... " BS_HARDWARE_COMPILED)  % try_shani()      % have_shani);
    logger(format("sha512...... " BS_HARDWARE_COMPILED)  % try_sha512()     % have_sha512);
    logger(format("sse41....... " BS_HARDWARE_COMPILED)  % try_sse41()      % have_sse41);
    logger(format("avx2........ " BS_HARDWARE_COMPILED)  % try_avx2()       % have_avx2);
    logger(format("avxifma..... " BS_HARDWARE_COMPILED)  % try_avxifma()    % have_avxifma);
    logger(format("avx512...... " BS_HARDWARE_COMPILED)  % try_avx512()     % have_avx512);
    logger(format("avx512ifma.. " BS_HARDWARE_COMPILED)  % try_avx512ifma() % have_avx512ifma);
#endif

    const auto device = gpu_device();
    const auto compiled = batched::compiled();
    const auto incompatible = device && compiled && !batched::accelerated();
    logger(format(incompatible ?
        "gpu......... " BS_HARDWARE_INCOMPATIBLE :
        "gpu......... " BS_HARDWARE_COMPILED) % device % compiled);
}

// logging compilation and initial values.
void executor::dump_options() const
{
    using namespace network;

    logger(BS_LOG_TABLE_HEADER);
    logger(format("[a]pplication.. " BS_LOG_TABLE) % levels::application_defined % toggle_.at(levels::application));
    logger(format("[n]ews......... " BS_LOG_TABLE) % levels::news_defined % toggle_.at(levels::news));
    logger(format("[s]ession...... " BS_LOG_TABLE) % levels::session_defined % toggle_.at(levels::session));
    logger(format("[p]rotocol..... " BS_LOG_TABLE) % levels::protocol_defined % toggle_.at(levels::protocol));
    logger(format("[x]proxy....... " BS_LOG_TABLE) % levels::proxy_defined % toggle_.at(levels::proxy));
    logger(format("[r]emote....... " BS_LOG_TABLE) % levels::remote_defined % toggle_.at(levels::remote));
    logger(format("[f]ault........ " BS_LOG_TABLE) % levels::fault_defined % toggle_.at(levels::fault));
    logger(format("[q]uitting..... " BS_LOG_TABLE) % levels::quitting_defined % toggle_.at(levels::quitting));
    logger(format("[o]bjects...... " BS_LOG_TABLE) % levels::objects_defined % toggle_.at(levels::objects));
    logger(format("[v]erbose...... " BS_LOG_TABLE) % levels::verbose_defined % toggle_.at(levels::verbose));
}

// query_ not valid unless store is loaded.
void executor::dump_configuration() const
{
    logger(format(BS_INFORMATION_START)
        % store_.is_dirty()
        % query_.interval_span());
}

void executor::dump_body_sizes() const
{
    logger(format(BS_INFORMATION_SIZES) %
        query_.header_body_size() %
        query_.txs_body_size() %
        query_.tx_body_size() %
        query_.input_body_size() %
        query_.output_body_size() %
        query_.ins_body_size() %
        query_.outs_body_size() %
        query_.candidate_body_size() %
        query_.confirmed_body_size() %
        query_.ecdsa_body_size() %
        query_.schnorr_body_size() %
        query_.silent_body_size() %
        query_.prevalid_body_size() %
        query_.prevout_body_size() %
        query_.duplicate_body_size() %
        query_.strong_tx_body_size() %
        query_.state_body_size() %
        query_.pool_body_size() %
        query_.spends_body_size() %
        query_.filter_bk_body_size() %
        query_.filter_tx_body_size());
}

void executor::dump_records() const
{
    logger(format(BS_INFORMATION_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.ins_records() %
        query_.outs_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.ecdsa_records() %
        query_.schnorr_records() %
        query_.silent_records() %
        query_.prevalid_records() %
        query_.duplicate_records() %
        query_.strong_tx_records() %
        query_.state_records() %
        query_.pool_records() %
        query_.spends_records() %
        query_.filter_bk_records());
}

void executor::dump_buckets() const
{
    logger(format(BS_INFORMATION_BUCKETS) %
        query_.header_buckets() %
        query_.txs_buckets() %
        query_.tx_buckets() %
        query_.ins_buckets() %
        query_.outs_buckets() %
        query_.prevout_buckets() %
        query_.duplicate_buckets() %
        query_.strong_tx_buckets() %
        query_.state_buckets() %
        query_.pool_buckets() %
        query_.filter_bk_buckets() %
        query_.filter_tx_buckets());
}

void executor::dump_collisions() const
{
    const auto rate = [](size_t records, size_t buckets)
    {
        return is_zero(buckets) ? 0.0 : to_double(records) / buckets;
    };

    const auto header = rate(query_.header_records(), query_.header_buckets());
    const auto tx = rate(query_.tx_records(), query_.tx_buckets());
    const auto ins = rate(query_.ins_records(), query_.ins_buckets());
    const auto strong_tx = rate(query_.strong_tx_records(),
        query_.strong_tx_buckets());
    const auto pool = rate(query_.pool_records(),
        query_.pool_buckets());

    if (query_.address_enabled())
    {
        const auto outs = rate(query_.outs_records(), query_.outs_buckets());
        logger(format(BS_INFORMATION_COLLISION_RATES_ADDRESS) %
            header % tx % ins % outs % strong_tx % pool);
        return;
    }

    logger(format(BS_INFORMATION_COLLISION_RATES) %
        header % tx % ins % strong_tx % pool);
}

void executor::dump_progress() const
{
    using namespace system;

    logger(format(BS_INFORMATION_PROGRESS) %
        query_.get_fork() %
        query_.get_top_confirmed() %
        encode_hash(query_.get_top_confirmed_hash()) %
        query_.get_top_candidate() %
        encode_hash(query_.get_top_candidate_hash()) %
        query_.get_top_associated() %
        (query_.get_top_candidate() - query_.get_unassociated_count()) %
        query_.get_confirmed_size() %
        query_.get_candidate_size());
}

} // namespace server
} // namespace libbitcoin
