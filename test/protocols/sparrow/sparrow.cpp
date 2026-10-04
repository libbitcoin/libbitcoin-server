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
#include "../electrum/electrum_setup_fixture.hpp"

using namespace system;

static const code not_implemented{ server::error::electrum::method_not_found };
static const code bad_request{ server::error::electrum::bad_request };

BOOST_FIXTURE_TEST_SUITE(sparrow_tests, sparrow_ten_block_setup_fixture)

// inherited electrum interface

BOOST_AUTO_TEST_CASE(sparrow__handshake__electrum_version__negotiated)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_numblocks_subscribe__ten_block_store__returns_9)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto response = get(R"({"id":900,"method":"blockchain.numblocks.subscribe","params":[]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(sparrow__unknown_method__electrum_terminal__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":901,"method":"server.bogus","params":[]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

// server.features

BOOST_AUTO_TEST_CASE(sparrow__server_features__silent_payments__advertised)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":902,"method":"server.features","params":[]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    REQUIRE_NO_THROW_TRUE(result.at("silent_payments").is_array());

    const auto& versions = result.at("silent_payments").as_array();
    BOOST_REQUIRE_EQUAL(versions.size(), one);
    BOOST_REQUIRE_EQUAL(versions.at(0).as_int64(), server::protocol_sparrow::silent_payments_version);
    BOOST_REQUIRE_EQUAL(result.at("server_version").as_string(), "server_name");
}

// sparrow interface

BOOST_AUTO_TEST_CASE(sparrow__blockchain_block_stats__stub__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":903,"method":"blockchain.block.stats","params":[5]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_block_stats__missing_arguments__dropped)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":903,"method":"blockchain.block.stats","params":[]})" "\n");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_silentpayments_subscribe__not_indexed__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto request = R"({"id":904,"method":"blockchain.silentpayments.subscribe","params":["0000000000000000000000000000000000000000000000000000000000000001","020000000000000000000000000000000000000000000000000000000000000002"]})" "\n";
    BOOST_REQUIRE_EQUAL(get_error(request), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_silentpayments_unsubscribe__not_subscribed__null)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":905,"method":"blockchain.silentpayments.unsubscribe","params":["0000000000000000000000000000000000000000000000000000000000000001","020000000000000000000000000000000000000000000000000000000000000002"]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(sparrow__blockchain_silentpayments_unsubscribe__invalid_scan_key__bad_request)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto request = R"({"id":906,"method":"blockchain.silentpayments.unsubscribe","params":["0000000000000000000000000000000000000000000000000000000000000000","020000000000000000000000000000000000000000000000000000000000000002"]})" "\n";
    BOOST_REQUIRE_EQUAL(get_error(request), bad_request.value());
}

BOOST_AUTO_TEST_SUITE_END()

// sparrow silent payments (indexed)

// BIP352 send_and_receive_test_vectors.json: "Simple send: two inputs".
static chain::input to_input(const hash_digest& hash, const data_chunk& script,
    const data_chunk& prevout) NOEXCEPT
{
    chain::input in{ { hash, 0 }, { script, false }, {}, max_uint32 };
    const chain::script prevout_script{ prevout, false };
    in.prevout = to_shared<chain::output>(0u, prevout_script);
    return in;
}

static const chain::transaction simple_send
{
    2u,
    chain::inputs
    {
        to_input
        (
            base16_hash("f4184fc596403b9d638783cf57adfe4c75c605f6356fbc91338530e9831e9e16"),
            base16_chunk("483046022100ad79e6801dd9a8727f342f31c71c4912866f59dc6e7981878e92c5844a0ce929022100fb0d2393e813968648b9753b7e9871d90ab3d815ebf91820d704b19f4ed224d621025a1e61f898173040e20616d43e9f496fba90338a39faa1ed98fcbaeee4dd9be5"),
            base16_chunk("76a91419c2f3ae0ca3b642bd3e49598b8da89f50c1416188ac")
        ),
        to_input
        (
            base16_hash("a1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"),
            base16_chunk("48304602210086783ded73e961037e77d49d9deee4edc2b23136e9728d56e4491c80015c3a63022100fda4c0f21ea18de29edbce57f7134d613e044ee150a89e2e64700de2d4e83d4e2103bd85685d03d111699b15d046319febe77f8de5286e9e512703cdee1bf3be3792"),
            base16_chunk("76a914d9317c66f54ff0a152ec50b1d19c25be50c8e15988ac")
        )
    },
    chain::outputs
    {
        { 0u, chain::script{ base16_chunk("51203e9fce73d4e77a4809908e3c3a2e54ee147b9312dc5044a193d1fc85de46e3c1"), false } }
    },
    0u
};

/// Silent payments enabled from genesis, with the simple send (unconfirmed)
/// archived and indexed.
struct sparrow_silent_setup_fixture
  : electrum_setup_fixture
{
    inline sparrow_silent_setup_fixture()
      : electrum_setup_fixture([](test::query_t& query)
        {
            database::tx_link link{};
            return test::setup_ten_block_store(query)
                && !query.set_code(link, simple_send)
                && query.set_silent(link, simple_send);
        }, true, [](configuration& config)
        {
            config.node.silent_start_height = 0;
            config.database.initialize(config.bitcoin, false, 0);
        }, service::sparrow)
    {
    }
};

BOOST_FIXTURE_TEST_SUITE(sparrow_silent_tests, sparrow_silent_setup_fixture)

BOOST_AUTO_TEST_CASE(sparrow__blockchain_silentpayments_subscribe__indexed__result_then_history)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":908,"method":"blockchain.silentpayments.subscribe","params":["0f694e068028a717f8af6b9411f9a133dd3565258714cc226594b34db90c1f2c","025cc9856d6f8375350e123978daac200c260cb5b5ae83106cab90484dcd8fcf36"]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("address").as_string(), "sp1qqgste7k9hx0qftg6qmwlkqtwuy6cycyavzmzj85c6qdfhjdpdjtdgqjuexzk6murw56suy3e0rd2cgqvycxttddwsvgxe2usfpxumr70xc9pkqwv");
    BOOST_REQUIRE_EQUAL(result.at("start_height").to_number<uint64_t>(), 0u);
    BOOST_REQUIRE_EQUAL(result.at("labels").as_array().size(), 1u);

    const auto notification = receive();
    REQUIRE_NO_THROW_TRUE(notification.at("method").is_string());
    BOOST_REQUIRE_EQUAL(notification.at("method").as_string(), "blockchain.silentpayments.subscribe");

    const auto& params = notification.at("params").as_object();
    BOOST_REQUIRE_EQUAL(params.at("progress").to_number<double>(), 1.0);
    BOOST_REQUIRE_EQUAL(params.at("subscription").as_object().at("address").as_string(), result.at("address").as_string());

    const auto& history = params.at("history").as_array();
    BOOST_REQUIRE_EQUAL(history.size(), 1u);

    const auto& entry = history.at(0).as_object();
    BOOST_REQUIRE_EQUAL(entry.at("height").to_number<uint64_t>(), 0u);
    BOOST_REQUIRE_EQUAL(entry.at("tx_hash").as_string(), encode_hash(simple_send.hash(false)));
    BOOST_REQUIRE_EQUAL(entry.at("tweak_key").as_string(), "024ac253c216532e961988e2a8ce266a447c894c781e52ef6cee902361db960004");
}

BOOST_AUTO_TEST_SUITE_END()

// electrum does not serve the sparrow interface

BOOST_FIXTURE_TEST_SUITE(sparrow_electrum_tests, electrum_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_stats__not_sparrow__method_not_found)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto result = get_error(R"({"id":906,"method":"blockchain.block.stats","params":[5]})" "\n");
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__server_features__not_sparrow__no_silent_payments)
{
    BOOST_REQUIRE(handshake(electrum::version::v1_4));

    const auto response = get(R"({"id":907,"method":"server.features","params":[]})" "\n");
    BOOST_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE(!response.at("result").as_object().contains("silent_payments"));
}

BOOST_AUTO_TEST_SUITE_END()
