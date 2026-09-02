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
#include "bitcoind_setup_fixture.hpp"

using namespace system;

namespace {

const auto block0 = encode_hash(test::block0_hash);
const auto block1 = encode_hash(test::block1_hash);
const auto block5 = encode_hash(test::block5_hash);
const auto block9 = encode_hash(test::block9_hash);

std::string hash_param(const hash_digest& hash) NOEXCEPT
{
    return "[\"" + encode_hash(hash) + "\"]";
}

std::string hash_param(const hash_digest& hash,
    const std::string& tail) NOEXCEPT
{
    return "[\"" + encode_hash(hash) + "\", " + tail + "]";
}

bool has_error(const boost::json::value& response) NOEXCEPT
{
    return response.is_object() && response.as_object().contains("error") &&
        !response.at("error").is_null();
}

// Codes are the bitcoind wire values, not our enumeration.
bool has_code(const boost::json::value& response, int64_t code) NOEXCEPT
{
    return has_error(response) &&
        response.at("error").at("code").as_int64() == code;
}

using method_code = std::pair<std::string, int64_t>;

const std::vector<method_code> rejected_methods
{
    { "dumptxoutset", -32601 },
    { "loadtxoutset", -32601 },
    { "clearbanned", -20 },
    { "listbanned", -20 },
    { "setban", -20 },
    { "stop", -32601 },
    { "descriptorprocesspsbt", -32601 },
    { "signrawtransactionwithkey", -32601 },
    { "signmessagewithprivkey", -32601 }
};

const std::vector<method_code> wip_methods
{
    { "getblockfrompeer", -32601 },
    { "disconnectnode", -32601 },
    { "exportasmap", -32601 },
    { "getaddednodeinfo", -24 }
};

std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

const std::vector<std::string> scope_methods
{
    "addconnection",
    "addpeeraddress",
    "echo",
    "echoipc",
    "echojson",
    "estimaterawfee",
    "generate",
    "generateblock",
    "generatetoaddress",
    "generatetodescriptor",
    "getmempoolfeeratediagram",
    "getorphantxs",
    "getrawaddrman",
    "invalidateblock",
    "mockscheduler",
    "reconsiderblock",
    "sendmsgtopeer",
    "setmocktime",
    "syncwithvalidationinterfacequeue",
    "abandontransaction",
    "abortrescan",
    "addhdkey",
    "backupwallet",
    "bumpfee",
    "createwallet",
    "createwalletdescriptor",
    "encryptwallet",
    "exportwatchonlywallet",
    "getaddressesbylabel",
    "getaddressinfo",
    "getbalance",
    "getbalances",
    "gethdkeys",
    "getnewaddress",
    "getrawchangeaddress",
    "getreceivedbyaddress",
    "getreceivedbylabel",
    "gettransaction",
    "getwalletinfo",
    "importdescriptors",
    "importprunedfunds",
    "keypoolrefill",
    "listaddressgroupings",
    "listdescriptors",
    "listlabels",
    "listlockunspent",
    "listreceivedbyaddress",
    "listreceivedbylabel",
    "listsinceblock",
    "listtransactions",
    "listunspent",
    "listwalletdir",
    "listwallets",
    "loadwallet",
    "lockunspent",
    "migratewallet",
    "psbtbumpfee",
    "removeprunedfunds",
    "rescanblockchain",
    "restorewallet",
    "send",
    "sendall",
    "sendmany",
    "sendtoaddress",
    "setlabel",
    "setwalletflag",
    "signmessage",
    "signrawtransactionwithwallet",
    "simulaterawtransaction",
    "unloadwallet",
    "walletcreatefundedpsbt",
    "walletdisplayaddress",
    "walletlock",
    "walletpassphrase",
    "walletpassphrasechange",
    "walletprocesspsbt",
    "enumeratesigners"
};

const std::vector<method_code> pending_methods
{
    { "getmempoolancestors", -33 },
    { "getmempoolcluster", -33 },
    { "getmempooldescendants", -33 },
    { "getmempoolentry", -33 },
    { "getmempoolinfo", -33 },
    { "getrawmempool", -33 },
    { "gettxspendingprevout", -33 },
    { "importmempool", -33 },
    { "abortprivatebroadcast", -32601 },
    { "getprivatebroadcastinfo", -32601 },
    { "submitpackage", -33 },
    { "getblocktemplate", -33 },
    { "getprioritisedtransactions", -33 },
    { "prioritisetransaction", -33 },
    { "estimatesmartfee", -32603 }
};

} // namespace

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
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockhash__height_five__block5)
{
    const auto response = rpc("getblockhash", "[5]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockhash__genesis__block0)
{
    const auto response = rpc("getblockhash", "[0]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), block0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockheader__block9__no_transactions)
{
    const auto response = rpc("getblockheader", hash_param(test::block9_hash));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE(!result.as_object().contains("tx"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__block9_verbosity1__txid_list)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "1"));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
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

// Coinbase-only blocks carry no prevout context (no fee, no prevouts).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__block9_verbosity3__tx_objects)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "3"));
    const auto& tx = response.at("result").at("tx");
    BOOST_REQUIRE(tx.is_array());
    BOOST_REQUIRE(tx.at(0).as_object().contains("txid"));
    BOOST_REQUIRE(!tx.at(0).as_object().contains("fee"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__spend_verbosity2__fee)
{
    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::mock_block11, database::context{ 0, 11, 0 }, false, false));

    const auto response = rpc("getblock", hash_param(test::mock_block11.hash(), "2"));
    const auto& tx = response.at("result").at("tx");
    BOOST_REQUIRE(tx.at(0).as_object().contains("fee"));
    BOOST_REQUIRE(!tx.at(0).at("vin").at(0).as_object().contains("prevout"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__spend_verbose__prevout)
{
    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.set(test::mock_block11, database::context{ 0, 11, 0 }, false, false));

    const auto txid = test::mock_block11.transactions_ptr()->front()->hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "2"));
    const auto& vin = response.at("result").at("vin").at(0);
    REQUIRE_NO_THROW_TRUE(vin.at("prevout").at("generated").as_bool());
    BOOST_REQUIRE_EQUAL(vin.at("prevout").at("height").as_int64(), 3);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__verbosity4__clamped_tx_objects)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "4"));
    REQUIRE_NO_THROW_TRUE(response.at("result").at("tx").at(0).is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__negative_verbosity__clamped_hex)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "-1"));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockheader__genesis__self_inclusive_mediantime)
{
    const auto response = rpc("getblockheader", hash_param(test::genesis.hash(), "true"));
    BOOST_REQUIRE_EQUAL(response.at("result").at("mediantime").as_int64(), 1231006505);
}

// The top block has no stored child context, so its mtp is promoted.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockheader__top__promoted_mediantime)
{
    const auto response = rpc("getblockheader", hash_param(test::block9_hash, "true"));
    BOOST_REQUIRE_EQUAL(response.at("result").at("mediantime").as_int64(), 1231471428);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockchaininfo__ten_block_store__expected)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE(result.at("headers").is_int64());
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblockhash")), block9);
    BOOST_REQUIRE(result.as_object().contains("target"));
    BOOST_REQUIRE(result.at("warnings").is_array());
    BOOST_REQUIRE(result.at("initialblockdownload").is_bool());
    BOOST_REQUIRE(result.at("chainwork").is_string());
    BOOST_REQUIRE(result.at("size_on_disk").as_int64() > 0);
}

// Ten blocks at minimum difficulty: cumulative work is 10 * 0x0100010001.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockchaininfo__ten_block_store__chainwork)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chainwork")), "0000000000000000000000000000000000000000000000000000000a000a000a");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblock__block9__chainwork)
{
    const auto response = rpc("getblock", hash_param(test::block9_hash, "1"));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chainwork")), "0000000000000000000000000000000000000000000000000000000a000a000a");
}

// bip9_softforks is a btcd-endpoint field (see btcd_rpc tests).
// Removed from bitcoind's getblockchaininfo in 0.19 and absent from the fork.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockchaininfo__softforks__absent)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE(!result.as_object().contains("bip9_softforks"));
    BOOST_REQUIRE(!result.as_object().contains("softforks"));
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

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxout__archived_unconfirmed__null)
{
    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));

    const auto txid = test::mock_block10.transactions_ptr()->at(1)->hash(false);
    const auto response = rpc("gettxout", hash_param(txid, "0"));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
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
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")), block1);
    BOOST_REQUIRE_EQUAL(result.at("confirmations").as_int64(), 9);
}

// A coinbase has no prevouts, so fee and prevout context are omitted.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__coinbase_verbosity_two__no_fee)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "2"));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("txid")), encode_hash(txid));
    BOOST_REQUIRE(!result.as_object().contains("fee"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__excess_verbosity__clamped_verbose)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "3"));
    REQUIRE_NO_THROW_TRUE(response.at("result").at("vin").is_array());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__negative_verbosity__clamped_hex)
{
    const auto txid = test::block1.transactions_ptr()->front()->hash(false);
    const auto response = rpc("getrawtransaction", hash_param(txid, "-1"));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
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

BOOST_AUTO_TEST_CASE(bitcoind_rpc__sendrawtransaction__confirmed_unspent__verify_already_in_utxo_set)
{
    const auto tx0 = encode_base16(test::genesis.transactions_ptr()->front()->to_data(true));
    const auto response = rpc("sendrawtransaction", "[\"" + tx0 + "\"]");
    BOOST_REQUIRE_MESSAGE(has_code(response, -27), response);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__sendrawtransaction__unknown_inputs__verify_error)
{
    const chain::input input{ chain::point{ one_hash, 0 }, {}, 0xffffffff };
    const chain::output output{ 1, chain::script{ chain::script::to_pay_key_hash_pattern({ 0x42 }) } };
    const chain::transaction missing{ 1, { input }, { output }, 0 };
    const auto hex = encode_base16(missing.to_data(true));
    const auto response = rpc("sendrawtransaction", "[\"" + hex + "\"]");
    BOOST_REQUIRE_MESSAGE(has_code(response, -25), response);
}

// control, mining, rawtransactions, util (moved from btcd)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__help__default__implemented_method_list)
{
    const auto response = rpc("help");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_NE(as_text(response.at("result")).find("getblockcount"), std::string::npos);
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")).find("pruneblockchain"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnetworkhashps__default__number)
{
    const auto response = rpc("getnetworkhashps");
    BOOST_REQUIRE(response.at("result").is_double() || response.at("result").is_int64());
}

// Work over the window divided by its timestamp span (early 2009 blocks).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnetworkhashps__nine_block_window__positive)
{
    const auto response = rpc("getnetworkhashps", "[9, 9]");
    BOOST_REQUIRE(response.at("result").as_double() > 0.0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnetworkhashps__genesis_height__zero_window)
{
    const auto response = rpc("getnetworkhashps", "[120, 0]");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 0);
}

// currentblockweight/currentblocktx omitted (no block ever assembled).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getmininginfo__ten_block_store__expected)
{
    const auto response = rpc("getmininginfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("difficulty").as_double(), 1.0);
    BOOST_REQUIRE_EQUAL(result.at("pooledtx").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE(!result.as_object().contains("currentblockweight"));
    BOOST_REQUIRE_EQUAL(result.at("next").at("height").as_int64(), 10);
    BOOST_REQUIRE_EQUAL(result.at("next").at("difficulty").as_double(), 1.0);
    BOOST_REQUIRE(result.at("warnings").as_array().empty());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__one_in_one_out__hex)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto response = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__amount__exact_satoshis)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 83052.07783498}]");
    const auto decoded = rpc("decoderawtransaction", "[\"" + std::string{ as_text(created.at("result")) } + "\"]");
    BOOST_REQUIRE_EQUAL(decoded.at("result").at("vout").at(0).at("value").as_double(), 83052.07783498);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__excess_amount__error)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto response = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 21000001}]");
    BOOST_REQUIRE_MESSAGE(has_code(response, -3), response);
}

// combinerawtransaction

BOOST_AUTO_TEST_CASE(bitcoind_rpc__combinerawtransaction__no_transactions__invalid_parameter)
{
    // 'txs' is one positional arg that is itself an array: [[...]].
    const auto response = rpc("combinerawtransaction", "[[]]");
    BOOST_REQUIRE(has_code(response, -8));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__combinerawtransaction__invalid_hex__deserialization)
{
    const auto response = rpc("combinerawtransaction", R"([["not-hex"]])");
    BOOST_REQUIRE(has_code(response, -22));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__combinerawtransaction__unknown_input__verify_error)
{
    using namespace chain;
    const transaction tx
    {
        1,
        inputs{ input{ point{ one_hash, 0 }, script{}, witness{}, 0xffffffff } },
        outputs{ output{ 1, script{ script::to_pay_key_hash_pattern(short_hash{}) } } },
        0
    };

    const auto hex = encode_base16(tx.to_data(true));
    const auto response = rpc("combinerawtransaction", R"([[")" + hex + R"("]])");
    BOOST_REQUIRE(has_code(response, -25));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__combinerawtransaction__key_hash_variants__endorsing_candidate)
{
    using namespace chain;
    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    // The found_address p2kh output of mock_block10's second transaction.
    const point prevout{ test::mock_block10.transactions_ptr()->at(1)->hash(false), 0 };
    const output out{ 1, script{ script::to_pay_key_hash_pattern(short_hash{}) } };
    const script endorsing{ operations{ { data_chunk(71, 0x30), false }, { data_chunk(33, 0x02), false } } };

    const transaction unsigned_tx{ 1, inputs{ input{ prevout, script{}, witness{}, 0xffffffff } }, outputs{ out }, 0 };
    const transaction signed_tx{ 1, inputs{ input{ prevout, endorsing, witness{}, 0xffffffff } }, outputs{ out }, 0 };

    // The unsigned variant is the base, the endorsing candidate is taken.
    const auto params = R"([[")" + encode_base16(unsigned_tx.to_data(true)) + R"(",")" + encode_base16(signed_tx.to_data(true)) + R"("]])";
    const auto response = rpc("combinerawtransaction", params);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), encode_base16(signed_tx.to_data(true)));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__combinerawtransaction__multisig_partials__merged_in_key_order)
{
    using namespace chain;
    const ec_secret secret1{ { 0x01 } };
    const ec_secret secret2{ { 0x02 } };
    const ec_secret secret3{ { 0x03 } };
    ec_compressed point1{};
    ec_compressed point2{};
    ec_compressed point3{};
    BOOST_REQUIRE(secret_to_public(point1, secret1));
    BOOST_REQUIRE(secret_to_public(point2, secret2));
    BOOST_REQUIRE(secret_to_public(point3, secret3));

    // A block paying 2-of-3 multisig of the derived keys, confirmed at 10.
    constexpr uint64_t value = 100'000'000;
    const script multisig{ script::to_pay_multisig_pattern(2, ec_compresseds{ point1, point2, point3 }) };
    const block block10
    {
        header{ 0x31323334, test::block9_hash, hash_digest{ 0x10, 0xcc }, 0x41424344, 0x51525354, 0x61626364 },
        transactions{ transaction{ 1, inputs{ input{ point{}, script{}, witness{}, 0x01 } }, outputs{ output{ value, multisig } }, 0 } }
    };

    BOOST_REQUIRE(query_.set(block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(block10.hash()), true));

    // A spend of the multisig output, endorsed separately by two keys.
    const point prevout{ block10.transactions_ptr()->front()->hash(false), 0 };
    const output out{ 1, script{ script::to_pay_key_hash_pattern(short_hash{}) } };
    const transaction spend{ 1, inputs{ input{ prevout, script{}, witness{}, 0xffffffff } }, outputs{ out }, 0 };

    endorsement endorse1{};
    endorsement endorse2{};
    BOOST_REQUIRE(spend.create_endorsement(endorse1, secret1, multisig, 0, value, coverage::hash_all, script_version::unversioned, flags::no_rules));
    BOOST_REQUIRE(spend.create_endorsement(endorse2, secret2, multisig, 0, value, coverage::hash_all, script_version::unversioned, flags::no_rules));

    const script partial1{ operations{ { opcode::push_size_0 }, { data_chunk{ endorse1 }, false } } };
    const script partial2{ operations{ { opcode::push_size_0 }, { data_chunk{ endorse2 }, false } } };
    const transaction variant1{ 1, inputs{ input{ prevout, partial1, witness{}, 0xffffffff } }, outputs{ out }, 0 };
    const transaction variant2{ 1, inputs{ input{ prevout, partial2, witness{}, 0xffffffff } }, outputs{ out }, 0 };

    // Passed in reverse to show ordering is by key position, not variant.
    const auto params = R"([[")" + encode_base16(variant2.to_data(true)) + R"(",")" + encode_base16(variant1.to_data(true)) + R"("]])";
    const auto response = rpc("combinerawtransaction", params);
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    data_chunk data{};
    BOOST_REQUIRE(decode_base16(data, as_text(response.at("result"))));

    const transaction merged{ data, true };
    BOOST_REQUIRE(merged.is_valid());

    const auto& ops = merged.inputs_ptr()->front()->script().ops();
    BOOST_REQUIRE_EQUAL(ops.size(), 3u);
    BOOST_REQUIRE(ops.at(1).data() == endorse1);
    BOOST_REQUIRE(ops.at(2).data() == endorse2);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decoderawtransaction__iswitness_false__round_trips)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\", false]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(response.at("result").at("vin").as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decoderawtransaction__created__round_trips)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(response.at("result").at("locktime").as_int64(), 0);
}

// The input sequence encodes locktime enforceability and replaceability: a
// final sequence disables locktime and near-final does not signal bip125.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__default__replaceable_sequence)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("version").as_int64(), 2);
    BOOST_REQUIRE_EQUAL(response.at("result").at("vin").at(0).at("sequence").as_int64(), 4294967293);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__not_replaceable__final_sequence)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}, 0, false]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("vin").at(0).at("sequence").as_int64(), 4294967295);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__locktime_not_replaceable__near_final_sequence)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}, 500, false]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("locktime").as_int64(), 500);
    BOOST_REQUIRE_EQUAL(response.at("result").at("vin").at(0).at("sequence").as_int64(), 4294967294);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__explicit_sequence__overrides)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0,\"sequence\":42}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("vin").at(0).at("sequence").as_int64(), 42);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__array_outputs__repeated_address)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], [{\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}, {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.002}]]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("vout").as_array().size(), 2u);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__version_three__decodes)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}, 0, true, 3]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("version").as_int64(), 3);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__replaceable__bip125_sequence)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}, 0, true]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").at("vin").at(0).at("sequence").as_int64(), 4294967293);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decodescript__p2kh__pubkeyhash)
{
    const auto response = rpc("decodescript", "[\"76a914000000000000000000000000000000000000000088ac\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(as_text(response.at("result").at("type")), "pubkeyhash");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decodescript__p2kh__descriptor_and_segwit)
{
    const auto response = rpc("decodescript", "[\"76a914000000000000000000000000000000000000000088ac\"]");
    const auto& result = response.at("result");
    const auto address = as_text(result.at("address"));
    BOOST_REQUIRE_EQUAL(as_text(result.at("desc")), "addr(" + address + ")#" + descriptor_checksum("addr(" + address + ")"));
    const auto& segwit = result.at("segwit");
    BOOST_REQUIRE_EQUAL(as_text(segwit.at("type")), "witness_v0_scripthash");
    BOOST_REQUIRE(as_text(segwit.at("address")).starts_with("bc1q"));
    BOOST_REQUIRE(segwit.as_object().contains("p2sh-segwit"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decodescript__undecodable__nonstandard)
{
    const auto response = rpc("decodescript", "[\"01\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("type").as_string(), "nonstandard");
    BOOST_REQUIRE_EQUAL(result.at("desc").as_string().subview(0, 8), "raw(01)#");
    BOOST_REQUIRE(!result.as_object().contains("p2sh"));
    BOOST_REQUIRE(!result.as_object().contains("segwit"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decodescript__witness_program__no_segwit)
{
    const auto response = rpc("decodescript", "[\"0014751e76e8199196d454941c45d1b3a323f1433bd6\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("type")), "witness_v0_keyhash");
    BOOST_REQUIRE(!result.as_object().contains("segwit"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__genesis__valid)
{
    const auto response = rpc("validateaddress", "[\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").at("isvalid").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__genesis__expected_script)
{
    const auto response = rpc("validateaddress", "[\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\"]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result").at("scriptPubKey")), "76a91462e907b15cbf27d5425399ebf6f0fb50ebb88f1888ac");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__foreign_network__invalid)
{
    const wallet::payment_address testnet{ short_hash{}, 111 };
    const auto response = rpc("validateaddress", "[\"" + testnet.encoded() + "\"]");
    BOOST_REQUIRE(!response.at("result").at("isvalid").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__foreign_network__invalid_address)
{
    const wallet::payment_address testnet{ short_hash{}, 111 };
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto response = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"" + testnet.encoded() + "\": 0.001}]");
    BOOST_REQUIRE_MESSAGE(has_code(response, -5), response);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__witness__expected_script)
{
    const auto response = rpc("validateaddress", "[\"bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").at("iswitness").as_bool());
    BOOST_REQUIRE_EQUAL(as_text(response.at("result").at("scriptPubKey")), "0014751e76e8199196d454941c45d1b3a323f1433bd6");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__garbage__invalid)
{
    const auto response = rpc("validateaddress", "[\"notanaddress\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE(!response.at("result").at("isvalid").as_bool());
}

// The redeem script is deterministic: 2 <key1> <key2> 2 checkmultisig.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__createmultisig__two_of_two__redeem_script_and_p2sh)
{
    const std::string key1 = "03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd";
    const std::string key2 = "03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626";
    const auto response = rpc("createmultisig", "[2, [\"" + key1 + "\", \"" + key2 + "\"]]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("redeemScript")), "5221" + key1 + "21" + key2 + "52ae");
    BOOST_REQUIRE(as_text(result.at("descriptor")).starts_with("sh(multi(2," + key1));
    BOOST_REQUIRE(!result.as_object().contains("warnings"));

    const auto validated = rpc("validateaddress", "[\"" + as_text(result.at("address")) + "\"]");
    BOOST_REQUIRE(validated.at("result").at("isvalid").as_bool());
    BOOST_REQUIRE(validated.at("result").at("isscript").as_bool());
    BOOST_REQUIRE(!validated.at("result").at("iswitness").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createmultisig__bech32__witness_address)
{
    const std::string key1 = "03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd";
    const std::string key2 = "03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626";
    const auto response = rpc("createmultisig", "[2, [\"" + key1 + "\", \"" + key2 + "\"], \"bech32\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(as_text(result.at("descriptor")).starts_with("wsh(multi(2,"));

    const auto validated = rpc("validateaddress", "[\"" + as_text(result.at("address")) + "\"]");
    BOOST_REQUIRE(validated.at("result").at("isvalid").as_bool());
    BOOST_REQUIRE(validated.at("result").at("iswitness").as_bool());
}

// An uncompressed key downgrades the segwit address type to legacy (warning).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__createmultisig__uncompressed_bech32__legacy_with_warning)
{
    const std::string key = "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f";
    const auto response = rpc("createmultisig", "[1, [\"" + key + "\"], \"bech32\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(as_text(result.at("descriptor")).starts_with("sh(multi(1,"));
    BOOST_REQUIRE_EQUAL(result.at("warnings").as_array().size(), 1u);

    const auto validated = rpc("validateaddress", "[\"" + as_text(result.at("address")) + "\"]");
    BOOST_REQUIRE(validated.at("result").at("isvalid").as_bool());
    BOOST_REQUIRE(!validated.at("result").at("iswitness").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createmultisig__excess_required__error)
{
    const std::string key = "03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd";
    const auto response = rpc("createmultisig", "[2, [\"" + key + "\"]]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createmultisig__excess_keys__invalid_parameter)
{
    const auto response = rpc("createmultisig", "[1, [\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\",\"00\"]]");
    BOOST_REQUIRE_MESSAGE(has_code(response, -8), response);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__rpc_discover__default__openrpc_version)
{
    const auto response = rpc("rpc.discover");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result").at("openrpc")), "1.2.6");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__testmempoolaccept__unsigned__not_allowed_with_reason)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("testmempoolaccept", "[[\"" + as_text(created.at("result")) + "\"]]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BOOST_REQUIRE(!response.at("result").at(0).at("allowed").as_bool());
    BOOST_REQUIRE(response.at("result").at(0).as_object().contains("reject-reason"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__testmempoolaccept__empty__error)
{
    const auto response = rpc("testmempoolaccept", "[[]]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__testmempoolaccept__coinbase__coinbase_token)
{
    const auto tx0 = encode_base16(test::genesis.transactions_ptr()->front()->to_data(true));
    const auto response = rpc("testmempoolaccept", "[[\"" + tx0 + "\"]]");
    BOOST_REQUIRE_EQUAL(response.at("result").at(0).at("reject-reason").as_string(), "coinbase");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__testmempoolaccept__unknown_inputs__missingorspent_token)
{
    const chain::input input{ chain::point{ one_hash, 0 }, {}, 0xffffffff };
    const chain::output output{ 1, chain::script{ chain::script::to_pay_key_hash_pattern({ 0x42 }) } };
    const chain::transaction missing{ 1, { input }, { output }, 0 };
    const auto hex = encode_base16(missing.to_data(true));
    const auto response = rpc("testmempoolaccept", "[[\"" + hex + "\"]]");
    BOOST_REQUIRE_EQUAL(response.at("result").at(0).at("reject-reason").as_string(), "bad-txns-inputs-missingorspent");
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
    BOOST_REQUIRE_EQUAL(as_text(result.at("networks").at(0).at("name")), "ipv4");
    BOOST_REQUIRE_EQUAL(as_text(result.at("localservices")).size(), 16u);
    BOOST_REQUIRE(result.at("localservicesnames").is_array());
    BOOST_REQUIRE(result.at("connections_in").is_int64());
    BOOST_REQUIRE(result.at("connections_out").is_int64());
}

// not implemented
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__not_implemented__error)
{
    const std::vector<std::pair<std::string, std::string>> methods
    {
        { "pruneblockchain", "[1]" },
        { "savemempool", "[]" }
    };

    for (const auto& [method, params]: methods)
        BOOST_REQUIRE_MESSAGE(has_error(rpc(method, params)), method);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__rejected__expected_code)
{
    for (const auto& [method, code]: rejected_methods)
    {
        BOOST_REQUIRE_MESSAGE(has_code(rpc(method, "[]"), code), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__wip__expected_code)
{
    for (const auto& [method, code]: wip_methods)
    {
        BOOST_REQUIRE_MESSAGE(has_code(rpc(method, "[]"), code), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__scope__method_not_found)
{
    for (const auto& method: scope_methods)
    {
        BOOST_REQUIRE_MESSAGE(has_code(rpc(method, "[]"), -32601), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__pending__expected_code)
{
    for (const auto& [method, code]: pending_methods)
    {
        BOOST_REQUIRE_MESSAGE(has_code(rpc(method, "[]"), code), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__verifychain__noop__true)
{
    const auto response = rpc("verifychain", "[]");
    REQUIRE_NO_THROW_TRUE(response.at("result").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdifficulty__ten_block_store__one)
{
    const auto response = rpc("getdifficulty");
    BOOST_REQUIRE_EQUAL(response.at("result").as_double(), 1.0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__verifymessage__compressed_vector__true)
{
    const wallet::payment_address address(base16_array(
        "002688cc350a5333a87fa622eacec626c3d1c0ebf9f3793de3885fa254d7e393"));
    const auto signature = encode_base64(base16_chunk(
        "20c0ae26619db18abd1e8a84d005bafd336512eda7207cf7f4f6c36c9614ed6bc"
        "f531a954929ddc0a86578f4d28a26e19b676c890a49881d6f25e393befd6d1682"));
    const auto params = "[\"" + address.encoded() + "\", \"" + signature +
        "\", \"Compressed\"]";
    const auto response = rpc("verifymessage", params);
    BOOST_REQUIRE_EQUAL(response.at("result").as_bool(), true);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__verifymessage__invalid_address__error)
{
    const auto response = rpc("verifymessage", "[\"notanaddress\", \"x\", \"m\"]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__verifymessage__malformed_signature__error)
{
    const wallet::payment_address address(base16_array(
        "002688cc350a5333a87fa622eacec626c3d1c0ebf9f3793de3885fa254d7e393"));
    const auto params = "[\"" + address.encoded() + "\", \"@@@\", \"m\"]";
    const auto response = rpc("verifymessage", params);
    BOOST_REQUIRE(has_error(response));
}

// The historical test store is not current, so synced is false.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getindexinfo__ten_block_store__txindex_not_synced)
{
    const auto response = rpc("getindexinfo");
    const auto& txindex = response.at("result").at("txindex");
    BOOST_REQUIRE(!txindex.at("synced").as_bool());
    BOOST_REQUIRE_EQUAL(txindex.at("best_block_height").as_int64(), 9);
}

// No currency window, coalesced (confirmed top is candidate top), so synced.
BOOST_FIXTURE_TEST_CASE(bitcoind_rpc__getindexinfo__current_coalesced__txindex_synced,
    bitcoind_current_setup_fixture)
{
    const auto response = rpc("getindexinfo");
    const auto& txindex = response.at("result").at("txindex");
    BOOST_REQUIRE(txindex.at("synced").as_bool());
    BOOST_REQUIRE_EQUAL(txindex.at("best_block_height").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockstats__block1_by_height__coinbase_only)
{
    const auto response = rpc("getblockstats", "[1]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")), block1);
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("txs").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("ins").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("outs").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("totalfee").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("subsidy").as_int64(), 5000000000);
    BOOST_REQUIRE_EQUAL(result.at("utxo_increase").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("feerate_percentiles").as_array().size(), 5u);
}

// The stats selection returns only the named subset.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockstats__by_hash_selected__subset)
{
    const auto response = rpc("getblockstats", "[\"" + block1 + "\", [\"height\", \"subsidy\"]]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.as_object().size(), 2u);
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(result.at("subsidy").as_int64(), 5000000000);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockstats__unknown_stat__error)
{
    const auto response = rpc("getblockstats", "[1, [\"nonsense\"]]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintips__ten_block_store__active)
{
    const auto response = rpc("getchaintips");
    const auto& active = response.at("result").as_array().at(0);
    BOOST_REQUIRE_EQUAL(active.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(active.at("status")), "active");
}

// A single fully-validated chainstate (assumeutxo is rejected).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchainstates__ten_block_store__single_validated)
{
    const auto response = rpc("getchainstates");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.at("headers").is_int64());

    const auto& states = result.at("chainstates").as_array();
    BOOST_REQUIRE_EQUAL(states.size(), 1u);
    BOOST_REQUIRE_EQUAL(states.at(0).at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(states.at(0).at("bestblockhash")), block9);
    BOOST_REQUIRE_EQUAL(states.at(0).at("verificationprogress").as_double(), 1.0);
    BOOST_REQUIRE(states.at(0).at("validated").as_bool());
}

// Empty until the zeromq service is introduced and configured (as bitcoind
// with no publishers configured).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getzmqnotifications__no_publishers__empty)
{
    const auto response = rpc("getzmqnotifications");
    BOOST_REQUIRE(response.at("result").is_array());
    BOOST_REQUIRE(response.at("result").as_array().empty());
}

// Ten block store, so the default window is bounded to height - 1.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintxstats__default__window_to_top)
{
    const auto response = rpc("getchaintxstats");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("window_final_block_height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("window_final_block_hash")), block9);
    BOOST_REQUIRE_EQUAL(result.at("window_block_count").as_int64(), 8);
    BOOST_REQUIRE_EQUAL(result.at("window_tx_count").as_int64(), 8);
}

// Ten blocks of one tx each, genesis included.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintxstats__default__txcount_ten)
{
    const auto response = rpc("getchaintxstats");
    BOOST_REQUIRE_EQUAL(response.at("result").at("txcount").as_int64(), 10);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintxstats__zero_window__no_interval)
{
    const auto response = rpc("getchaintxstats", "[0]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("window_block_count").as_int64(), 0);
    BOOST_REQUIRE(!result.as_object().contains("window_interval"));
    BOOST_REQUIRE(!result.as_object().contains("txrate"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintxstats__window_five__five_txs)
{
    const auto response = rpc("getchaintxstats", "[5]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("window_block_count").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("window_tx_count").as_int64(), 5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintxstats__window_exceeds_height__error)
{
    const auto response = rpc("getchaintxstats", "[10]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintxstats__block_hash__that_block)
{
    const auto response = rpc("getchaintxstats", "[2, \"" + block5 + "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("window_final_block_height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("window_tx_count").as_int64(), 2);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__uptime__running__seconds)
{
    const auto response = rpc("uptime");
    BOOST_REQUIRE(response.at("result").is_int64());
    BOOST_REQUIRE(response.at("result").as_int64() >= 0);
}

// The test fixture makes no peer connections.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getconnectioncount__no_peers__zero)
{
    const auto response = rpc("getconnectioncount");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 0);
}

// Byte counters are untracked, and no upload target is configured.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnettotals__untracked_counters__zero)
{
    const auto response = rpc("getnettotals");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("totalbytesrecv").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("totalbytessent").as_int64(), 0);
    BOOST_REQUIRE(result.at("timemillis").as_int64() > 0);
    BOOST_REQUIRE_EQUAL(result.at("uploadtarget").at("target").as_int64(), 0);
    BOOST_REQUIRE(!result.at("uploadtarget").at("target_reached").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrpcinfo__default__logpath_and_no_active)
{
    const auto response = rpc("getrpcinfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.at("active_commands").as_array().empty());
    BOOST_REQUIRE(!as_text(result.at("logpath")).empty());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getmemoryinfo__default__zero_locked)
{
    const auto response = rpc("getmemoryinfo");
    const auto& locked = response.at("result").at("locked");
    BOOST_REQUIRE_EQUAL(locked.at("total").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(locked.at("locked").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(locked.at("chunks_used").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getmemoryinfo__mallocinfo__error)
{
    const auto response = rpc("getmemoryinfo", "[\"mallocinfo\"]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__logging__default__levels)
{
    const auto response = rpc("logging");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.as_object().contains("application"));
    BOOST_REQUIRE(result.as_object().contains("verbose"));
    BOOST_REQUIRE(result.at("fault").is_bool());
}

// Levels are compiled in or out, so they cannot be changed at run time.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__logging__include__error)
{
    const auto response = rpc("logging", "[[\"news\"]]");
    BOOST_REQUIRE(has_error(response));
}

// A proof produced by gettxoutproof verifies to the proven txid (round trip
// over the merkle block wire form).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__txoutproof__round_trip__proven_txid)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto proof = rpc("gettxoutproof", "[[\"" + txid + "\"], \"" + block1 + "\"]");
    REQUIRE_NO_THROW_TRUE(proof.at("result").is_string());

    const auto verified = rpc("verifytxoutproof", "[\"" + as_text(proof.at("result")) + "\"]");
    const auto& result = verified.at("result");
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0)), txid);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutproof__empty_txids__error)
{
    const auto response = rpc("gettxoutproof", "[[]]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutproof__unknown_txid__error)
{
    const auto response = rpc("gettxoutproof", "[[\"" + encode_hash(system::one_hash) + "\"]]");
    BOOST_REQUIRE(has_error(response));
}

// A structurally valid proof for a block not in the store proves nothing.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__verifytxoutproof__garbage__error)
{
    const auto response = rpc("verifytxoutproof", "[\"00\"]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdeploymentinfo__ten_block_store__top_frozen)
{
    const auto response = rpc("getdeploymentinfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("deployments").at("bip34").at("type")), "buried");
    BOOST_REQUIRE_EQUAL(result.at("deployments").at("bip34").at("height").as_int64(), 227931);
    BOOST_REQUIRE_EQUAL(result.at("deployments").at("bip66").at("height").as_int64(), 363725);
    BOOST_REQUIRE_EQUAL(result.at("deployments").at("bip65").at("height").as_int64(), 388381);
    BOOST_REQUIRE_EQUAL(result.at("deployments").at("csv").at("height").as_int64(), 419328);
    BOOST_REQUIRE_EQUAL(result.at("deployments").at("segwit").at("height").as_int64(), 481824);
}

// bitcoind buried taproot and no longer reports it as a deployment.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdeploymentinfo__taproot__absent)
{
    const auto response = rpc("getdeploymentinfo");
    BOOST_REQUIRE(!response.at("result").at("deployments").as_object().contains("taproot"));
}

// Mainnet activations are all above the ten block store, so none are active.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdeploymentinfo__below_activation__inactive)
{
    const auto response = rpc("getdeploymentinfo");
    const auto& deployments = response.at("result").at("deployments");
    BOOST_REQUIRE(!deployments.at("bip34").at("active").as_bool());
    BOOST_REQUIRE(!deployments.at("segwit").at("active").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdeploymentinfo__block_hash__that_block)
{
    const auto response = rpc("getdeploymentinfo", hash_param(test::block5_hash));
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdeploymentinfo__unknown_hash__error)
{
    const auto response = rpc("getdeploymentinfo", hash_param(system::one_hash));
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockfilter__filters_disabled__error)
{
    const auto response = rpc("getblockfilter", hash_param(test::block9_hash, "\"basic\""));
    BOOST_REQUIRE(has_error(response));
}

// batch
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__batch__two_requests__two_ordered_responses)
{
    const auto response = rpc_body(
        R"([{"jsonrpc":"2.0","id":1,"method":"getblockcount","params":[]},)"
        R"({"jsonrpc":"2.0","id":2,"method":"getbestblockhash","params":[]}])");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 2u);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("result").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(batch.at(1).at("id").as_int64(), 2);
    BOOST_REQUIRE_EQUAL(as_text(batch.at(1).at("result")), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__batch__single_element__array_of_one)
{
    const auto response = rpc_body(
        R"([{"jsonrpc":"2.0","id":7,"method":"getblockcount","params":[]}])");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 1u);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 7);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__batch__error_element__delivered_in_order)
{
    const auto unknown = hash_param(null_hash, "1");
    const auto response = rpc_body((boost_format(
        R"([{"jsonrpc":"2.0","id":1,"method":"getblockcount","params":[]},)"
        R"({"jsonrpc":"2.0","id":2,"method":"getrawtransaction","params":%1%},)"
        R"({"jsonrpc":"2.0","id":3,"method":"getblockcount","params":[]}])") %
            unknown).str());

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 3u);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("result").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(batch.at(1).at("id").as_int64(), 2);
    BOOST_REQUIRE(has_error(batch.at(1)));
    BOOST_REQUIRE_EQUAL(batch.at(2).at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__batch__v1_second_element__closed_partial_batch)
{
    // The batched v1 policy stop closes the open batch response (the first
    // element was delivered and responded).
    const auto response = rpc_body(
        R"([{"jsonrpc":"2.0","id":1,"method":"getblockcount","params":[]},)"
        R"({"id":2,"method":"getblockcount","params":[]}])");

    BOOST_REQUIRE(response.is_array());
    const auto& batch = response.as_array();
    BOOST_REQUIRE_EQUAL(batch.size(), 1u);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("id").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(batch.at(0).at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__batch__v1_element__dropped)
{
    // Batch participation of a v1 message (no jsonrpc field) drops (http).
    const auto response = rpc_body(
        R"([{"id":1,"method":"getblockcount","params":[]}])");

    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__batch__empty__dropped)
{
    const auto response = rpc_body("[]");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

// notifications
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__notification__v2_missing_id__no_content)
{
    const auto result = rpc_body_status(R"({"jsonrpc":"2.0","method":"getblockcount","params":[]})");
    BOOST_REQUIRE(result == bitcoind_setup_fixture::status::no_content);
}

// bitcoind answers a v1 null id notification (non-compliant, reproduced).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__notification__v1_null_id__answered)
{
    const auto response = rpc_body(R"({"id":null,"method":"getblockcount","params":[]})");
    BOOST_REQUIRE(response.at("id").is_null());
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__notification__ws_v2_missing_id__no_response)
{
    BOOST_REQUIRE(!ws_upgrade());

    ws_notify(R"({"jsonrpc":"2.0","method":"getblockcount","params":[]})");
    const auto response = ws_rpc("getblockcount");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

// websocket
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockcount__websocket__nine)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_rpc("getblockcount");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getbestblockhash__websocket__block9)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_rpc("getbestblockhash");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockhash__websocket__block5)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_rpc("getblockhash", "[5]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockchaininfo__websocket__expected)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_rpc("getblockchaininfo");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__unknown_method__websocket__error_keeps_connection)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto unknown = ws_rpc("nosuchmethod");
    REQUIRE_NO_THROW_TRUE(unknown.at("error").is_object());

    const auto response = ws_rpc("getblockcount");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__response__websocket__id_matches_request)
{
    BOOST_REQUIRE(!ws_upgrade());

    const auto response = ws_rpc("getblockcount");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 0);
}


// descriptor activity

// Block one's coinbase output is watched via its raw script descriptor.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdescriptoractivity__block1_coinbase__one_receive)
{
    const auto hash = encode_hash(test::block1.hash());
    const auto script = encode_base16(test::block1.transactions_ptr()->front()->outputs_ptr()->front()->script().to_data(false));
    const auto response = rpc("getdescriptoractivity", "[[\"" + hash + "\"], [\"raw(" + script + ")\"]]");
    const auto& activity = response.at("result").at("activity");
    BOOST_REQUIRE_EQUAL(activity.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(as_text(activity.at(0).at("type")), "receive");
    BOOST_REQUIRE_EQUAL(activity.at(0).at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(activity.at(0).at("amount").as_double(), 50.0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdescriptoractivity__unknown_block__not_found)
{
    const auto response = rpc("getdescriptoractivity", "[[\"0000000000000000000000000000000000000000000000000000000000000001\"], []]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

// scanblocks

// The fixture runs with block filters disabled (as getblockfilter).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__scanblocks__filters_disabled__error)
{
    const auto response = rpc("scanblocks", "[\"start\", [\"pk(04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f)\"]]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__scanblocks__bad_action__invalid)
{
    const auto response = rpc("scanblocks", "[\"status\", []]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

// gettxoutsetinfo

// The genesis output is excluded from the utxo set (as bitcoind).
// TODO: pin the hash_serialized_3/muhash digests against bitcoind vectors.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__default__expected)
{
    const auto response = rpc("gettxoutsetinfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblock")), block9);
    BOOST_REQUIRE_EQUAL(result.at("transactions").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("txouts").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("bogosize").as_int64(), 9 * (50 + 67));
    BOOST_REQUIRE_EQUAL(result.at("disk_size").as_int64(), 9 * (48 + 1 + 67));
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 450.0);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash_serialized_3")).size(), 64u);
    BOOST_REQUIRE(!result.as_object().contains("muhash"));
}

// Ten coinbase-only blocks issue 500 and retain 450, the genesis coinbase
// being excluded from the set (as bitcoind).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__default__genesis_unspendable)
{
    const auto response = rpc("gettxoutsetinfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 450.0);
    BOOST_REQUIRE_EQUAL(result.at("total_unspendable_amount").as_double(), 50.0);

    const auto& info = result.at("block_info");
    BOOST_REQUIRE_EQUAL(info.at("coinbase").as_double(), 50.0);
    BOOST_REQUIRE_EQUAL(info.at("prevout_spent").as_double(), 0.0);
    BOOST_REQUIRE_EQUAL(info.at("new_outputs_ex_coinbase").as_double(), 0.0);
    BOOST_REQUIRE_EQUAL(info.at("unspendable").as_double(), 0.0);
    BOOST_REQUIRE_EQUAL(info.at("unspendables").at("scripts").as_double(), 0.0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__muhash__expected)
{
    const auto response = rpc("gettxoutsetinfo", "[\"muhash\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE_EQUAL(result.at("txouts").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 450.0);
    BOOST_REQUIRE_EQUAL(as_text(result.at("muhash")).size(), 64u);
    BOOST_REQUIRE(!result.as_object().contains("hash_serialized_3"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__none__expected)
{
    const auto response = rpc("gettxoutsetinfo", "[\"none\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE_EQUAL(result.at("transactions").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("txouts").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("disk_size").as_int64(), 9 * (48 + 1 + 67));
    BOOST_REQUIRE(!result.as_object().contains("hash_serialized_3"));
    BOOST_REQUIRE(!result.as_object().contains("muhash"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__bad_hash_type__invalid)
{
    const auto response = rpc("gettxoutsetinfo", "[\"sha256\"]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__height__expected)
{
    const auto response = rpc("gettxoutsetinfo", "[\"none\", 5]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblock")), block5);
    BOOST_REQUIRE_EQUAL(result.at("transactions").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("txouts").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 250.0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__hash__expected)
{
    const auto params = "[\"muhash\", \"" + block5 + "\"]";
    const auto response = rpc("gettxoutsetinfo", params);
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblock")), block5);
    BOOST_REQUIRE_EQUAL(as_text(result.at("muhash")).size(), 64u);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__serialized_at_height__expected)
{
    const auto response = rpc("gettxoutsetinfo", "[\"hash_serialized_3\", 5]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash_serialized_3")).size(), 64u);
}

// bitcoind rejects the use_index contradiction.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__gettxoutsetinfo__no_index_at_height__invalid)
{
    const auto response = rpc("gettxoutsetinfo", "[\"muhash\", 5, false]");
    BOOST_REQUIRE(has_error(response));
}

// preciousblock

// Only a cached (tied) branch is prioritizable, an organized one is not.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__preciousblock__archived__invalid_address)
{
    const auto response = rpc("preciousblock", hash_param(test::block5_hash));
    BOOST_REQUIRE(has_code(response, -5));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__preciousblock__unknown__invalid_address)
{
    const std::string unknown(64, '1');
    const auto response = rpc("preciousblock", "[\"" + unknown + "\"]");
    BOOST_REQUIRE(has_code(response, -5));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__preciousblock__not_hash__invalid_parameter)
{
    const auto response = rpc("preciousblock", "[\"nothex\"]");
    BOOST_REQUIRE(has_code(response, -8));
}

// scantxoutset

BOOST_AUTO_TEST_CASE(bitcoind_rpc__scantxoutset__status__null)
{
    const auto response = rpc("scantxoutset", "[\"status\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__scantxoutset__abort__false)
{
    const auto response = rpc("scantxoutset", "[\"abort\"]");
    REQUIRE_NO_THROW_FALSE(response.at("result").as_bool());
}

// bitcoind scans the full set when given nothing to match (success, empty).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__scantxoutset__empty_scanobjects__empty)
{
    const auto response = rpc("scantxoutset", "[\"start\", []]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE(result.at("success").as_bool());
    BOOST_REQUIRE(result.at("unspents").as_array().empty());
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 0.0);
}

// The genesis output is excluded from the utxo set (as bitcoind).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__scantxoutset__genesis_pk__excluded)
{
    const auto response = rpc("scantxoutset", "[\"start\", [\"pk(04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f)\"]]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE(result.at("success").as_bool());
    BOOST_REQUIRE(result.at("unspents").as_array().empty());
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 0.0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__scantxoutset__block1_script__expected)
{
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto txid = encode_hash(coinbase.hash(false));
    const auto script = coinbase.outputs_ptr()->front()->script().to_data(false);
    const auto params = "[\"start\", [\"raw(" + encode_base16(script) + ")\"]]";

    const auto response = rpc("scantxoutset", params);
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE(result.at("success").as_bool());
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblock")), block9);
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 50.0);

    const auto& unspents = result.at("unspents").as_array();
    BOOST_REQUIRE_EQUAL(unspents.size(), 1u);
    const auto& unspent = unspents.front();
    BOOST_REQUIRE_EQUAL(as_text(unspent.at("txid")), txid);
    BOOST_REQUIRE_EQUAL(unspent.at("vout").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(as_text(unspent.at("scriptPubKey")),
        encode_base16(script));
    BOOST_REQUIRE_EQUAL(as_text(unspent.at("desc")).find("pk(04"), 0u);
    BOOST_REQUIRE_EQUAL(unspent.at("amount").as_double(), 50.0);
    BOOST_REQUIRE(unspent.at("coinbase").as_bool());
    BOOST_REQUIRE_EQUAL(unspent.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(as_text(unspent.at("blockhash")), block1);
    BOOST_REQUIRE_EQUAL(unspent.at("confirmations").as_int64(), 9);
}

// bitcoind's maximum descriptor range (ParseDescriptorRange).
BOOST_AUTO_TEST_CASE(bitcoind_rpc__scantxoutset__range_too_large__invalid)
{
    const auto response = rpc("scantxoutset", "[\"start\", [{\"desc\": "
        "\"pk(04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61de"
        "b649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f)\","
        " \"range\": [0, 1000000]}]]");
    BOOST_REQUIRE(has_error(response));
}

// The scan path serves without the address index (recovery-grade).
BOOST_FIXTURE_TEST_CASE(bitcoind_rpc__scantxoutset__no_address_index__scans,
    bitcoind_no_address_setup_fixture)
{
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto txid = encode_hash(coinbase.hash(false));
    const auto script = coinbase.outputs_ptr()->front()->script().to_data(false);
    const auto params = "[\"start\", [\"raw(" + encode_base16(script) + ")\"]]";

    const auto response = rpc("scantxoutset", params);
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.is_object());
    BOOST_REQUIRE(result.at("success").as_bool());

    // The scanned coin count is the full set (blocks 1-9 coinbases).
    BOOST_REQUIRE_EQUAL(result.at("txouts").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("total_amount").as_double(), 50.0);

    const auto& unspents = result.at("unspents").as_array();
    BOOST_REQUIRE_EQUAL(unspents.size(), 1u);
    const auto& unspent = unspents.front();
    BOOST_REQUIRE_EQUAL(as_text(unspent.at("txid")), txid);
    BOOST_REQUIRE_EQUAL(unspent.at("height").as_int64(), 1);
    BOOST_REQUIRE_EQUAL(unspent.at("confirmations").as_int64(), 9);
}

// openrpc

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getopenrpcinfo__always__document)
{
    const auto response = rpc("getopenrpcinfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("openrpc")), "1.2.6");
    BOOST_REQUIRE(result.at("methods").is_array());
    BOOST_REQUIRE(!result.at("methods").as_array().empty());
}

// help

BOOST_AUTO_TEST_CASE(bitcoind_rpc__help__getblock__usage_line)
{
    const auto response = rpc("help", "[\"getblock\"]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), "getblock blockhash ( verbosity )");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__help__unknown__reported)
{
    const auto response = rpc("help", "[\"nonsense\"]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), "help: unknown command: nonsense");
}

// descriptors

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getdescriptorinfo__wpkh__expected)
{
    const auto response = rpc("getdescriptorinfo", "[\"wpkh([d34db33f/84h/0h/0h]xpub6DJ2dNUysrn5Vt36jH2KLBT2i1auw1tTSSomg8PhqNiUtx8QX2SvC9nrHu81fT41fvDUnhMjEzQgXnQjKEu3oaqMSzhSrHMxyyoEAmUHQbY/0/*)\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("checksum")), "cjjspncu");
    BOOST_REQUIRE(result.at("isrange").as_bool());
    BOOST_REQUIRE(result.at("issolvable").as_bool());
    BOOST_REQUIRE(!result.at("hasprivatekeys").as_bool());
}

// The derived address round-trips to the bip386 vector script.
BOOST_AUTO_TEST_CASE(bitcoind_rpc__deriveaddresses__tr_bip386__expected)
{
    const auto response = rpc("deriveaddresses", "[\"tr(a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 1u);
    const auto address = as_text(result.at(0));
    BOOST_REQUIRE(address.starts_with("bc1p"));
    const auto validated = rpc("validateaddress", "[\"" + address + "\"]");
    BOOST_REQUIRE_EQUAL(as_text(validated.at("result").at("scriptPubKey")), "512077aab6e066f8a7419c5ab714c12c67d25007ed55a43cadcacb4d7a970a093f11");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__deriveaddresses__ranged_without_range__invalid)
{
    const auto response = rpc("deriveaddresses", "[\"pkh(xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw/1/*)\"]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__deriveaddresses__range_pair__two_addresses)
{
    const auto response = rpc("deriveaddresses", "[\"pkh(xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw/1/*)\", [3, 4]]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 2u);
    BOOST_REQUIRE(as_text(result.at(0)).starts_with("1"));
}

// network group

BOOST_AUTO_TEST_CASE(bitcoind_rpc__addnode__remove__not_implemented)
{
    const auto response = rpc("addnode", "[\"127.0.0.1:8333\", \"remove\"]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__addnode__bad_command__invalid)
{
    const auto response = rpc("addnode", "[\"127.0.0.1:8333\", \"nonsense\"]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__ping__always__null)
{
    const auto response = rpc("ping");
    BOOST_REQUIRE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnodeaddresses__empty_pool__empty)
{
    const auto response = rpc("getnodeaddresses");
    BOOST_REQUIRE(response.at("result").is_array());
    BOOST_REQUIRE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnodeaddresses__bad_network__invalid)
{
    const auto response = rpc("getnodeaddresses", "[0, \"onion\"]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getaddrmaninfo__empty_pool__zero_buckets)
{
    const auto response = rpc("getaddrmaninfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("all_networks").at("total").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("ipv4").at("tried").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__setnetworkactive__toggle__round_trips)
{
    const auto off = rpc("setnetworkactive", "[false]");
    BOOST_REQUIRE(!off.at("result").as_bool());
    const auto on = rpc("setnetworkactive", "[true]");
    BOOST_REQUIRE(on.at("result").as_bool());
}

// submit

BOOST_AUTO_TEST_CASE(bitcoind_rpc__submitblock__existing_block__duplicate)
{
    const auto block = encode_base16(test::block9.to_data(true));
    const auto response = rpc("submitblock", "[\"" + block + "\"]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), "duplicate");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__submitblock__unknown_header__prev_blk_not_found_token)
{
    auto data = test::block1.to_data(true);
    data[76]++;
    const auto response = rpc("submitblock", "[\"" + encode_base16(data) + "\"]");
    BOOST_REQUIRE_EQUAL(response.at("result").as_string(), "prev-blk-not-found");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__submitblock__garbage__invalid)
{
    const auto response = rpc("submitblock", "[\"deadbeef\"]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__submitheader__existing_header__null)
{
    const auto header = encode_base16(test::block9.header().to_data());
    const auto response = rpc("submitheader", "[\"" + header + "\"]");
    BOOST_REQUIRE(response.at("result").is_null());
}

// waitfor (all conditions immediately met or timing out on the fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rpc__waitforblockheight__at_top__immediate_top)
{
    const auto response = rpc("waitforblockheight", "[9]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__waitforblock__confirmed_hash__immediate_top)
{
    const auto response = rpc("waitforblock", "[\"" + std::string{ block9 } + "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__waitfornewblock__short_timeout__times_out_with_top)
{
    const auto response = rpc("waitfornewblock", "[1]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
}

// psbt (vectors from bip174)

#define PSBT_UPDATER "cHNidP8BAJoCAAAAAljoeiG1ba8MI76OcHBFbDNvfLqlyHV5JPVFiHuyq911AAAAAAD/////g40EJ9DsZQpoqka7CwmK6kQiwHGyyng1Kgd5WdB86h0BAAAAAP////8CcKrwCAAAAAAWABTYXCtx0AYLCcmIauuBXlCZHdoSTQDh9QUAAAAAFgAUAK6pouXw+HaliN9VRuh0LR2HAI8AAAAAAAEAuwIAAAABqtc5MQGL0l+ErkALaISL4J23BurCrBgpi6vucatlb4sAAAAASEcwRAIgWPb8fGoz4bMVSNSByCbAFb0wE1qtQs1neQ2rZtKtJDsCIEoc7SYExnNbY5PltBaR3XiwDwxZQvufdRhW+qk4FX26Af7///8CgPD6AgAAAAAXqRQPuUY0IWlrgsgzryQceMF9295JNIfQ8gonAQAAABepFCnKdPigj4GZlCgYXJe12FLkBj9hh2UAAAABBEdSIQKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgfyEC2rYf9JoU22p9ArDNH7t4/EsYMStbTlTa5Nui+/71NtdSriIGApWDvzmuCmCXR60Zmt3WNPphCFWdbFzTm0whg/GrluB/ENkMak8AAACAAAAAgAAAAIAiBgLath/0mhTban0CsM0fu3j8SxgxK1tOVNrk26L7/vU21xDZDGpPAAAAgAAAAIABAACAAAEBIADC6wsAAAAAF6kUt/X69A49QKWkWbHbNTXyty+pIeiHAQQiACCMI1MXN0O1ld+0oHtyuo5C43l9p06H/n2ddJfjsgKJAwEFR1IhAwidwQx6xttU+RMpr2FzM9s4jOrQwjH3IzedG5kDCwLcIQI63ZBPPW3PWd25BrDe4jUpt/+57VDl6GFRkmhgIh8Oc1KuIgYCOt2QTz1tz1nduQaw3uI1Kbf/ue1Q5ehhUZJoYCIfDnMQ2QxqTwAAAIAAAACAAwAAgCIGAwidwQx6xttU+RMpr2FzM9s4jOrQwjH3IzedG5kDCwLcENkMak8AAACAAAAAgAIAAIAAIgIDqaTDf1mW06ol26xrVwrwZQOUSSlCRgs1R1Ptnuylh3EQ2QxqTwAAAIAAAACABAAAgAAiAgJ/Y5l1fS7/VaE2rQLGhLGDi2VW5fG2s0KCqUtrUAUQlhDZDGpPAAAAgAAAAIAFAACAAA=="
#define PSBT_SIGNER_A "cHNidP8BAJoCAAAAAljoeiG1ba8MI76OcHBFbDNvfLqlyHV5JPVFiHuyq911AAAAAAD/////g40EJ9DsZQpoqka7CwmK6kQiwHGyyng1Kgd5WdB86h0BAAAAAP////8CcKrwCAAAAAAWABTYXCtx0AYLCcmIauuBXlCZHdoSTQDh9QUAAAAAFgAUAK6pouXw+HaliN9VRuh0LR2HAI8AAAAAAAEAuwIAAAABqtc5MQGL0l+ErkALaISL4J23BurCrBgpi6vucatlb4sAAAAASEcwRAIgWPb8fGoz4bMVSNSByCbAFb0wE1qtQs1neQ2rZtKtJDsCIEoc7SYExnNbY5PltBaR3XiwDwxZQvufdRhW+qk4FX26Af7///8CgPD6AgAAAAAXqRQPuUY0IWlrgsgzryQceMF9295JNIfQ8gonAQAAABepFCnKdPigj4GZlCgYXJe12FLkBj9hh2UAAAAiAgLath/0mhTban0CsM0fu3j8SxgxK1tOVNrk26L7/vU210gwRQIhAPYQOLMI3B2oZaNIUnRvAVdyk0IIxtJEVDk82ZvfIhd3AiAFbmdaZ1ptCgK4WxTl4pB02KJam1dgvqKBb2YZEKAG6gEBAwQBAAAAAQRHUiEClYO/Oa4KYJdHrRma3dY0+mEIVZ1sXNObTCGD8auW4H8hAtq2H/SaFNtqfQKwzR+7ePxLGDErW05U2uTbovv+9TbXUq4iBgKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgfxDZDGpPAAAAgAAAAIAAAACAIgYC2rYf9JoU22p9ArDNH7t4/EsYMStbTlTa5Nui+/71NtcQ2QxqTwAAAIAAAACAAQAAgAABASAAwusLAAAAABepFLf1+vQOPUClpFmx2zU18rcvqSHohyICAjrdkE89bc9Z3bkGsN7iNSm3/7ntUOXoYVGSaGAiHw5zRzBEAiBl9FulmYtZon/+GnvtAWrx8fkNVLOqj3RQql9WolEDvQIgf3JHA60e25ZoCyhLVtT/y4j3+3Weq74IqjDym4UTg9IBAQMEAQAAAAEEIgAgjCNTFzdDtZXftKB7crqOQuN5fadOh/59nXSX47ICiQMBBUdSIQMIncEMesbbVPkTKa9hczPbOIzq0MIx9yM3nRuZAwsC3CECOt2QTz1tz1nduQaw3uI1Kbf/ue1Q5ehhUZJoYCIfDnNSriIGAjrdkE89bc9Z3bkGsN7iNSm3/7ntUOXoYVGSaGAiHw5zENkMak8AAACAAAAAgAMAAIAiBgMIncEMesbbVPkTKa9hczPbOIzq0MIx9yM3nRuZAwsC3BDZDGpPAAAAgAAAAIACAACAACICA6mkw39ZltOqJdusa1cK8GUDlEkpQkYLNUdT7Z7spYdxENkMak8AAACAAAAAgAQAAIAAIgICf2OZdX0u/1WhNq0CxoSxg4tlVuXxtrNCgqlLa1AFEJYQ2QxqTwAAAIAAAACABQAAgAA="
#define PSBT_SIGNER_B "cHNidP8BAJoCAAAAAljoeiG1ba8MI76OcHBFbDNvfLqlyHV5JPVFiHuyq911AAAAAAD/////g40EJ9DsZQpoqka7CwmK6kQiwHGyyng1Kgd5WdB86h0BAAAAAP////8CcKrwCAAAAAAWABTYXCtx0AYLCcmIauuBXlCZHdoSTQDh9QUAAAAAFgAUAK6pouXw+HaliN9VRuh0LR2HAI8AAAAAAAEAuwIAAAABqtc5MQGL0l+ErkALaISL4J23BurCrBgpi6vucatlb4sAAAAASEcwRAIgWPb8fGoz4bMVSNSByCbAFb0wE1qtQs1neQ2rZtKtJDsCIEoc7SYExnNbY5PltBaR3XiwDwxZQvufdRhW+qk4FX26Af7///8CgPD6AgAAAAAXqRQPuUY0IWlrgsgzryQceMF9295JNIfQ8gonAQAAABepFCnKdPigj4GZlCgYXJe12FLkBj9hh2UAAAAiAgKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgf0cwRAIgdAGK1BgAl7hzMjwAFXILNoTMgSOJEEjn282bVa1nnJkCIHPTabdA4+tT3O+jOCPIBwUUylWn3ZVE8VfBZ5EyYRGMASICAtq2H/SaFNtqfQKwzR+7ePxLGDErW05U2uTbovv+9TbXSDBFAiEA9hA4swjcHahlo0hSdG8BV3KTQgjG0kRUOTzZm98iF3cCIAVuZ1pnWm0KArhbFOXikHTYolqbV2C+ooFvZhkQoAbqAQEDBAEAAAABBEdSIQKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgfyEC2rYf9JoU22p9ArDNH7t4/EsYMStbTlTa5Nui+/71NtdSriIGApWDvzmuCmCXR60Zmt3WNPphCFWdbFzTm0whg/GrluB/ENkMak8AAACAAAAAgAAAAIAiBgLath/0mhTban0CsM0fu3j8SxgxK1tOVNrk26L7/vU21xDZDGpPAAAAgAAAAIABAACAAAEBIADC6wsAAAAAF6kUt/X69A49QKWkWbHbNTXyty+pIeiHIgIDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtxHMEQCIGLrelVhB6fHP0WsSrWh3d9vcHX7EnWWmn84Pv/3hLyyAiAMBdu3Rw2/LwhVfdNWxzJcHtMJE+mWzThAlF2xIijaXwEiAgI63ZBPPW3PWd25BrDe4jUpt/+57VDl6GFRkmhgIh8Oc0cwRAIgZfRbpZmLWaJ//hp77QFq8fH5DVSzqo90UKpfVqJRA70CIH9yRwOtHtuWaAsoS1bU/8uI9/t1nqu+CKow8puFE4PSAQEDBAEAAAABBCIAIIwjUxc3Q7WV37Sge3K6jkLjeX2nTof+fZ10l+OyAokDAQVHUiEDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtwhAjrdkE89bc9Z3bkGsN7iNSm3/7ntUOXoYVGSaGAiHw5zUq4iBgI63ZBPPW3PWd25BrDe4jUpt/+57VDl6GFRkmhgIh8OcxDZDGpPAAAAgAAAAIADAACAIgYDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtwQ2QxqTwAAAIAAAACAAgAAgAAiAgOppMN/WZbTqiXbrGtXCvBlA5RJKUJGCzVHU+2e7KWHcRDZDGpPAAAAgAAAAIAEAACAACICAn9jmXV9Lv9VoTatAsaEsYOLZVbl8bazQoKpS2tQBRCWENkMak8AAACAAAAAgAUAAIAA"
#define PSBT_COMBINED "cHNidP8BAJoCAAAAAljoeiG1ba8MI76OcHBFbDNvfLqlyHV5JPVFiHuyq911AAAAAAD/////g40EJ9DsZQpoqka7CwmK6kQiwHGyyng1Kgd5WdB86h0BAAAAAP////8CcKrwCAAAAAAWABTYXCtx0AYLCcmIauuBXlCZHdoSTQDh9QUAAAAAFgAUAK6pouXw+HaliN9VRuh0LR2HAI8AAAAAAAEAuwIAAAABqtc5MQGL0l+ErkALaISL4J23BurCrBgpi6vucatlb4sAAAAASEcwRAIgWPb8fGoz4bMVSNSByCbAFb0wE1qtQs1neQ2rZtKtJDsCIEoc7SYExnNbY5PltBaR3XiwDwxZQvufdRhW+qk4FX26Af7///8CgPD6AgAAAAAXqRQPuUY0IWlrgsgzryQceMF9295JNIfQ8gonAQAAABepFCnKdPigj4GZlCgYXJe12FLkBj9hh2UAAAAiAgKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgf0cwRAIgdAGK1BgAl7hzMjwAFXILNoTMgSOJEEjn282bVa1nnJkCIHPTabdA4+tT3O+jOCPIBwUUylWn3ZVE8VfBZ5EyYRGMASICAtq2H/SaFNtqfQKwzR+7ePxLGDErW05U2uTbovv+9TbXSDBFAiEA9hA4swjcHahlo0hSdG8BV3KTQgjG0kRUOTzZm98iF3cCIAVuZ1pnWm0KArhbFOXikHTYolqbV2C+ooFvZhkQoAbqAQEDBAEAAAABBEdSIQKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgfyEC2rYf9JoU22p9ArDNH7t4/EsYMStbTlTa5Nui+/71NtdSriIGApWDvzmuCmCXR60Zmt3WNPphCFWdbFzTm0whg/GrluB/ENkMak8AAACAAAAAgAAAAIAiBgLath/0mhTban0CsM0fu3j8SxgxK1tOVNrk26L7/vU21xDZDGpPAAAAgAAAAIABAACAAAEBIADC6wsAAAAAF6kUt/X69A49QKWkWbHbNTXyty+pIeiHIgIDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtxHMEQCIGLrelVhB6fHP0WsSrWh3d9vcHX7EnWWmn84Pv/3hLyyAiAMBdu3Rw2/LwhVfdNWxzJcHtMJE+mWzThAlF2xIijaXwEiAgI63ZBPPW3PWd25BrDe4jUpt/+57VDl6GFRkmhgIh8Oc0cwRAIgZfRbpZmLWaJ//hp77QFq8fH5DVSzqo90UKpfVqJRA70CIH9yRwOtHtuWaAsoS1bU/8uI9/t1nqu+CKow8puFE4PSAQEDBAEAAAABBCIAIIwjUxc3Q7WV37Sge3K6jkLjeX2nTof+fZ10l+OyAokDAQVHUiEDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtwhAjrdkE89bc9Z3bkGsN7iNSm3/7ntUOXoYVGSaGAiHw5zUq4iBgI63ZBPPW3PWd25BrDe4jUpt/+57VDl6GFRkmhgIh8OcxDZDGpPAAAAgAAAAIADAACAIgYDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtwQ2QxqTwAAAIAAAACAAgAAgAAiAgOppMN/WZbTqiXbrGtXCvBlA5RJKUJGCzVHU+2e7KWHcRDZDGpPAAAAgAAAAIAEAACAACICAn9jmXV9Lv9VoTatAsaEsYOLZVbl8bazQoKpS2tQBRCWENkMak8AAACAAAAAgAUAAIAA"
#define PSBT_FINALIZED "cHNidP8BAJoCAAAAAljoeiG1ba8MI76OcHBFbDNvfLqlyHV5JPVFiHuyq911AAAAAAD/////g40EJ9DsZQpoqka7CwmK6kQiwHGyyng1Kgd5WdB86h0BAAAAAP////8CcKrwCAAAAAAWABTYXCtx0AYLCcmIauuBXlCZHdoSTQDh9QUAAAAAFgAUAK6pouXw+HaliN9VRuh0LR2HAI8AAAAAAAEAuwIAAAABqtc5MQGL0l+ErkALaISL4J23BurCrBgpi6vucatlb4sAAAAASEcwRAIgWPb8fGoz4bMVSNSByCbAFb0wE1qtQs1neQ2rZtKtJDsCIEoc7SYExnNbY5PltBaR3XiwDwxZQvufdRhW+qk4FX26Af7///8CgPD6AgAAAAAXqRQPuUY0IWlrgsgzryQceMF9295JNIfQ8gonAQAAABepFCnKdPigj4GZlCgYXJe12FLkBj9hh2UAAAABB9oARzBEAiB0AYrUGACXuHMyPAAVcgs2hMyBI4kQSOfbzZtVrWecmQIgc9Npt0Dj61Pc76M4I8gHBRTKVafdlUTxV8FnkTJhEYwBSDBFAiEA9hA4swjcHahlo0hSdG8BV3KTQgjG0kRUOTzZm98iF3cCIAVuZ1pnWm0KArhbFOXikHTYolqbV2C+ooFvZhkQoAbqAUdSIQKVg785rgpgl0etGZrd1jT6YQhVnWxc05tMIYPxq5bgfyEC2rYf9JoU22p9ArDNH7t4/EsYMStbTlTa5Nui+/71NtdSrgABASAAwusLAAAAABepFLf1+vQOPUClpFmx2zU18rcvqSHohwEHIyIAIIwjUxc3Q7WV37Sge3K6jkLjeX2nTof+fZ10l+OyAokDAQjaBABHMEQCIGLrelVhB6fHP0WsSrWh3d9vcHX7EnWWmn84Pv/3hLyyAiAMBdu3Rw2/LwhVfdNWxzJcHtMJE+mWzThAlF2xIijaXwFHMEQCIGX0W6WZi1mif/4ae+0BavHx+Q1Us6qPdFCqX1aiUQO9AiB/ckcDrR7blmgLKEtW1P/LiPf7dZ6rvgiqMPKbhROD0gFHUiEDCJ3BDHrG21T5EymvYXMz2ziM6tDCMfcjN50bmQMLAtwhAjrdkE89bc9Z3bkGsN7iNSm3/7ntUOXoYVGSaGAiHw5zUq4AIgIDqaTDf1mW06ol26xrVwrwZQOUSSlCRgs1R1Ptnuylh3EQ2QxqTwAAAIAAAACABAAAgAAiAgJ/Y5l1fS7/VaE2rQLGhLGDi2VW5fG2s0KCqUtrUAUQlhDZDGpPAAAAgAAAAIAFAACAAA=="
#define PSBT_EXTRACTED_TX "0200000000010258e87a21b56daf0c23be8e7070456c336f7cbaa5c8757924f545887bb2abdd7500000000da00473044022074018ad4180097b873323c0015720b3684cc8123891048e7dbcd9b55ad679c99022073d369b740e3eb53dcefa33823c8070514ca55a7dd9544f157c167913261118c01483045022100f61038b308dc1da865a34852746f015772934208c6d24454393cd99bdf2217770220056e675a675a6d0a02b85b14e5e29074d8a25a9b5760bea2816f661910a006ea01475221029583bf39ae0a609747ad199addd634fa6108559d6c5cd39b4c2183f1ab96e07f2102dab61ff49a14db6a7d02b0cd1fbb78fc4b18312b5b4e54dae4dba2fbfef536d752aeffffffff838d0427d0ec650a68aa46bb0b098aea4422c071b2ca78352a077959d07cea1d01000000232200208c2353173743b595dfb4a07b72ba8e42e3797da74e87fe7d9d7497e3b2028903ffffffff0270aaf00800000000160014d85c2b71d0060b09c9886aeb815e50991dda124d00e1f5050000000016001400aea9a2e5f0f876a588df5546e8742d1d87008f000400473044022062eb7a556107a7c73f45ac4ab5a1dddf6f7075fb1275969a7f383efff784bcb202200c05dbb7470dbf2f08557dd356c7325c1ed30913e996cd3840945db12228da5f01473044022065f45ba5998b59a27ffe1a7bed016af1f1f90d54b3aa8f7450aa5f56a25103bd02207f724703ad1edb96680b284b56d4ffcb88f7fb759eabbe08aa30f29b851383d20147522103089dc10c7ac6db54f91329af617333db388cead0c231f723379d1b99030b02dc21023add904f3d6dcf59ddb906b0dee23529b7ffb9ed50e5e86151926860221f0e7352ae00000000"

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__data_output__op_return)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"data\": \"deadbeef\"}]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    const auto& out = response.at("result").at("vout").at(0);
    BOOST_REQUIRE_EQUAL(out.at("value").as_double(), 0.0);
    BOOST_REQUIRE_EQUAL(as_text(out.at("scriptPubKey").at("hex")), "6a04deadbeef");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createpsbt__data_output__decodes)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createpsbt", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"data\": \"deadbeef\"}]");
    const auto response = rpc("decodepsbt", "[\"" + as_text(created.at("result")) + "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("psbt_version").as_int64(), 0);
    const auto& out = result.at("tx").at("vout").at(0);
    BOOST_REQUIRE_EQUAL(as_text(out.at("scriptPubKey").at("hex")), "6a04deadbeef");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decodepsbt__updater__expected_scripts)
{
    const auto response = rpc("decodepsbt", "[\"" PSBT_UPDATER "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(result.at("psbt_version").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("inputs").as_array().size(), 2u);
    BOOST_REQUIRE(result.at("inputs").at(0).as_object().contains("non_witness_utxo"));
    BOOST_REQUIRE(result.at("inputs").at(0).as_object().contains("redeem_script"));
    BOOST_REQUIRE(result.at("inputs").at(1).as_object().contains("witness_utxo"));
    BOOST_REQUIRE_EQUAL(result.at("outputs").as_array().size(), 2u);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__combinepsbt__two_signed__combined)
{
    const auto response = rpc("combinepsbt", "[[\"" PSBT_SIGNER_A "\", \"" PSBT_SIGNER_B "\"]]");
    const auto decoded = rpc("decodepsbt", "[\"" + as_text(response.at("result")) + "\"]");
    const auto& sigs = decoded.at("result").at("inputs").at(0).at("partial_signatures");
    BOOST_REQUIRE_EQUAL(sigs.as_object().size(), 2u);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__finalizepsbt__combined__extracted_transaction)
{
    const auto response = rpc("finalizepsbt", "[\"" PSBT_COMBINED "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.at("complete").as_bool());
    BOOST_REQUIRE_EQUAL(as_text(result.at("hex")), PSBT_EXTRACTED_TX);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__finalizepsbt__no_extract__psbt)
{
    const auto response = rpc("finalizepsbt", "[\"" PSBT_COMBINED "\", false]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(result.at("complete").as_bool());
    BOOST_REQUIRE(result.as_object().contains("psbt"));
    BOOST_REQUIRE(!result.as_object().contains("hex"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__finalizepsbt__unsigned__incomplete)
{
    const auto response = rpc("finalizepsbt", "[\"" PSBT_UPDATER "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE(!result.at("complete").as_bool());
    BOOST_REQUIRE(result.as_object().contains("psbt"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__analyzepsbt__finalized__extractor_next)
{
    const auto response = rpc("analyzepsbt", "[\"" PSBT_FINALIZED "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("next")), "extractor");
    BOOST_REQUIRE(result.as_object().contains("estimated_vsize"));
    BOOST_REQUIRE(result.at("inputs").at(0).at("is_final").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__analyzepsbt__updater__signer_next)
{
    const auto response = rpc("analyzepsbt", "[\"" PSBT_UPDATER "\"]");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("next")), "signer");
    BOOST_REQUIRE(result.at("inputs").at(0).at("has_utxo").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__joinpsbts__single__invalid)
{
    const auto response = rpc("joinpsbts", "[[\"" PSBT_UPDATER "\"]]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__converttopsbt__created_raw__psbt)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("converttopsbt", "[\"" + as_text(created.at("result")) + "\"]");
    const auto decoded = rpc("decodepsbt", "[\"" + as_text(response.at("result")) + "\"]");
    BOOST_REQUIRE_EQUAL(decoded.at("result").at("inputs").as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__utxoupdatepsbt__descriptors__not_implemented)
{
    const auto response = rpc("utxoupdatepsbt", "[\"" PSBT_UPDATER "\", [\"wpkh(abc)\"]]");
    REQUIRE_NO_THROW_TRUE(response.as_object().contains("error"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__utxoupdatepsbt__no_matching_utxos__round_trips)
{
    const auto response = rpc("utxoupdatepsbt", "[\"" PSBT_UPDATER "\"]");
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")), PSBT_UPDATER);
}

BOOST_AUTO_TEST_SUITE_END()

// websocket authorization
// ----------------------------------------------------------------------------

// ws frames carry no headers, so authorization is established by basic auth
// on the upgrade request and latched on the channel for the connection. An
// unauthorized upgrade is refused (401), as bitcoind has no in-band method.
BOOST_FIXTURE_TEST_SUITE(bitcoind_credentialed_tests,
    bitcoind_credentialed_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rpc__websocket__credentialed_upgrade__dispatches)
{
    BOOST_REQUIRE(!ws_upgrade(BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD));

    const auto response = ws_rpc("getblockcount");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__websocket__uncredentialed_upgrade__refused)
{
    BOOST_REQUIRE(ws_upgrade());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__websocket__wrong_password_upgrade__refused)
{
    BOOST_REQUIRE(ws_upgrade(BITCOIND_TEST_USERNAME, "wrong"));
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__websocket__wrong_username_upgrade__refused)
{
    BOOST_REQUIRE(ws_upgrade("wrong", BITCOIND_TEST_PASSWORD));
}

BOOST_AUTO_TEST_SUITE_END()

// scoped credential
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(bitcoind_scoped_credential_tests,
    bitcoind_scoped_credential_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_scoped_credential__post_listed_method__ok)
{
    const auto result = rpc_status(BITCOIND_TEST_SCOPED_METHOD, BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD);
    BOOST_REQUIRE_EQUAL(result, status::ok);
}

BOOST_AUTO_TEST_CASE(bitcoind_scoped_credential__post_unlisted_method__forbidden)
{
    const auto result = rpc_status("getbestblockhash", BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD);
    BOOST_REQUIRE_EQUAL(result, status::forbidden);
}

BOOST_AUTO_TEST_CASE(bitcoind_scoped_credential__post_unknown_method__forbidden)
{
    const auto result = rpc_status("nosuchmethod", BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD);
    BOOST_REQUIRE_EQUAL(result, status::forbidden);
}

BOOST_AUTO_TEST_CASE(bitcoind_scoped_credential__websocket_listed_method__result)
{
    BOOST_REQUIRE(!ws_upgrade(BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD));

    const auto response = ws_rpc(BITCOIND_TEST_SCOPED_METHOD);
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(bitcoind_scoped_credential__websocket_unlisted_method__dropped)
{
    BOOST_REQUIRE(!ws_upgrade(BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD));

    const auto response = ws_rpc_dropped("getbestblockhash");
    REQUIRE_NO_THROW_TRUE(response.at("dropped").as_bool());
}

BOOST_AUTO_TEST_SUITE_END()

// witness
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(bitcoind_witness_tests, bitcoind_witness_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getrawtransaction__witness_tx__wtxid_differs)
{
    const auto& tx = *test::block1a.transactions_ptr()->front();
    const auto txid = tx.hash(false);
    const auto txid_hex = encode_hash(txid);
    const auto response = rpc("getrawtransaction", hash_param(txid, "1"));
    const auto& result = response.at("result");

    BOOST_REQUIRE_EQUAL(as_text(result.at("txid")), txid_hex);
    BOOST_REQUIRE_NE(as_text(result.at("hash")), txid_hex);

    const auto weight = result.at("weight").as_int64();
    BOOST_REQUIRE_EQUAL(result.at("vsize").as_int64(), (weight + 3) / 4);
}

BOOST_AUTO_TEST_SUITE_END()
