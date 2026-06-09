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
#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include <algorithm>
#include <vector>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

using namespace system;

static std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

// Exposes the protected static json helpers for direct testing.
struct json
  : server::protocol_bitcoind_rpc
{
    using protocol_bitcoind_rpc::median_time_past;
    using protocol_bitcoind_rpc::inject_block_context;
    using protocol_bitcoind_rpc::inject_tx_context;
    using protocol_bitcoind_rpc::header_to_bitcoind;
    using protocol_bitcoind_rpc::chain_name;
};

// header_to_bitcoind
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(bitcoind_header_to_bitcoind_tests)

BOOST_AUTO_TEST_CASE(bitcoind_json__header_to_bitcoind__block1_header__maps_fields)
{
    const auto& header = test::block1.header();
    const auto out = json::header_to_bitcoind(header);

    BOOST_REQUIRE_EQUAL(as_text(out.at("hash")), encode_hash(header.hash()));
    BOOST_REQUIRE_EQUAL(out.at("version").to_number<int64_t>(), header.version());
    BOOST_REQUIRE_EQUAL(as_text(out.at("versionHex")),
        encode_base16(to_big_endian(header.version())));
    BOOST_REQUIRE_EQUAL(as_text(out.at("merkleroot")),
        encode_hash(header.merkle_root()));
    BOOST_REQUIRE_EQUAL(out.at("time").to_number<uint64_t>(), header.timestamp());
    BOOST_REQUIRE_EQUAL(out.at("nonce").to_number<uint64_t>(), header.nonce());
    BOOST_REQUIRE_EQUAL(as_text(out.at("bits")),
        encode_base16(to_big_endian(header.bits())));
    BOOST_REQUIRE(out.at("difficulty").is_number());
    BOOST_REQUIRE(!out.contains("height"));
    BOOST_REQUIRE(!out.contains("confirmations"));
}

BOOST_AUTO_TEST_SUITE_END()

// ----------------------------------------------------------------------------

struct bitcoind_json_setup_fixture
{
    DELETE_COPY_MOVE(bitcoind_json_setup_fixture);

    bitcoind_json_setup_fixture()
      : config_
        {
            system::chain::selection::mainnet,
            test::web_pages,
            test::web_pages
        },
        store_
        {
            [this]() NOEXCEPT -> const database::settings&
            {
                config_.database.path = TEST_DIRECTORY;
                return config_.database;
            }()
        },
        query_{ store_ }
    {
        BOOST_REQUIRE_MESSAGE(test::clear(test::directory), "json setup");
        config_.database.interval_depth = 2;

        const auto ec = store_.create([](auto, auto) {});
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
        BOOST_REQUIRE_MESSAGE(test::setup_ten_block_store(query_),
            "json initialize");
    }

    ~bitcoind_json_setup_fixture()
    {
        const auto ec = store_.close([](auto, auto) {});
        BOOST_WARN_MESSAGE(!ec, ec.message());
        BOOST_WARN_MESSAGE(test::clear(test::directory), "json cleanup");
    }

    configuration config_;
    test::store_t store_;
    test::query_t query_;
};

BOOST_FIXTURE_TEST_SUITE(bitcoind_json_tests, bitcoind_json_setup_fixture)

// chain_name

BOOST_AUTO_TEST_CASE(bitcoind_json__chain_name__mainnet_genesis__main)
{
    BOOST_REQUIRE_EQUAL(json::chain_name(query_), "main");
}

// median_time_past

BOOST_AUTO_TEST_CASE(bitcoind_json__median_time_past__genesis__genesis_time)
{
    BOOST_REQUIRE_EQUAL(json::median_time_past(query_, 0),
        test::genesis.header().timestamp());
}

BOOST_AUTO_TEST_CASE(bitcoind_json__median_time_past__height_nine__sorted_median)
{
    std::vector<uint32_t> times
    {
        test::genesis.header().timestamp(),
        test::block1.header().timestamp(),
        test::block2.header().timestamp(),
        test::block3.header().timestamp(),
        test::block4.header().timestamp(),
        test::block5.header().timestamp(),
        test::block6.header().timestamp(),
        test::block7.header().timestamp(),
        test::block8.header().timestamp(),
        test::block9.header().timestamp()
    };

    std::sort(times.begin(), times.end());
    BOOST_REQUIRE_EQUAL(json::median_time_past(query_, 9),
        times.at(times.size() / 2u));
}

// inject_block_context

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__middle__height_confirmations_siblings)
{
    const auto link = query_.to_header(test::block5_hash);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);

    boost::json::object out{};
    json::inject_block_context(out, query_, link, *header);

    BOOST_REQUIRE_EQUAL(out.at("height").to_number<uint64_t>(), 5u);
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 5);
    BOOST_REQUIRE_EQUAL(out.at("mediantime").to_number<uint64_t>(),
        json::median_time_past(query_, 5));
    BOOST_REQUIRE_EQUAL(as_text(out.at("previousblockhash")),
        encode_hash(test::block4_hash));
    BOOST_REQUIRE_EQUAL(as_text(out.at("nextblockhash")),
        encode_hash(test::block6_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__genesis__no_previous)
{
    const auto link = query_.to_header(test::block0_hash);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);

    boost::json::object out{};
    json::inject_block_context(out, query_, link, *header);

    BOOST_REQUIRE_EQUAL(out.at("height").to_number<uint64_t>(), 0u);
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 10);
    BOOST_REQUIRE(!out.contains("previousblockhash"));
    BOOST_REQUIRE_EQUAL(as_text(out.at("nextblockhash")),
        encode_hash(test::block1_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__tip__no_next)
{
    const auto link = query_.to_header(test::block9_hash);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);

    boost::json::object out{};
    json::inject_block_context(out, query_, link, *header);

    BOOST_REQUIRE_EQUAL(out.at("height").to_number<uint64_t>(), 9u);
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 1);
    BOOST_REQUIRE_EQUAL(as_text(out.at("previousblockhash")),
        encode_hash(test::block8_hash));
    BOOST_REQUIRE(!out.contains("nextblockhash"));
}

// inject_tx_context

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_tx_context__confirmed_coinbase__block_context)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto link = query_.to_tx(txid);

    boost::json::object out{};
    json::inject_tx_context(out, query_, link);

    BOOST_REQUIRE(out.at("in_active_chain").as_bool());
    BOOST_REQUIRE_EQUAL(as_text(out.at("blockhash")),
        encode_hash(test::block1_hash));
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 9);
    BOOST_REQUIRE_EQUAL(out.at("blocktime").to_number<uint64_t>(),
        test::block1.header().timestamp());
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_tx_context__unknown__zero_confirmations)
{
    const auto link = query_.to_tx(null_hash);

    boost::json::object out{};
    json::inject_tx_context(out, query_, link);

    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 0);
    BOOST_REQUIRE(!out.contains("blockhash"));
    BOOST_REQUIRE(!out.contains("in_active_chain"));
}

BOOST_AUTO_TEST_SUITE_END()
