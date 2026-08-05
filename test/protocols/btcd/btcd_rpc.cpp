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

namespace {

bool has_error(const boost::json::value& response) NOEXCEPT
{
    return response.is_object() && response.as_object().contains("error") &&
        !response.at("error").is_null();
}

bool has_result(const boost::json::value& response) NOEXCEPT
{
    return response.is_object() && response.as_object().contains("result") &&
        !response.at("result").is_null();
}

std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

const auto block9 = encode_hash(test::block9_hash);

} // namespace

BOOST_FIXTURE_TEST_SUITE(btcd_rpc_tests, btcd_ten_block_setup_fixture)

// session management
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__authenticate__no_auth_configured__null_result)
{
    // Without basic auth configured, authenticate is a no-op success.
    const auto response = rpc("authenticate", R"(["user","pass"])");
    BOOST_REQUIRE(!has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__session__returns_id)
{
    const auto response = rpc("session");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").is_object());
    BOOST_REQUIRE(response.at("result").as_object().contains("id"));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getcurrentnet__mainnet_magic)
{
    // The fixture configures mainnet, magic 3652501241 (0xd9b4bef9).
    const auto response = rpc("getcurrentnet");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 3652501241);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getbestblock__ten_block_store__block9)
{
    const auto response = rpc("getbestblock");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));

    const auto& result = response.at("result");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__btcd_only_method__reachable_over_http_post)
{
    // getinfo is btcd-only, so this exercises dispatch_extension (real
    // clients issue post-based capability checks before any ws traffic).
    const auto response = http_rpc("getinfo");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").as_object().contains("blocks"));
}

// block subscription
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__notifyblocks__returns_null_result)
{
    const auto response = rpc("notifyblocks");
    BOOST_REQUIRE(!has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__stopnotifyblocks__returns_null_result)
{
    // Subscribe first so stop is meaningful.
    rpc("notifyblocks");
    const auto response = rpc("stopnotifyblocks");
    BOOST_REQUIRE(!has_error(response));
}

// Standard chain methods (bridged into the ws dispatcher)
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__getblockcount__ten_block_store__nine)
{
    const auto response = rpc("getblockcount");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

// Address/outpoint filtering (loadtxfilter, filteredblockconnected/
// disconnected, rescanblocks).
// ----------------------------------------------------------------------------
// blocks 1-9 are p2pk coinbases with no inter-block spends, so they cannot
// exercise address matching -- block10() (a controlled p2kh output chained
// after block9) is built for that purpose.

namespace {

const short_hash& filter_test_hash() NOEXCEPT
{
    static const short_hash hash{ base16_array(
        "0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a") };
    return hash;
}

const wallet::payment_address& filter_test_address() NOEXCEPT
{
    static const wallet::payment_address address{ filter_test_hash() };
    return address;
}

const std::string& filter_test_address_text() NOEXCEPT
{
    static const std::string text{ filter_test_address().encoded() };
    return text;
}

const chain::block& block10() NOEXCEPT
{
    static const chain::block instance
    {
        [&]() NOEXCEPT
        {
            using namespace wallet;
            const chain::transaction coinbase
            {
                1,
                chain::inputs{ chain::input
                {
                    chain::point{}, chain::script{}, 0xffffffff
                } },
                chain::outputs{ chain::output
                {
                    50'0000'0000, filter_test_address().output_script(
                        payment_address::mainnet_p2kh,
                        payment_address::mainnet_p2sh)
                } },
                0
            };

            // Raw persistence (no organize-time validation), so a consistent
            // merkle root is not required.
            return chain::block
            {
                chain::header
                {
                    1, test::block9_hash, null_hash,
                    test::block9.header().timestamp() + 600,
                    test::block9.header().bits(), 0
                },
                chain::transactions{ coinbase }
            };
        }()
    };

    return instance;
}

} // namespace

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__valid_address__acknowledges)
{
    const auto response = rpc("loadtxfilter",
        "[true,[\"" + filter_test_address_text() + "\"],[]]");
    BOOST_REQUIRE(!has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__invalid_address__invalid_argument)
{
    const auto response = rpc("loadtxfilter",
        R"([true,["not-an-address"],[]])");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__valid_outpoint__acknowledges)
{
    const auto txid = encode_hash(
        test::block1.transactions_ptr()->front()->hash(false));
    const auto response = rpc("loadtxfilter", "[true,[],[{\"hash\":\"" +
        txid + "\",\"index\":0}]]");
    BOOST_REQUIRE(!has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__malformed_outpoint__invalid_argument)
{
    const auto response = rpc("loadtxfilter", R"([true,[],[{"hash":"00"}]])");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescanblocks__unknown_hash__not_found)
{
    // 'blockhashes' is one positional arg that is itself an array, so the
    // wire params need double-wrapping: [[...]], not [...].
    const auto response = rpc("rescanblocks", "[[\"" + encode_hash(null_hash) +
        "\"]]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescanblocks__no_filter_match__empty_result)
{
    rpc("loadtxfilter", "[true,[\"" + filter_test_address_text() + "\"],[]]");

    const auto response = rpc("rescanblocks",
        "[[\"" + encode_hash(test::block1_hash) + "\"]]");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").as_array().empty());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescanblocks__address_match__returns_matched_tx)
{
    BOOST_REQUIRE(query_.set(block10(), database::context{ 0, 10, 0 }, false,
        false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(block10().hash()),
        true));

    rpc("loadtxfilter", "[true,[\"" + filter_test_address_text() + "\"],[]]");

    const auto response = rpc("rescanblocks",
        "[[\"" + encode_hash(block10().hash()) + "\"]]");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_REQUIRE_EQUAL(result.front().at("hash").as_string(),
        encode_hash(block10().hash()));
    BOOST_REQUIRE_EQUAL(result.front().at("transactions").as_array().size(),
        1u);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__filteredblockconnected__address_match__delivered)
{
    rpc("notifyblocks");
    rpc("loadtxfilter", "[true,[\"" + filter_test_address_text() + "\"],[]]");

    BOOST_REQUIRE(query_.set(block10(), database::context{ 0, 10, 0 }, false,
        false));
    BOOST_REQUIRE(query_.push_confirmed(query_.to_header(block10().hash()),
        true));

    notify(node::chase::organized, { 10_u32 });

    const auto blockconnected = receive_notification();
    BOOST_REQUIRE_EQUAL(blockconnected.at("method").as_string(),
        "blockconnected");

    const auto filtered = receive_notification();
    BOOST_REQUIRE_EQUAL(filtered.at("method").as_string(),
        "filteredblockconnected");

    const auto& params = filtered.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params.size(), 3u);
    BOOST_REQUIRE_EQUAL(params[0].as_int64(), 10);
    BOOST_REQUIRE_EQUAL(params[2].as_array().size(), 1u);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescan__unknown_beginblock__not_found)
{
    const auto response = rpc("rescan",
        "[\"" + encode_hash(null_hash) + "\",[],[],\"\"]");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescan__no_addrs_no_outpoints__finishes_immediately)
{
    const auto response = rpc("rescan", "[\"" + block9 + "\",[],[],\"\"]");
    BOOST_REQUIRE(!has_error(response));

    const auto finished = receive_notification();
    BOOST_REQUIRE_EQUAL(finished.at("method").as_string(), "rescanfinished");

    const auto& params = finished.at("params").as_array();
    BOOST_REQUIRE_EQUAL(params.size(), 3u);
    BOOST_REQUIRE_EQUAL(params[0].as_string(), block9);
    BOOST_REQUIRE_EQUAL(params[1].as_int64(), 9);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__rescan__with_addresses__not_implemented)
{
    const auto response = rpc("rescan",
        "[\"" + block9 + "\",[\"" + filter_test_address_text() + "\"],[],\"\"]");
    BOOST_REQUIRE(has_error(response));
}

// Generic btcd-tooling compatibility
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__getdifficulty__returns_number)
{
    const auto response = rpc("getdifficulty");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").is_double());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getinfo__ten_block_store__nine)
{
    const auto response = rpc("getinfo");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(result.at("testnet").as_bool(), false);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getnettotals__returns_object)
{
    const auto response = rpc("getnettotals");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE(result.contains("totalbytesrecv"));
    BOOST_REQUIRE(result.contains("totalbytessent"));
    BOOST_REQUIRE(result.at("timemillis").as_int64() > 0);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__getnetworkhashps__returns_number)
{
    const auto response = rpc("getnetworkhashps");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").is_double());
}

BOOST_AUTO_TEST_CASE(btcd_rpc__createrawtransaction__well_formed_hex)
{
    const auto txid = encode_hash(
        test::block1.transactions_ptr()->front()->hash(false));
    const auto response = rpc("createrawtransaction",
        "[[{\"txid\":\"" + txid + "\",\"vout\":0}],{\"" +
        filter_test_address_text() + "\":0.01}]");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));

    data_chunk data{};
    BOOST_REQUIRE(decode_base16(data, response.at("result").as_string()));

    const chain::transaction tx{ data, false };
    BOOST_REQUIRE(tx.is_valid());
    BOOST_REQUIRE_EQUAL(tx.inputs_ptr()->size(), 1u);
    BOOST_REQUIRE_EQUAL(tx.outputs_ptr()->size(), 1u);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__decoderawtransaction__block1_coinbase)
{
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto hex = encode_base16(coinbase.to_data(false));

    const auto response = rpc("decoderawtransaction", "[\"" + hex + "\"]");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("txid").as_string(),
        encode_hash(coinbase.hash(false)));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__decoderawtransaction__malformed_hex__invalid_argument)
{
    const auto response = rpc("decoderawtransaction", R"(["not-hex"])");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__decodescript__pubkey_script__typed_correctly)
{
    // block1's coinbase output is a p2pk script (early mainnet convention).
    const auto& script =
        test::block1.transactions_ptr()->front()->outputs_ptr()->front()->
            script();
    const auto hex = encode_base16(script.to_data(false));

    const auto response = rpc("decodescript", "[\"" + hex + "\"]");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("type").as_string(), "pubkey");
    BOOST_REQUIRE(result.contains("asm"));
    BOOST_REQUIRE(result.contains("p2sh"));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__validateaddress__valid_address__isvalid_true)
{
    const auto response = rpc("validateaddress",
        "[\"" + filter_test_address_text() + "\"]");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.at("isvalid").as_bool(), true);
    BOOST_REQUIRE_EQUAL(result.at("address").as_string(),
        filter_test_address_text());
    BOOST_REQUIRE_EQUAL(result.at("iswitness").as_bool(), false);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__validateaddress__invalid_address__isvalid_false)
{
    const auto response = rpc("validateaddress", R"(["not-an-address"])");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE_EQUAL(
        response.at("result").as_object().at("isvalid").as_bool(), false);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__help__returns_method_list)
{
    const auto response = rpc("help");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").as_string().find("getcurrentnet") !=
        std::string::npos);
}

// not implemented stubs
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__not_implemented__error)
{
    const std::vector<std::pair<std::string, std::string>> methods
    {
        { "stop",                      "[]" },
        { "notifynewtransactions",     "[false]" },
        { "stopnotifynewtransactions", "[]" },
        { "notifyreceived",            "[[]]" },
        { "stopnotifyreceived",        "[[]]" },
        { "notifyspent",               "[[]]" },
        { "stopnotifyspent",           "[[]]" }
    };

    for (const auto& method: methods)
        BOOST_REQUIRE_MESSAGE(has_error(rpc(method.first, method.second)),
            method.first);
}

// response envelope
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__response__id_matches_request)
{
    // request_id_ increments each call: first call uses id=0, second id=1.
    const auto r0 = rpc("session");
    const auto r1 = rpc("notifyblocks");
    BOOST_REQUIRE_EQUAL(r0.at("id").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(r1.at("id").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__unknown_method__error_keeps_connection_alive)
{
    // Unrecognized method returns a json-rpc error rather than dropping the
    // ws connection (matches real btcd, verified by the follow-up call).
    const auto unknown = rpc("nosuchmethod");
    BOOST_REQUIRE(has_error(unknown));

    const auto follow_up = rpc("session");
    BOOST_REQUIRE(!has_error(follow_up));
}

BOOST_AUTO_TEST_SUITE_END()

// Method-scoped credentials (channel_btcd::permitted()): a credential's
// optional 'user:pass:method,...' suffix restricts which methods it may
// call, enforced over ws by protocol_btcd_rpc::dispatch_websocket.
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_scoped_credential_tests,
    btcd_scoped_credential_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_scoped_credential__listed_method__permitted)
{
    const auto auth = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", ")" BTCD_TEST_PASSWORD R"("])");
    BOOST_REQUIRE(!has_error(auth));

    // BTCD_TEST_SCOPED_METHOD ("session") is the credential's only permitted
    // method.
    const auto session = rpc("session");
    BOOST_REQUIRE(!has_error(session));
}

BOOST_AUTO_TEST_CASE(btcd_scoped_credential__unlisted_method__rejected)
{
    const auto auth = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", ")" BTCD_TEST_PASSWORD R"("])");
    BOOST_REQUIRE(!has_error(auth));

    // notifyblocks is a real, implemented handler, not scoped by this
    // credential -- confirms rejection is permitted()'s doing, not the
    // method being unimplemented.
    const auto response = rpc("notifyblocks");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_SUITE_END()

// authenticate (protocol_btcd_rpc::handle_authenticate): exercises the
// credential-configured branch, not covered by btcd_rpc_tests above (whose
// fixture leaves btcd.credential unset, so every one of those connections
// takes the no-op path).
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_auth_tests, btcd_credentialed_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_auth__correct_credentials__succeeds_then_session_works)
{
    const auto auth = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", ")" BTCD_TEST_PASSWORD R"("])");
    BOOST_REQUIRE(!has_error(auth));

    // Connection survives a successful handshake; other methods now work.
    const auto session = rpc("session");
    BOOST_REQUIRE(!has_error(session));
}

BOOST_AUTO_TEST_CASE(btcd_auth__wrong_password__rejected)
{
    const auto response = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", "definitely-wrong"])");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_auth__other_method_before_authenticating__rejected)
{
    // 'session' instead of 'authenticate' as the first message: the
    // handshake requires authenticate specifically when a credential is
    // configured.
    const auto response = rpc("session");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_SUITE_END()
