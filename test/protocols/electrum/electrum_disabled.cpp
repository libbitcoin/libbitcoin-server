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
#include "electrum.hpp"

using namespace system;
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };
static const std::string bogus_address{ "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn" };
static const std::string bogus_scripthash{ "9c2c84a6cf9809e08af19557e28d38257e6fee6981269760637a5f9dfb000b05" };
static const chain::script bogus{ chain::script::to_pay_key_hash_pattern({ 0x42 }) };
static const auto bogus_script = encode_base16(bogus.to_data(false));

BOOST_FIXTURE_TEST_SUITE(electrum_disabled_address_tests, electrum_disabled_address_index_setup_fixture)

// address

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":901,"method":"blockchain.address.get_balance","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn").str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_history__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1001,"method":"blockchain.address.get_history","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn").str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1001,"method":"blockchain.address.get_mempool","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn").str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_0));

    const auto request = R"({"id":1001,"method":"blockchain.address.listunspent","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn").str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

// scripthash

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_get_balance__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":901,"method":"blockchain.scripthash.get_balance","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_get_history__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":1001,"method":"blockchain.scripthash.get_history","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_get_mempool__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":1001,"method":"blockchain.scripthash.get_mempool","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_list_unspent__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":1001,"method":"blockchain.scripthash.listunspent","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":1001,"method":"blockchain.address.subscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_address).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_1));

    const auto request = R"({"id":1001,"method":"blockchain.scripthash.subscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scripthash_unsubscribe__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_4_2));

    const auto request = R"({"id":1001,"method":"blockchain.scripthash.unsubscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_scripthash).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.subscribe","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

// scriptpubkey

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":901,"method":"blockchain.scriptpubkey.get_balance","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.get_history","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.get_mempool","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__no_address_index__not_implemented)
{
    BOOST_REQUIRE(!query_.address_enabled());
    BOOST_REQUIRE(handshake(electrum::version::v1_7));

    const auto request = R"({"id":1001,"method":"blockchain.scriptpubkey.listunspent","params":["%1%"]})" "\n";
    const auto result = get_error((boost::format(request) % bogus_script).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_SUITE_END()
