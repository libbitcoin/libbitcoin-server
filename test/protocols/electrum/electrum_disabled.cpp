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
#include "../../test.hpp"
#include "electrum_setup_fixture.hpp"

using namespace system;
static const code not_implemented{ server::error::electrum::method_not_found };
static const code invalid_argument{ server::error::electrum::bad_request };
static const std::string bogus_address{ "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn" };
static const std::string bogus_scripthash{ "9c2c84a6cf9809e08af19557e28d38257e6fee6981269760637a5f9dfb000b05" };
static const chain::script bogus{ chain::script::to_pay_key_hash_pattern({ 0x42 }) };
static const auto bogus_script = encode_base16(bogus.to_data(false));

BC_FIXTURE_TEST_SUITE(electrum_disabled_address_tests, electrum_disabled_address_index_setup_fixture)

// address

BC_AUTO_TEST_CASE(electrum__blockchain_address_get_balance__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto request = R"({{"id":901,"method":"blockchain.address.get_balance","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn"));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_address_get_history__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.address.get_history","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn"));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_address_get_mempool__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.address.get_mempool","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn"));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_address_list_unspent__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_0));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.address.listunspent","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn"));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

// scripthash

BC_AUTO_TEST_CASE(electrum__blockchain_scripthash_get_balance__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto request = R"({{"id":901,"method":"blockchain.scripthash.get_balance","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_scripthash));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scripthash_get_history__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scripthash.get_history","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_scripthash));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scripthash_get_mempool__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scripthash.get_mempool","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_scripthash));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scripthash_list_unspent__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scripthash.listunspent","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_scripthash));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_address_subscribe__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.address.subscribe","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_address));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scripthash_subscribe__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_1));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scripthash.subscribe","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_scripthash));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scripthash_unsubscribe__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_4_2));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scripthash.unsubscribe","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_scripthash));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_subscribe__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_7));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scriptpubkey.subscribe","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_script));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

// scriptpubkey

BC_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_balance__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_7));

    constexpr auto request = R"({{"id":901,"method":"blockchain.scriptpubkey.get_balance","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_script));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_history__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_7));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scriptpubkey.get_history","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_script));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_get_mempool__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_7));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scriptpubkey.get_mempool","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_script));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_CASE(electrum__blockchain_scriptpubkey_list_unspent__no_address_index__not_implemented)
{
    BC_REQUIRE(!query_.address_enabled());
    BC_REQUIRE(handshake(electrum::version::v1_7));

    constexpr auto request = R"({{"id":1001,"method":"blockchain.scriptpubkey.listunspent","params":["{}"]}})" "\n";
    const auto result = get_error(std::format(request, bogus_script));
    BC_REQUIRE_EQUAL(result, not_implemented.value());
}

BC_AUTO_TEST_SUITE_END()
