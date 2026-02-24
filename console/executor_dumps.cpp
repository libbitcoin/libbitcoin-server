/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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

using boost::format;

constexpr double to_double(auto integer)
{
    return 1.0 * integer;
}

// Store dumps.
// ----------------------------------------------------------------------------

// emit version information for libbitcoin libraries
void executor::dump_version() const
{
    logger(format(BS_VERSION_MESSAGE)
        % LIBBITCOIN_SERVER_VERSION
        % LIBBITCOIN_NODE_VERSION
        % LIBBITCOIN_NETWORK_VERSION
        % LIBBITCOIN_DATABASE_VERSION
        % LIBBITCOIN_SYSTEM_VERSION);
}

// The "try" functions are safe for instructions not compiled in.
void executor::dump_hardware() const
{
    using namespace system;

    logger(BS_HARDWARE_HEADER);
    logger(format("arm..... " BS_HARDWARE_TABLE1) % have_arm);
    logger(format("intel... " BS_HARDWARE_TABLE1) % have_xcpu);
    logger(format("avx512.. " BS_HARDWARE_TABLE2) % try_avx512() % have_512);
    logger(format("avx2.... " BS_HARDWARE_TABLE2) % try_avx2() % have_256);
    logger(format("sse41... " BS_HARDWARE_TABLE2) % try_sse41() % have_128);
    logger(format("shani... " BS_HARDWARE_TABLE2) % try_shani() % have_sha);
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
        query_.point_body_size() %
        query_.input_body_size() %
        query_.output_body_size() %
        query_.ins_body_size() %
        query_.outs_body_size() %
        query_.candidate_body_size() %
        query_.confirmed_body_size() %
        query_.duplicate_body_size() %
        query_.prevout_body_size() %
        query_.strong_tx_body_size() %
        query_.validated_bk_body_size() %
        query_.validated_tx_body_size() %
        query_.filter_bk_body_size() %
        query_.filter_tx_body_size() %
        query_.address_body_size());
}

void executor::dump_records() const
{
    logger(format(BS_INFORMATION_RECORDS) %
        query_.header_records() %
        query_.tx_records() %
        query_.point_records() %
        query_.ins_records() %
        query_.outs_records() %
        query_.candidate_records() %
        query_.confirmed_records() %
        query_.duplicate_records() %
        query_.strong_tx_records() %
        query_.filter_bk_records() %
        query_.address_records());
}

void executor::dump_buckets() const
{
    logger(format(BS_INFORMATION_BUCKETS) %
        query_.header_buckets() %
        query_.txs_buckets() %
        query_.tx_buckets() %
        query_.point_buckets() %
        query_.duplicate_buckets() %
        query_.prevout_buckets() %
        query_.strong_tx_buckets() %
        query_.validated_bk_buckets() %
        query_.validated_tx_buckets() %
        query_.filter_bk_buckets() %
        query_.filter_tx_buckets() %
        query_.address_buckets());
}

void executor::dump_collisions() const
{
    logger(format(BS_INFORMATION_COLLISION_RATES) %
        (to_double(query_.header_records()) / query_.header_buckets()) %
        (to_double(query_.tx_records()) / query_.tx_buckets()) %
        (to_double(query_.point_records()) / query_.point_buckets()) %
        (to_double(query_.strong_tx_records()) / query_.strong_tx_buckets()) %
        (to_double(query_.tx_records()) / query_.validated_tx_buckets()) %
        (query_.address_enabled() ? (to_double(query_.address_records()) / 
            query_.address_buckets()) : zero));
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
