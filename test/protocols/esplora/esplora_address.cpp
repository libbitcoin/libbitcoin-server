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
#include "../../mocks/blocks.hpp"
#include "esplora_setup_fixture.hpp"

using namespace system;
namespace http = network::http;
namespace ws = network::ws;

BC_FIXTURE_TEST_SUITE(esplora_tests, esplora_ten_block_setup_fixture)

static const chain::transaction& coinbase1() NOEXCEPT
{
    return *test::block1.transactions_ptr()->front();
}

static std::string block1_tx() NOEXCEPT
{
    return encode_hash(coinbase1().hash(false));
}

static const std::string scripthash = encode_base16(null_hash);
static std::string funded_scripthash() NOEXCEPT
{
    return encode_base16(sha256_hash(coinbase1().outputs_ptr()->front()->script().to_data(false)));
}

// key resolution
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__address__undecodable__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/address/notanaddress"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__address_txs__undecodable__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/address/notanaddress/txs"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(esplora__address_utxo__undecodable__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/address/notanaddress/utxo"), http::status::bad_request);
}

// address/txs/mempool
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__address_txs_mempool__scripthash__empty)
{
    const auto response = get_json("/scripthash/" + scripthash + "/txs/mempool");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

// scripthash of the block one coinbase output
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__address__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash());
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BC_REQUIRE_EQUAL(object.at("scripthash").as_string(), funded_scripthash());

    const auto& stats = object.at("chain_stats").as_object();
    BC_REQUIRE_EQUAL(stats.at("funded_txo_count").as_int64(), 1);
    BC_REQUIRE_EQUAL(stats.at("funded_txo_sum").as_int64(), 5000000000);
    BC_REQUIRE_EQUAL(stats.at("spent_txo_count").as_int64(), 0);
    BC_REQUIRE_EQUAL(stats.at("spent_txo_sum").as_int64(), 0);
    BC_REQUIRE_EQUAL(stats.at("tx_count").as_int64(), 1);
}

BC_AUTO_TEST_CASE(esplora__address__unfunded__zeroed)
{
    const auto response = get_json("/scripthash/" + scripthash);
    BC_REQUIRE(response.is_object());

    const auto& stats = response.as_object().at("chain_stats").as_object();
    BC_REQUIRE_EQUAL(stats.at("funded_txo_count").as_int64(), 0);
    BC_REQUIRE_EQUAL(stats.at("tx_count").as_int64(), 0);
}

BC_AUTO_TEST_CASE(esplora__address_txs__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash() + "/txs");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(response.as_array().front().as_object().at("txid").as_string(), block1_tx());
}

BC_AUTO_TEST_CASE(esplora__address_utxo__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash() + "/utxo");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);

    const auto& utxo = response.as_array().front().as_object();
    BC_REQUIRE_EQUAL(utxo.at("txid").as_string(), block1_tx());
    BC_REQUIRE_EQUAL(utxo.at("vout").as_int64(), 0);
    BC_REQUIRE_EQUAL(utxo.at("value").as_int64(), 5000000000);
    BC_REQUIRE(utxo.at("status").as_object().at("confirmed").as_bool());
}

BC_AUTO_TEST_CASE(esplora__address_utxo__unfunded__empty)
{
    const auto response = get_json("/scripthash/" + scripthash + "/utxo");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

// address/txs/chain
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__address_txs_chain__funded__expected)
{
    const auto response = get_json("/scripthash/" + funded_scripthash() + "/txs/chain");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(response.as_array().front().as_object().at("txid").as_string(), block1_tx());
}

BC_AUTO_TEST_CASE(esplora__address_txs_chain__last_seen__empty)
{
    const auto response = get_json("/scripthash/" + funded_scripthash() + "/txs/chain/" + block1_tx());
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

BC_AUTO_TEST_CASE(esplora__address_txs_chain__unknown_last_seen__not_found)
{
    const auto target = "/scripthash/" + funded_scripthash() + "/txs/chain/" + encode_hash(null_hash);
    BC_REQUIRE_EQUAL(get_status(target), http::status::not_found);
}

BC_AUTO_TEST_CASE(esplora__address_txs_mempool__undecodable__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/address/notanaddress/txs/mempool"), http::status::bad_request);
}

// address (payment address)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(esplora__address__payment_address__echoed_unfunded)
{
    const std::string address{ "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa" };
    const auto response = get_json("/address/" + address);
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("address").as_string(), address);
    BC_REQUIRE_EQUAL(response.as_object().at("chain_stats").as_object().at("tx_count").as_int64(), 0);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(esplora_no_address_tests, esplora_no_address_setup_fixture)

BC_AUTO_TEST_CASE(esplora__address__disabled__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/scripthash/" + esplora_tests::funded_scripthash()), http::status::not_implemented);
}

BC_AUTO_TEST_CASE(esplora__address_txs__disabled__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/scripthash/" + esplora_tests::funded_scripthash() + "/txs"), http::status::not_implemented);
}

BC_AUTO_TEST_CASE(esplora__address_utxo__disabled__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/scripthash/" + esplora_tests::funded_scripthash() + "/utxo"), http::status::not_implemented);
}

BC_AUTO_TEST_SUITE_END()

// unconfirmed address
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(esplora_unconfirmed_address_tests, esplora_unconfirmed_setup_fixture)

static std::string pick_scripthash() NOEXCEPT
{
    return encode_base16(sha256_hash(test::tx4.outputs_ptr()->front()->script().to_data(false)));
}

BC_AUTO_TEST_CASE(esplora__address__unconfirmed__mempool_stats)
{
    const auto response = get_json("/scripthash/" + pick_scripthash());
    BC_REQUIRE(response.is_object());

    const auto& chain = response.as_object().at("chain_stats").as_object();
    BC_REQUIRE_EQUAL(chain.at("funded_txo_count").as_int64(), 0);
    BC_REQUIRE_EQUAL(chain.at("tx_count").as_int64(), 0);

    const auto& mempool = response.as_object().at("mempool_stats").as_object();
    BC_REQUIRE_EQUAL(mempool.at("funded_txo_count").as_int64(), 6);
    BC_REQUIRE_EQUAL(mempool.at("funded_txo_sum").as_int64(), 0x18 + 0x81 + 0x81 + 0x83 + 0x08 + 0x85);
    BC_REQUIRE_EQUAL(mempool.at("spent_txo_count").as_int64(), 1);
    BC_REQUIRE_EQUAL(mempool.at("spent_txo_sum").as_int64(), 0x18);
    BC_REQUIRE_EQUAL(mempool.at("tx_count").as_int64(), 6);
}

BC_AUTO_TEST_CASE(esplora__address_txs_mempool__unconfirmed__expected)
{
    const auto response = get_json("/scripthash/" + pick_scripthash() + "/txs/mempool");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 6u);
}

BC_AUTO_TEST_CASE(esplora__address_txs__unconfirmed__mempool_first)
{
    const auto response = get_json("/scripthash/" + pick_scripthash() + "/txs");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 6u);
    BC_REQUIRE(!response.as_array().front().as_object().at("status").as_object().at("confirmed").as_bool());
}

BC_AUTO_TEST_CASE(esplora__address_txs_chain__unconfirmed__empty)
{
    const auto response = get_json("/scripthash/" + pick_scripthash() + "/txs/chain");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE(response.as_array().empty());
}

BC_AUTO_TEST_CASE(esplora__address_utxo__unconfirmed__unspent)
{
    const auto response = get_json("/scripthash/" + pick_scripthash() + "/utxo");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 5u);

    const auto& utxos = response.as_array();
    const auto found = std::find_if(utxos.begin(), utxos.end(), [](const auto& utxo)
    {
        return utxo.as_object().at("txid").as_string() == encode_hash(test::tx4.hash(false));
    });

    BC_REQUIRE(found != utxos.end());
    BC_REQUIRE_EQUAL(found->as_object().at("value").as_int64(), 0x08);
    BC_REQUIRE(!found->as_object().at("status").as_object().at("confirmed").as_bool());
}

BC_AUTO_TEST_SUITE_END()

// broadcast address
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(esplora_broadcast_address_tests, esplora_broadcast_setup_fixture)

static std::string funder_scripthash() NOEXCEPT
{
    return encode_base16(sha256_hash(test::block1c.transactions_ptr()->front()->outputs_ptr()->front()->script().to_data(false)));
}

static std::string payee_scripthash() NOEXCEPT
{
    return encode_base16(sha256_hash(test::tx1c.outputs_ptr()->front()->script().to_data(false)));
}

static std::string tx1c() NOEXCEPT
{
    return encode_hash(test::tx1c.hash(false));
}

BC_AUTO_TEST_CASE(esplora__address__confirmed_spent_by_broadcast__mempool_spent)
{
    BC_REQUIRE_EQUAL(post_text("/tx", encode_base16(test::tx1c.to_data(true))), tx1c());

    const auto response = get_json("/scripthash/" + funder_scripthash());
    BC_REQUIRE(response.is_object());

    const auto& chain = response.as_object().at("chain_stats").as_object();
    BC_REQUIRE_EQUAL(chain.at("funded_txo_count").as_int64(), 1);
    BC_REQUIRE_EQUAL(chain.at("funded_txo_sum").as_int64(), 0x64);
    BC_REQUIRE_EQUAL(chain.at("spent_txo_count").as_int64(), 0);
    BC_REQUIRE_EQUAL(chain.at("tx_count").as_int64(), 1);

    const auto& mempool = response.as_object().at("mempool_stats").as_object();
    BC_REQUIRE_EQUAL(mempool.at("funded_txo_count").as_int64(), 0);
    BC_REQUIRE_EQUAL(mempool.at("spent_txo_count").as_int64(), 1);
    BC_REQUIRE_EQUAL(mempool.at("spent_txo_sum").as_int64(), 0x64);
    BC_REQUIRE_EQUAL(mempool.at("tx_count").as_int64(), 1);
}

BC_AUTO_TEST_CASE(esplora__address__broadcast_payee__mempool_funded)
{
    BC_REQUIRE_EQUAL(post_text("/tx", encode_base16(test::tx1c.to_data(true))), tx1c());

    const auto response = get_json("/scripthash/" + payee_scripthash());
    BC_REQUIRE(response.is_object());

    const auto& mempool = response.as_object().at("mempool_stats").as_object();
    BC_REQUIRE_EQUAL(mempool.at("funded_txo_count").as_int64(), 1);
    BC_REQUIRE_EQUAL(mempool.at("funded_txo_sum").as_int64(), 0x5a);
    BC_REQUIRE_EQUAL(mempool.at("spent_txo_count").as_int64(), 0);
    BC_REQUIRE_EQUAL(mempool.at("tx_count").as_int64(), 1);
}

BC_AUTO_TEST_CASE(esplora__address_txs_mempool__broadcast_payee__tx)
{
    BC_REQUIRE_EQUAL(post_text("/tx", encode_base16(test::tx1c.to_data(true))), tx1c());

    const auto response = get_json("/scripthash/" + payee_scripthash() + "/txs/mempool");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(response.as_array().front().as_object().at("txid").as_string(), tx1c());
}

BC_AUTO_TEST_CASE(esplora__address_utxo__broadcast__spent_excluded_payee_included)
{
    BC_REQUIRE_EQUAL(post_text("/tx", encode_base16(test::tx1c.to_data(true))), tx1c());

    const auto spent = get_json("/scripthash/" + funder_scripthash() + "/utxo");
    BC_REQUIRE(spent.is_array());
    BC_REQUIRE(spent.as_array().empty());

    const auto response = get_json("/scripthash/" + payee_scripthash() + "/utxo");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);

    const auto& utxo = response.as_array().front().as_object();
    BC_REQUIRE_EQUAL(utxo.at("txid").as_string(), tx1c());
    BC_REQUIRE_EQUAL(utxo.at("value").as_int64(), 0x5a);
    BC_REQUIRE(!utxo.at("status").as_object().at("confirmed").as_bool());
}

BC_AUTO_TEST_SUITE_END()
