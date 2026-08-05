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
#include "btcd_setup_fixture.hpp"

using namespace system;
static const code not_found{ server::error::not_found };
static const code not_implemented{ server::error::not_implemented };
static const code invalid_argument{ server::error::invalid_argument };
static const code subscription_limit{ server::error::subscription_limit };
static const code unauthorized{ network::error::unauthorized };
static const code unexpected_method{ network::error::unexpected_method };

// mock_block10 chains onto block9 and pays found_address from its second
// transaction only (its other outputs pay distinct key/script hashes), so a
// filter watching found_address matches exactly one of its transactions.
static const std::string found_address{ "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM" };
static const std::string other_address{ "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn" };
static const std::string bogus_address{ "not-an-address" };

namespace {

const auto block9 = encode_hash(test::block9_hash);

std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

std::string coinbase_txid(const chain::block& block) NOEXCEPT
{
    return encode_hash(block.transactions_ptr()->front()->hash(false));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(btcd_tests, btcd_ten_block_setup_fixture)

// session management
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__authenticate__no_credential_configured__unauthorized)
{
    // Without a credential configured the connection is authorized from the
    // start, and authenticate is invalid once authorization is established.
    const auto result = rpc_error("authenticate", R"(["user","pass"])");
    BOOST_REQUIRE_EQUAL(result, unauthorized.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__session__default__returns_id)
{
    const auto response = rpc("session");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE(response.at("result").as_object().contains("id"));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getcurrentnet__mainnet__magic)
{
    // The fixture configures mainnet, magic 3652501241 (0xd9b4bef9).
    const auto response = rpc("getcurrentnet");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 3652501241);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getbestblock__ten_block_store__block9)
{
    const auto response = rpc("getbestblock");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getinfo__http_post__dispatched)
{
    // getinfo is btcd-only, so this exercises the btcd post transport (real
    // clients issue post-based capability checks before any ws traffic).
    const auto response = http_rpc("getinfo");
    REQUIRE_NO_THROW_TRUE(response.at("result").as_object().contains("blocks"));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__authenticate__http_post__unexpected_method)
{
    // authenticate is websocket-only (as btcd): not part of the post surface.
    const auto response = http_rpc("authenticate", R"(["user","pass"])");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    BOOST_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), unexpected_method.value());
}

// block subscription
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__notifyblocks__default__null_result)
{
    const auto response = rpc("notifyblocks");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__stopnotifyblocks__subscribed__null_result)
{
    // Subscribe first so stop is meaningful.
    const auto subscribed = rpc("notifyblocks");
    REQUIRE_NO_THROW_TRUE(subscribed.at("result").is_null());

    const auto response = rpc("stopnotifyblocks");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

// bitcoind interface methods (bridged into the ws dispatcher)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__getblockcount__ten_block_store__nine)
{
    const auto response = rpc("getblockcount");
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

// Address/outpoint filtering (loadtxfilter, filteredblockconnected/
// disconnected, rescanblocks).
// ----------------------------------------------------------------------------
// blocks 1-9 are p2pk coinbases with no inter-block spends, so they cannot
// exercise address matching -- mock_block10 is added where a match is required.

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__valid_address__null_result)
{
    const auto response = rpc("loadtxfilter", (boost_format(R"([true,["%1%"],[]])") % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__invalid_address__invalid_argument)
{
    const auto result = rpc_error("loadtxfilter", (boost_format(R"([true,["%1%"],[]])") % bogus_address).str());
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__valid_outpoint__null_result)
{
    const auto request = R"([true,[],[{"hash":"%1%","index":0}]])";
    const auto response = rpc("loadtxfilter", (boost_format(request) % coinbase_txid(test::block1)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__malformed_outpoint__invalid_argument)
{
    const auto result = rpc_error("loadtxfilter", R"([true,[],[{"hash":"00"}]])");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescanblocks__unknown_hash__not_found)
{
    // 'blockhashes' is one positional arg that is itself an array, so the
    // wire params need double-wrapping: [[...]], not [...].
    const auto result = rpc_error("rescanblocks", (boost_format(R"([["%1%"]])") % encode_hash(null_hash)).str());
    BOOST_REQUIRE_EQUAL(result, not_found.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescanblocks__no_filter_match__empty_result)
{
    rpc("loadtxfilter", (boost_format(R"([true,["%1%"],[]])") % found_address).str());

    const auto response = rpc("rescanblocks", (boost_format(R"([["%1%"]])") % encode_hash(test::block1_hash)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescanblocks__address_match__matched_transaction)
{
    const auto block10 = encode_hash(test::mock_block10.hash());
    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    rpc("loadtxfilter", (boost_format(R"([true,["%1%"],[]])") % found_address).str());

    const auto response = rpc("rescanblocks", (boost_format(R"([["%1%"]])") % block10).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_REQUIRE_EQUAL(as_text(result.front().at("hash")), block10);
    BOOST_REQUIRE_EQUAL(result.front().at("transactions").as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__filteredblockconnected__address_match__delivered)
{
    rpc("notifyblocks");
    rpc("loadtxfilter", (boost_format(R"([true,["%1%"],[]])") % found_address).str());

    BOOST_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, false, false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    notify(node::chase::organized, { 10_u32 });

    const auto blockconnected = receive_notification();
    BOOST_REQUIRE_EQUAL(as_text(blockconnected.at("method")), "blockconnected");

    const auto filtered = receive_notification();
    BOOST_REQUIRE_EQUAL(as_text(filtered.at("method")), "filteredblockconnected");

    const auto& params = filtered.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params.size(), 3u);
    BOOST_REQUIRE_EQUAL(params[0].as_int64(), 10);
    BOOST_REQUIRE_EQUAL(params[2].as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescan__unknown_beginblock__not_found)
{
    const auto result = rpc_error("rescan", (boost_format(R"(["%1%",[],[],""])") % encode_hash(null_hash)).str());
    BOOST_REQUIRE_EQUAL(result, not_found.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescan__no_addresses_or_outpoints__rescan_finished)
{
    const auto response = rpc("rescan", (boost_format(R"(["%1%",[],[],""])") % block9).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());

    const auto finished = receive_notification();
    BOOST_REQUIRE_EQUAL(as_text(finished.at("method")), "rescanfinished");

    const auto& params = finished.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params.size(), 3u);
    BOOST_REQUIRE_EQUAL(as_text(params[0]), block9);
    BOOST_REQUIRE_EQUAL(params[1].as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescan__with_addresses__not_implemented)
{
    const auto request = R"(["%1%",["%2%"],[],""])";
    const auto result = rpc_error("rescan", (boost_format(request) % block9 % found_address).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

// Generic btcd-tooling compatibility
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__getdifficulty__ten_block_store__number)
{
    const auto response = rpc("getdifficulty");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getinfo__ten_block_store__nine)
{
    const auto response = rpc("getinfo");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("testnet").as_bool(), false);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getnettotals__untracked_counters__zero)
{
    const auto response = rpc("getnettotals");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("totalbytesrecv").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(result.at("totalbytessent").as_int64(), 0);
    BOOST_REQUIRE(result.at("timemillis").as_int64() > 0);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getnetworkhashps__ten_block_store__number)
{
    const auto response = rpc("getnetworkhashps");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__createrawtransaction__one_input_one_output__decodes)
{
    const auto request = R"([[{"txid":"%1%","vout":0}],{"%2%":0.01}])";
    const auto response = rpc("createrawtransaction", (boost_format(request) % coinbase_txid(test::block1) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    data_chunk data{};
    BOOST_REQUIRE(decode_base16(data, as_text(response.at("result"))));

    const chain::transaction tx{ data, false };
    BOOST_REQUIRE(tx.is_valid());
    BOOST_REQUIRE_EQUAL(tx.inputs_ptr()->size(), 1u);
    BOOST_REQUIRE_EQUAL(tx.outputs_ptr()->size(), 1u);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__decoderawtransaction__block1_coinbase__expected_txid)
{
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto request = R"(["%1%"])";
    const auto response = rpc("decoderawtransaction", (boost_format(request) % encode_base16(coinbase.to_data(false))).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(as_text(response.at("result").at("txid")), encode_hash(coinbase.hash(false)));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__decoderawtransaction__malformed_hex__invalid_argument)
{
    const auto result = rpc_error("decoderawtransaction", R"(["not-hex"])");
    BOOST_REQUIRE_EQUAL(result, invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__decodescript__pay_public_key__pubkey)
{
    // block1's coinbase output is a p2pk script (early mainnet convention).
    const auto& script = test::block1.transactions_ptr()->front()->outputs_ptr()->front()->script();
    const auto request = R"(["%1%"])";
    const auto response = rpc("decodescript", (boost_format(request) % encode_base16(script.to_data(false))).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(as_text(result.at("type")), "pubkey");
    BOOST_REQUIRE(result.contains("asm"));
    BOOST_REQUIRE(result.contains("p2sh"));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__validateaddress__valid_address__isvalid_true)
{
    const auto request = R"(["%1%"])";
    const auto response = rpc("validateaddress", (boost_format(request) % found_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("isvalid").as_bool(), true);
    BOOST_REQUIRE_EQUAL(as_text(result.at("address")), found_address);
    BOOST_REQUIRE_EQUAL(result.at("iswitness").as_bool(), false);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__validateaddress__invalid_address__isvalid_false)
{
    const auto request = R"(["%1%"])";
    const auto response = rpc("validateaddress", (boost_format(request) % bogus_address).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BOOST_REQUIRE_EQUAL(response.at("result").at("isvalid").as_bool(), false);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__help__default__method_list)
{
    const auto response = rpc("help");
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BOOST_REQUIRE_NE(as_text(response.at("result")).find("getcurrentnet"), std::string::npos);
}

// not implemented stubs
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__stop__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("stop"), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__notifynewtransactions__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("notifynewtransactions", "[false]"), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__stopnotifynewtransactions__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("stopnotifynewtransactions"), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__notifyreceived__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("notifyreceived", "[[]]"), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__stopnotifyreceived__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("stopnotifyreceived", "[[]]"), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__notifyspent__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("notifyspent", "[[]]"), not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__stopnotifyspent__default__not_implemented)
{
    BOOST_REQUIRE_EQUAL(rpc_error("stopnotifyspent", "[[]]"), not_implemented.value());
}

// response envelope
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__response__sequential_requests__id_matches_request)
{
    // request_id_ increments each call: first call uses id=0, second id=1.
    const auto r0 = rpc("session");
    const auto r1 = rpc("notifyblocks");
    BOOST_REQUIRE_EQUAL(r0.at("id").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(r1.at("id").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__unknown_method__default__unexpected_method)
{
    BOOST_REQUIRE_EQUAL(rpc_error("nosuchmethod"), unexpected_method.value());

    // The connection survives an unknown method (as btcd).
    const auto follow_up = rpc("session");
    REQUIRE_NO_THROW_TRUE(follow_up.at("result").is_object());
}

BOOST_AUTO_TEST_SUITE_END()

// Filter limit (btcd.maximum_filters): loadtxfilter watches are bounded per
// channel (DoS guard).
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_limited_filter_tests, btcd_limited_filter_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_limited_filter__loadtxfilter__over_limit__subscription_limit)
{
    // The fixture allows one watch; the second address exceeds it.
    const auto request = R"([true,["%1%","%2%"],[]])";
    const auto result = rpc_error("loadtxfilter", (boost_format(request) % found_address % other_address).str());
    BOOST_REQUIRE_EQUAL(result, subscription_limit.value());
}

BOOST_AUTO_TEST_SUITE_END()

// Address index requirement: address watching and rescan are index queries,
// not_implemented when the index is disabled.
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_no_index_tests, btcd_no_index_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_no_index__loadtxfilter__address__not_implemented)
{
    const auto request = R"([true,["%1%"],[]])";
    const auto result = rpc_error("loadtxfilter", (boost_format(request) % found_address).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_CASE(btcd_no_index__loadtxfilter__outpoint_only__null_result)
{
    // Outpoint watching is spender lookup, not an address index query.
    const auto request = R"([true,[],[{"hash":"%1%","index":0}]])";
    const auto response = rpc("loadtxfilter", (boost_format(request) % coinbase_txid(test::block1)).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(btcd_no_index__rescanblocks__any__not_implemented)
{
    const auto request = R"([["%1%"]])";
    const auto result = rpc_error("rescanblocks", (boost_format(request) % encode_hash(test::block1_hash)).str());
    BOOST_REQUIRE_EQUAL(result, not_implemented.value());
}

BOOST_AUTO_TEST_SUITE_END()

// Method-scoped credentials (channel_http::permitted()): a credential's
// optional 'user:pass:method,...' suffix restricts which methods it may
// call, enforced over ws by protocol_btcd::dispatch_websocket.
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_scoped_credential_tests, btcd_scoped_credential_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_scoped_credential__session__listed_method__permitted)
{
    BOOST_REQUIRE(authenticate());

    // BTCD_TEST_SCOPED_METHOD ("session") is the credential's only method.
    const auto session = rpc("session");
    REQUIRE_NO_THROW_TRUE(session.at("result").is_object());
}

BOOST_AUTO_TEST_CASE(btcd_scoped_credential__notifyblocks__unlisted_method__unauthorized)
{
    BOOST_REQUIRE(authenticate());

    // notifyblocks is implemented, so rejection is permitted()'s doing.
    BOOST_REQUIRE_EQUAL(rpc_error("notifyblocks"), unauthorized.value());
}

BOOST_AUTO_TEST_SUITE_END()

// authenticate (protocol_btcd::handle_authenticate): exercises the
// credential-configured branch, not covered by btcd_tests above (whose
// fixture leaves btcd.credential unset, so every one of those connections
// takes the no-op path).
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_auth_tests, btcd_credentialed_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_auth__authenticate__correct_credentials__null_result)
{
    const auto request = R"(["%1%","%2%"])";
    const auto response = rpc("authenticate", (boost_format(request) % BTCD_TEST_USERNAME % BTCD_TEST_PASSWORD).str());
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BOOST_AUTO_TEST_CASE(btcd_auth__session__authenticated__permitted)
{
    BOOST_REQUIRE(authenticate());

    // Connection survives, and other methods now work.
    const auto session = rpc("session");
    REQUIRE_NO_THROW_TRUE(session.at("result").is_object());
}

BOOST_AUTO_TEST_CASE(btcd_auth__authenticate__wrong_password__unauthorized)
{
    const auto request = R"(["%1%","definitely-wrong"])";
    const auto result = rpc_error("authenticate", (boost_format(request) % BTCD_TEST_USERNAME).str());
    BOOST_REQUIRE_EQUAL(result, unauthorized.value());
}

BOOST_AUTO_TEST_CASE(btcd_auth__session__not_authenticated__unauthorized)
{
    // 'session' as the first message, with a credential configured: any
    // method but authenticate is invalid until authorization is established.
    BOOST_REQUIRE_EQUAL(rpc_error("session"), unauthorized.value());
}

BOOST_AUTO_TEST_CASE(btcd_auth__authenticate__already_authenticated__unauthorized)
{
    BOOST_REQUIRE(authenticate());

    // authenticate is invalid once authorization is established (as btcd).
    const auto request = R"(["%1%","%2%"])";
    const auto result = rpc_error("authenticate", (boost_format(request) % BTCD_TEST_USERNAME % BTCD_TEST_PASSWORD).str());
    BOOST_REQUIRE_EQUAL(result, unauthorized.value());
}

BOOST_AUTO_TEST_SUITE_END()
