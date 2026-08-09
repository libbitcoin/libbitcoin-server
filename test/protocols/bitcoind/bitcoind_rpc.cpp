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

bool is_not_implemented(const boost::json::value& response) NOEXCEPT
{
    return has_error(response) &&
        response.at("error").at("message").as_string() == "not_implemented";
}

const std::vector<std::string> rejected_methods
{
    "dumptxoutset",
    "loadtxoutset",
    "clearbanned",
    "listbanned",
    "setban",
    "stop"
};

const std::vector<std::string> wip_methods
{
    "getblockfrompeer",
    "getchainstates",
    "getdescriptoractivity",
    "preciousblock",
    "scanblocks",
    "waitforblock",
    "waitforblockheight",
    "waitfornewblock",
    "analyzepsbt",
    "combinepsbt",
    "converttopsbt",
    "createpsbt",
    "decodepsbt",
    "finalizepsbt",
    "joinpsbts",
    "descriptorprocesspsbt",
    "utxoupdatepsbt",
    "getmininginfo",
    "submitblock",
    "submitheader",
    "addnode",
    "disconnectnode",
    "exportasmap",
    "getaddednodeinfo",
    "getaddrmaninfo",
    "getnodeaddresses",
    "getpeerinfo",
    "ping",
    "setnetworkactive",
    "createmultisig",
    "deriveaddresses",
    "getdescriptorinfo",
    "getopenrpcinfo",
    "getzmqnotifications"
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

const std::vector<std::string> pending_methods
{
    "getmempoolancestors",
    "getmempoolcluster",
    "getmempooldescendants",
    "getmempoolentry",
    "getmempoolinfo",
    "getrawmempool",
    "gettxspendingprevout",
    "importmempool",
    "abortprivatebroadcast",
    "getprivatebroadcastinfo",
    "submitpackage",
    "getblocktemplate",
    "getprioritisedtransactions",
    "prioritisetransaction",
    "estimatesmartfee"
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

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getblockchaininfo__ten_block_store__expected)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE(result.at("headers").is_int64());
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblockhash")), block9);
    BOOST_REQUIRE(result.as_object().contains("target"));
    BOOST_REQUIRE(result.at("warnings").is_string());
    BOOST_REQUIRE(result.at("initialblockdownload").is_bool());
    BOOST_REQUIRE(result.at("chainwork").is_string());
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

// bip9_softforks is a btcd-endpoint field (see btcd_rpc tests), removed from
// bitcoind's getblockchaininfo in Core 0.19 and absent from the fork.
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

// control, mining, rawtransactions, util (moved from btcd)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__help__default__implemented_method_list)
{
    const auto response = rpc("help");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_NE(as_text(response.at("result")).find("getblockcount"), std::string::npos);
    BOOST_REQUIRE_EQUAL(as_text(response.at("result")).find("getblockstats"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getnetworkhashps__default__number)
{
    const auto response = rpc("getnetworkhashps");
    BOOST_REQUIRE(response.at("result").is_double() || response.at("result").is_int64());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__createrawtransaction__one_in_one_out__hex)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto response = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decoderawtransaction__created__round_trips)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto created = rpc("createrawtransaction", "[[{\"txid\":\"" + txid + "\",\"vout\":0}], {\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\": 0.001}]");
    const auto response = rpc("decoderawtransaction", "[\"" + as_text(created.at("result")) + "\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(response.at("result").at("locktime").as_int64(), 0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__decodescript__p2kh__pubkeyhash)
{
    const auto response = rpc("decodescript", "[\"76a914000000000000000000000000000000000000000088ac\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(as_text(response.at("result").at("type")), "pubkeyhash");
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__genesis__valid)
{
    const auto response = rpc("validateaddress", "[\"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").at("isvalid").as_bool());
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__validateaddress__garbage__invalid)
{
    const auto response = rpc("validateaddress", "[\"notanaddress\"]");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE(!response.at("result").at("isvalid").as_bool());
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

// not implemented
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rpc__not_implemented__error)
{
    const std::vector<std::pair<std::string, std::string>> methods
    {
        { "getblockstats", "[0]" },
        { "gettxoutsetinfo", "[]" },
        { "scantxoutset", "[\"start\", []]" },
        { "pruneblockchain", "[1]" },
        { "savemempool", "[]" }
    };

    for (const auto& [method, params]: methods)
        BOOST_REQUIRE_MESSAGE(has_error(rpc(method, params)), method);
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__rejected__not_implemented)
{
    for (const auto& method: rejected_methods)
    {
        BOOST_REQUIRE_MESSAGE(is_not_implemented(rpc(method, "[]")), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__wip__not_implemented)
{
    for (const auto& method: wip_methods)
    {
        BOOST_REQUIRE_MESSAGE(is_not_implemented(rpc(method, "[]")), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__scope__not_implemented)
{
    for (const auto& method: scope_methods)
    {
        BOOST_REQUIRE_MESSAGE(is_not_implemented(rpc(method, "[]")), method);
    }
}

BOOST_AUTO_TEST_CASE(bitcoind_rpc__pending__not_implemented)
{
    for (const auto& method: pending_methods)
    {
        BOOST_REQUIRE_MESSAGE(is_not_implemented(rpc(method, "[]")), method);
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

BOOST_AUTO_TEST_CASE(bitcoind_rpc__getchaintips__ten_block_store__active)
{
    const auto response = rpc("getchaintips");
    const auto& active = response.at("result").as_array().at(0);
    BOOST_REQUIRE_EQUAL(active.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(active.at("status")), "active");
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
