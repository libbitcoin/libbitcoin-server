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
#include "bitcoind_setup_fixture.hpp"

using namespace system;

// Quote a hash as a single-element json-rpc params array.
static std::string hash_param(const hash_digest& hash) NOEXCEPT
{
    return "[\"" + encode_hash(hash) + "\"]";
}

// Quote a hash plus a trailing scalar (e.g. verbosity) as a params array.
static std::string hash_param(const hash_digest& hash,
    const std::string& tail) NOEXCEPT
{
    return "[\"" + encode_hash(hash) + "\", " + tail + "]";
}

// True if the json-rpc response carries a non-null error member.
static bool has_error(const boost::json::value& response) NOEXCEPT
{
    return response.is_object() && response.as_object().contains("error") &&
        !response.at("error").is_null();
}

static std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return std::string{ value.as_string().c_str() };
}

// The ten-block store contains mainnet blocks 0..9 (block9 is the tip).
BOOST_FIXTURE_TEST_SUITE(bitcoind_tests, bitcoind_ten_block_setup_fixture)

// blockchain
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockcount__ten_block_store__nine)
{
    const auto response = rpc("getblockcount");
    BOOST_REQUIRE(response.at("result").is_int64());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getbestblockhash__ten_block_store__block9)
{
    const auto response = rpc("getbestblockhash");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")),
        encode_hash(test::block9_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockhash__height_five__block5)
{
    const auto response = rpc("getblockhash", "[5]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")),
        encode_hash(test::block5_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockhash__genesis__block0)
{
    const auto response = rpc("getblockhash", "[0]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")),
        encode_hash(test::block0_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockheader__block9__no_transactions)
{
    const auto response = rpc("getblockheader", hash_param(test::block9_hash));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")),
        encode_hash(test::block9_hash));
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE(!result.as_object().contains("tx"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__block9_verbosity1__txid_list)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "1"));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")),
        encode_hash(test::block9_hash));
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE_EQUAL(result.at("tx").as_array().size(), 1u);
    BOOST_REQUIRE(result.at("tx").at(0).is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__block9_verbosity2__tx_objects)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "2"));
    const auto& tx = response.at("result").at("tx");
    BOOST_REQUIRE(tx.is_array());
    BOOST_REQUIRE(tx.at(0).is_object());
    BOOST_REQUIRE(tx.at(0).as_object().contains("txid"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockchaininfo__ten_block_store__expected)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    // The mock store populates the confirmed chain only; headers (candidate
    // top) reflects that, so assert presence/type rather than a height.
    BOOST_REQUIRE(result.at("headers").is_int64());
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblockhash")),
        encode_hash(test::block9_hash));

    // Fields added alongside these tests.
    BOOST_REQUIRE(result.as_object().contains("target"));
    BOOST_REQUIRE(result.at("warnings").is_string());
    BOOST_REQUIRE(!result.at("initialblockdownload").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxout__unspent_coinbase__output)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto response = rpc("gettxout", hash_param(txid, "0"));
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE(result.as_object().contains("value"));
    BOOST_REQUIRE(result.as_object().contains("scriptPubKey"));
}

// rawtransactions
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__coinbase_raw__hex)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "0"));
    BOOST_REQUIRE(response.at("result").is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__coinbase_verbose__context)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "1"));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("txid")), encode_hash(txid));
    BOOST_REQUIRE(result.at("vin").is_array());
    BOOST_REQUIRE(result.at("vout").is_array());

    // Chain context injected at the protocol layer.
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")),
        encode_hash(test::block1_hash));
    BOOST_REQUIRE_EQUAL(result.at("confirmations").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__unknown_txid__error)
{
    const auto response = rpc("getrawtransaction", hash_param(null_hash, "1"));
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__sendrawtransaction__invalid_hex__error)
{
    const auto response = rpc("sendrawtransaction", "[\"nothex\"]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__sendrawtransaction__malformed__error)
{
    const auto response = rpc("sendrawtransaction", "[\"00\"]");
    BOOST_REQUIRE(has_error(response));
}

// network
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnetworkinfo__fields)
{
    const auto response = rpc("getnetworkinfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.as_object().contains("version"));
    BOOST_REQUIRE(result.at("subversion").is_string());
    BOOST_REQUIRE(result.as_object().contains("protocolversion"));
    BOOST_REQUIRE(result.at("networks").is_array());
}

// declared but deliberately not implemented (structured not_implemented error)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__not_implemented__error)
{
    const std::vector<std::pair<std::string, std::string>> methods
    {
        { "getblockstats", "[0]" },
        { "getchaintxstats", "[]" },
        { "getchainwork", "[]" },
        { "gettxoutsetinfo", "[]" },
        { "scantxoutset", "[\"start\", []]" },
        { "verifychain", "[]" },
        { "verifytxoutset", "[\"test\"]" },
        { "pruneblockchain", "[1]" },
        { "savemempool", "[]" }
    };

    for (const auto& [method, params]: methods)
        BOOST_REQUIRE_MESSAGE(has_error(rpc(method, params)), method);
}

// getblockfilter is implemented but requires bip158 (disabled in this store).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockfilter__filters_disabled__error)
{
    const auto response = rpc("getblockfilter",
        hash_param(test::block9_hash, "\"basic\""));
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_SUITE_END()

// segwit (witness store: genesis + two blocks carrying witness transactions)
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(bitcoind_witness_tests, bitcoind_witness_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__witness_tx__wtxid_differs)
{
    // block1a's first transaction carries witnesses; wtxid differs from txid.
    const auto& tx = *test::block1a.transactions_ptr()->front();
    const auto txid = tx.hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "1"));
    const auto& result = response.at("result");

    BOOST_REQUIRE_EQUAL(as_text(result.at("txid")), encode_hash(txid));
    BOOST_REQUIRE_NE(as_text(result.at("hash")), encode_hash(txid));

    // Segwit weight accounting: vsize == ceil(weight / 4).
    const auto weight = result.at("weight").as_int64();
    BOOST_REQUIRE_EQUAL(result.at("vsize").as_int64(), (weight + 3) / 4);
}

BOOST_AUTO_TEST_SUITE_END()
