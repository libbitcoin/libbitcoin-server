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
#include "test.hpp"
#include <atomic>

namespace std {

std::ostream& operator<<(std::ostream& stream,
    const system::data_slice& slice) NOEXCEPT
{
    // Avoid serialize() here for its own test benefit.
    // stream << serialize(slice);
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    stream << encode_base16(slice);
    BC_POP_WARNING()
    return stream;
}

} // namespace std

namespace test {

const std::string directory = "tests";

bool clear(const std::filesystem::path& directory) NOEXCEPT
{
    // remove_all returns count removed, and error code if fails.
    // create_directories returns true if path exists or created.
    // used for setup, with no expectations of file/directory existence.
    const auto path = system::extended_path(directory);
    code ec;
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    std::filesystem::remove_all(path, ec);
    return !ec && std::filesystem::create_directories(path, ec);
    BC_POP_WARNING()
}

bool folder(const std::filesystem::path& directory) NOEXCEPT
{
    const auto path = system::extended_path(directory);
    code ec;
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return !ec && std::filesystem::is_directory(path, ec);
    BC_POP_WARNING()
}

bool create(const std::filesystem::path& file_path) NOEXCEPT
{
    // Creates and returns true if file existed or not (and no error).
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    std::ofstream file(system::extended_path(file_path));
    const auto good = file.good();
    file.close();
    BC_POP_WARNING()
    return good;
}

bool exists(const std::filesystem::path& file_path) NOEXCEPT
{
    // Returns true only if file existed.
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    std::ifstream file(system::extended_path(file_path));
    const auto good = file.good();
    file.close();
    BC_POP_WARNING()
    return good;
}

bool remove(const std::filesystem::path& file_path) NOEXCEPT
{
    // Deletes and returns false if file did not exist (or error).
    code ec;
    return std::filesystem::remove(system::extended_path(file_path), ec);
}

static database::settings database_settings() NOEXCEPT
{
    const auto simple = [](database::settings::simple_table& table) NOEXCEPT
    {
        table.size = 1;
        table.rate = 5;
    };

    const auto bucket = [&](database::settings::bucket_table& table) NOEXCEPT
    {
        simple(table);
        table.buckets = 128;
    };

    const auto hash = [&](database::settings::hash_table& table) NOEXCEPT
    {
        bucket(table);
        table.expected = 0;
    };

    database::settings out{};
    out.network = system::chain::selection::none;
    out.turbo = false;
    out.gpu_scan_minimum = 65'536;
    out.mark_unconfirmable = true;
    out.interval_depth = max_uint8;
    out.path = "bitcoin";

    hash(out.header);
    simple(out.input);
    simple(out.output);
    hash(out.ins);
    hash(out.outs);
    hash(out.tx);
    bucket(out.txs);

    bucket(out.candidate);
    bucket(out.confirmed);
    bucket(out.strong);
    bucket(out.scan);
    hash(out.wtxid);

    simple(out.ecdsa);
    simple(out.schnorr);
    simple(out.silent);
    hash(out.duplicate);
    simple(out.prevalid);
    simple(out.presilent);
    bucket(out.prevout);
    bucket(out.state);
    hash(out.pool);
    simple(out.spends);

    bucket(out.filter_bk);
    bucket(out.filter_tx);

    out.envelope.set(out);
    return out;
}

static network::settings network_settings() NOEXCEPT
{
    using namespace network::messages::peer;
    network::settings out{ system::chain::selection::none };
    out.network = system::chain::selection::mainnet;
    out.identifier = 3652501241;
    out.protocol_maximum = level::maximum_protocol;
    out.protocol_minimum = level::minimum_protocol;
    out.enable_address = false;
    out.enable_address_v2 = true;
    out.enable_witness_tx = false;
    out.enable_compact = false;
    out.enable_not_found = false;
    out.enable_relay = false;
    out.maximum_skew_minutes = 120;

    out.outbound.connections = 10;
    out.outbound.connect_batch_size = 5;
    out.outbound.host_pool_capacity = 0;
    out.outbound.seeds.clear();
    out.outbound.seeds.emplace_back("mainnet1.libbitcoin.net", 8333_u16);
    out.outbound.seeds.emplace_back("mainnet2.libbitcoin.net", 8333_u16);
    out.outbound.seeds.emplace_back("mainnet3.libbitcoin.net", 8333_u16);
    out.outbound.seeds.emplace_back("mainnet4.libbitcoin.net", 8333_u16);

    out.inbound.connections = 0;
    out.inbound.binds.clear();
    out.inbound.binds.emplace_back(system::ip::address{}, 8333_u16);
    return out;
}

static node::settings node_settings() NOEXCEPT
{
    node::settings out{};
    out.network = system::chain::selection::mainnet;
    out.batch_verify = 0;
    out.batch_silent = 0;
    out.minimum_fee_rate = 0.0;
    out.maximum_concurrency = 50'000;
    return out;
}

void configure(server::configuration& out) NOEXCEPT
{
    const auto service = [](network::settings::tcp_server& value) NOEXCEPT
    {
        value.maximum_request = network::max_payload;
        value.maximum_backlog = network::megabyte;
        value.inactivity_minutes = 10;
        value.expiration_minutes = 60;
    };

    constexpr auto mainnet = system::chain::selection::mainnet;
    out.database = database_settings();
    out.network = network_settings();
    out.node = node_settings();

    auto& services = out.server;
    services.network = mainnet;
    services.wallet = server::settings::wallet_settings{ mainnet };
    service(services.admin);
    service(services.native);
    service(services.bitcoind);
    service(services.btcd);
    service(services.electrum);
    service(services.sparrow);
    service(services.esplora);
    service(services.stratum_v1);
    service(services.zmq);
    service(services.stratum_v2);
}

} // namespace test