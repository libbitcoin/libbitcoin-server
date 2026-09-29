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
#include "../test.hpp"
#include "../mocks/blocks.hpp"

using namespace system;

static std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

// bitcoind(header)
// ----------------------------------------------------------------------------
// The serializer moved to system, this guards the fields consumed by rpc.

BOOST_AUTO_TEST_SUITE(bitcoind_header_to_bitcoind_tests)

BOOST_AUTO_TEST_CASE(bitcoind_json__header_to_bitcoind__block1_header__maps_fields)
{
    const auto& header = test::block1.header();
    const auto out = boost::json::value_from(bitcoind(header)).as_object();

    BOOST_REQUIRE_EQUAL(as_text(out.at("hash")), encode_hash(header.hash()));
    BOOST_REQUIRE_EQUAL(out.at("version").to_number<int64_t>(), header.version());
    BOOST_REQUIRE_EQUAL(as_text(out.at("versionHex")), encode_base16(to_big_endian(header.version())));
    BOOST_REQUIRE_EQUAL(as_text(out.at("merkleroot")), encode_hash(header.merkle_root()));
    BOOST_REQUIRE_EQUAL(out.at("time").to_number<uint64_t>(), header.timestamp());
    BOOST_REQUIRE_EQUAL(out.at("nonce").to_number<uint64_t>(), header.nonce());
    BOOST_REQUIRE_EQUAL(as_text(out.at("bits")), encode_base16(to_big_endian(header.bits())));
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
    BOOST_REQUIRE_EQUAL(chain_name(query_), "main");
}

// median_time

// The median of mainnet block 0..5 timestamps (self-inclusive, as bitcoind).
BOOST_AUTO_TEST_CASE(bitcoind_json__median_time__self_inclusive_window)
{
    const system::settings settings{ chain::selection::mainnet };
    const auto link = query_.to_header(test::block5_hash);
    BOOST_REQUIRE_EQUAL(median_time(query_, settings, link), 1231470173u);
}

// inject_block_context

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__middle__height_confirmations_siblings)
{
    const system::settings settings{ chain::selection::mainnet };
    const auto link = query_.to_header(test::block5_hash);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);

    boost::json::object out{};
    inject_block_context(out, query_, settings, link, *header);

    BOOST_REQUIRE_EQUAL(out.at("height").to_number<uint64_t>(), 5u);
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 5);
    BOOST_REQUIRE_EQUAL(out.at("mediantime").to_number<uint32_t>(), 1231470173u);
    BOOST_REQUIRE_EQUAL(as_text(out.at("previousblockhash")), encode_hash(test::block4_hash));
    BOOST_REQUIRE_EQUAL(as_text(out.at("nextblockhash")), encode_hash(test::block6_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__genesis__no_previous)
{
    const system::settings settings{ chain::selection::mainnet };
    const auto link = query_.to_header(test::block0_hash);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);

    boost::json::object out{};
    inject_block_context(out, query_, settings, link, *header);

    BOOST_REQUIRE_EQUAL(out.at("height").to_number<uint64_t>(), 0u);
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 10);
    BOOST_REQUIRE(!out.contains("previousblockhash"));
    BOOST_REQUIRE_EQUAL(as_text(out.at("nextblockhash")), encode_hash(test::block1_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__top__no_next)
{
    const system::settings settings{ chain::selection::mainnet };
    const auto link = query_.to_header(test::block9_hash);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);

    boost::json::object out{};
    inject_block_context(out, query_, settings, link, *header);

    BOOST_REQUIRE_EQUAL(out.at("height").to_number<uint64_t>(), 9u);
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 1);
    BOOST_REQUIRE_EQUAL(as_text(out.at("previousblockhash")), encode_hash(test::block8_hash));
    BOOST_REQUIRE(!out.contains("nextblockhash"));
}

// inject_tx_context

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_tx_context__confirmed_coinbase__block_context)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto link = query_.to_tx(txid);

    boost::json::object out{};
    inject_tx_context(out, query_, link);

    BOOST_REQUIRE(out.at("in_active_chain").as_bool());
    BOOST_REQUIRE_EQUAL(as_text(out.at("blockhash")), encode_hash(test::block1_hash));
    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 9);
    BOOST_REQUIRE_EQUAL(out.at("blocktime").to_number<uint64_t>(), test::block1.header().timestamp());
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_tx_context__unknown__zero_confirmations)
{
    const auto link = query_.to_tx(null_hash);

    boost::json::object out{};
    inject_tx_context(out, query_, link);

    BOOST_REQUIRE_EQUAL(out.at("confirmations").to_number<int64_t>(), 0);
    BOOST_REQUIRE(!out.contains("blockhash"));
    BOOST_REQUIRE(!out.contains("in_active_chain"));
}

// inject_block_context (unknown)

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_block_context__unknown__unchanged)
{
    const system::settings settings{ chain::selection::mainnet };
    boost::json::object out{};
    inject_block_context(out, query_, settings, database::header_link{}, test::block1.header());
    BOOST_REQUIRE(out.empty());
}

// chain_states_entry

BOOST_AUTO_TEST_CASE(bitcoind_json__chain_states_entry__top__expected)
{
    const auto link = query_.to_header(test::block9_hash);
    const auto out = chain_states_entry(query_, link, 1.0, true);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(out.at("blocks").value()), 9u);
    BOOST_REQUIRE_EQUAL(std::get<network::rpc::string_t>(out.at("bestblockhash").value()), encode_hash(test::block9_hash));
    BOOST_REQUIRE_EQUAL(std::get<network::rpc::string_t>(out.at("bits").value()), "1d00ffff");
    BOOST_REQUIRE(std::get<bool>(out.at("validated").value()));
}

BOOST_AUTO_TEST_CASE(bitcoind_json__chain_states_entry__unknown__empty)
{
    BOOST_REQUIRE(chain_states_entry(query_, database::header_link{}, 1.0, true).empty());
}

// inject_activity

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_activity__watched_output__receive_and_spend)
{
    using namespace network::rpc;
    BOOST_REQUIRE(query_.set(test::mock_block13, database::context{ 0, 10, 0 }, {}, false, false));
    const auto block = query_.get_block(query_.to_header(test::mock_block13.hash()), false);
    BOOST_REQUIRE(block);
    BOOST_REQUIRE(query_.populate_without_metadata(*block));

    const auto& txs = *test::mock_block13.transactions_ptr();
    const auto blockhash = encode_hash(test::mock_block13.hash());
    const auto script = test::mock_tx13.outputs_ptr()->front()->script().to_data(false);
    const std::unordered_set<std::string> watch{ encode_base16(script) };

    array_t out{};
    inject_activity(out, *block, 10, blockhash, watch, 0x00, 0x05, "bc", 0);
    BOOST_REQUIRE_EQUAL(out.size(), 2u);

    const auto& receive = std::get<object_t>(out.at(0).value());
    BOOST_REQUIRE_EQUAL(std::get<string_t>(receive.at("type").value()), "receive");
    BOOST_REQUIRE_EQUAL(std::get<number_t>(receive.at("amount").value()), 9 / 100'000'000.0);
    BOOST_REQUIRE_EQUAL(std::get<string_t>(receive.at("blockhash").value()), blockhash);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(receive.at("height").value()), 10u);
    BOOST_REQUIRE_EQUAL(std::get<string_t>(receive.at("txid").value()), encode_hash(test::mock_tx13.hash(false)));
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(receive.at("vout").value()), 0u);

    const auto& spend = std::get<object_t>(out.at(1).value());
    BOOST_REQUIRE_EQUAL(std::get<string_t>(spend.at("type").value()), "spend");
    BOOST_REQUIRE_EQUAL(std::get<number_t>(spend.at("amount").value()), 9 / 100'000'000.0);
    BOOST_REQUIRE_EQUAL(std::get<string_t>(spend.at("blockhash").value()), blockhash);
    BOOST_REQUIRE_EQUAL(std::get<uint64_t>(spend.at("height").value()), 10u);
    BOOST_REQUIRE_EQUAL(std::get<string_t>(spend.at("spend_txid").value()), encode_hash(txs.back()->hash(false)));
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(spend.at("spend_vin").value()), 0u);
    BOOST_REQUIRE_EQUAL(std::get<string_t>(spend.at("prevout_txid").value()), encode_hash(test::mock_tx13.hash(false)));
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(spend.at("prevout_vout").value()), 0u);

    const auto& prevout_spk = std::get<json_t>(spend.at("prevout_spk").value());
    BOOST_REQUIRE_EQUAL(as_text(prevout_spk.at("address")), "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM");
}

BOOST_AUTO_TEST_CASE(bitcoind_json__inject_activity__unwatched__empty)
{
    network::rpc::array_t out{};
    inject_activity(out, test::block1, 1, encode_hash(test::block1_hash), {}, 0x00, 0x05, "bc", 0);
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_SUITE_END()
