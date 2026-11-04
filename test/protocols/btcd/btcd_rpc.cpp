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

// Inherited bitcoind paths send bitcoind codes (btcjson uses the same values).
static const code method_not_found{ server::error::bitcoind::method_not_found };
static const code deserialization{ server::error::bitcoind::deserialization_error };

// btcd reports unimplemented and misc alike as -1.
static const code unimplemented{ server::error::btcd::unimplemented };
static const code misc_error{ server::error::btcd::misc_error };
static const code invalid_parameter{ server::error::btcd::invalid_parameter };
static const code invalid_params{ server::error::btcd::invalid_params };
static const code internal{ server::error::btcd::internal_error };
static const code block_not_found{ server::error::btcd::invalid_address_or_key };
static const code node_not_added{ server::error::btcd::client_node_not_added };
static const code node_not_connected{ server::error::btcd::client_node_not_connected };

// mock_block10 chains onto block9 and pays found_address from its second
// transaction only (its other outputs pay distinct key/script hashes), so a
// filter watching found_address matches exactly one of its transactions.
static const std::string found_address{ "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM" };
static const std::string other_address{ "1JqDybm2nWTENrHvMyafbSXXtTk5Uv5QAn" };
static const std::string bogus_address{ "not-an-address" };

namespace {

const auto block9 = encode_hash(test::block9_hash);

std::string as_text(const system::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

std::string coinbase_txid(const chain::block& block) NOEXCEPT
{
    return encode_hash(block.transactions_ptr()->front()->hash(false));
}

} // namespace

BC_FIXTURE_TEST_SUITE(btcd_tests, btcd_ten_block_setup_fixture)

// administrative
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__authenticate__no_credential_configured__unauthorized)
{
    // Without a credential configured the connection is authorized from the
    // start, and authenticate is invalid once authorization is established.
    const auto result = rpc_error("authenticate", R"(["user","pass"])");
    BC_REQUIRE_EQUAL(result, unauthorized.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__authenticate__http_post__method_not_found)
{
    // authenticate is websocket-only, not part of the post surface.
    const auto response = http_rpc("authenticate", R"(["user","pass"])");
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), method_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__help__default__method_list)
{
    const auto response = rpc("help");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());
    BC_REQUIRE_NE(as_text(response.at("result")).find("getcurrentnet"), std::string::npos);
}

BC_AUTO_TEST_CASE(btcd_rpc__session__default__returns_id)
{
    const auto response = rpc("session");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BC_REQUIRE(response.at("result").as_object().contains("id"));
}

// stop is refused by the inherited bitcoind interface.
BC_AUTO_TEST_CASE(btcd_rpc__stop__default__method_not_found)
{
    BC_REQUIRE_EQUAL(rpc_error("stop"), method_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__node__unknown_subcommand__invalid_parameter)
{
    BC_REQUIRE_EQUAL(rpc_error("node", R"(["restart","1.2.3.4:8333"])"), invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__node__unknown_connect_subcommand__invalid_parameter)
{
    BC_REQUIRE_EQUAL(rpc_error("node", R"(["connect","1.2.3.4:8333","forever"])"), invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__node__disconnect_no_channels__node_not_connected)
{
    // The fixture runs no peer sessions, so the terminator identifies none.
    BC_REQUIRE_EQUAL(rpc_error("node", R"(["disconnect","1.2.3.4:8333"])"), node_not_connected.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__node__remove_no_channels__node_not_added)
{
    BC_REQUIRE_EQUAL(rpc_error("node", R"(["remove","42"])"), node_not_added.value());
}

// getters
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__estimatefee__zero_blocks__invalid_params)
{
    BC_REQUIRE_EQUAL(rpc_error("estimatefee", "[0]"), invalid_params.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__estimatefee__uninitialized__misc_error)
{
    // The fixture estimator is not initialized (btcd reports an error).
    BC_REQUIRE_EQUAL(rpc_error("estimatefee", "[2]"), misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getbestblock__ten_block_store__block9)
{
    const auto response = rpc("getbestblock");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result");
    BC_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BC_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilter__filters_disabled__block_not_found)
{
    const auto result = rpc_error("getcfilter", std::format(R"(["{}",0])", block9));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilter__malformed_hash__deserialization)
{
    BC_REQUIRE_EQUAL(rpc_error("getcfilter", R"(["not-a-hash",0])"), deserialization.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilter__unsupported_type__block_not_found)
{
    const auto result = rpc_error("getcfilter", std::format(R"(["{}",1])", block9));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilterheader__filters_disabled__block_not_found)
{
    const auto result = rpc_error("getcfilterheader", std::format(R"(["{}",0])", block9));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getcurrentnet__mainnet__magic)
{
    // The fixture configures mainnet, magic 3652501241 (0xd9b4bef9).
    const auto response = rpc("getcurrentnet");
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 3652501241);
}

BC_AUTO_TEST_CASE(btcd_rpc__getdifficulty__ten_block_store__number)
{
    const auto response = rpc("getdifficulty");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
}

BC_AUTO_TEST_CASE(btcd_rpc__getheaders__block5_locator__headers_after_fork)
{
    const auto response = rpc("getheaders", std::format(R"([["{}"],""])", encode_hash(test::block5_hash)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 4u);
    BC_REQUIRE_EQUAL(as_text(result.front()), encode_base16(test::block6.header().to_data()));
    BC_REQUIRE_EQUAL(as_text(result.back()), encode_base16(test::block9.header().to_data()));
}

BC_AUTO_TEST_CASE(btcd_rpc__getheaders__hashstop__bounded)
{
    const auto request = std::format(R"([["{}"],"{}"])", encode_hash(test::block5_hash), encode_hash(test::block8_hash));
    const auto response = rpc("getheaders", request);
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 3u);
    BC_REQUIRE_EQUAL(as_text(result.back()), encode_base16(test::block8.header().to_data()));
}

BC_AUTO_TEST_CASE(btcd_rpc__getheaders__unknown_locator__headers_from_genesis)
{
    const auto response = rpc("getheaders", std::format(R"([["{}"],""])", encode_hash(system::one_hash)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BC_REQUIRE_EQUAL(response.at("result").as_array().size(), 9u);
}

BC_AUTO_TEST_CASE(btcd_rpc__getheaders__malformed_locator__deserialization)
{
    BC_REQUIRE_EQUAL(rpc_error("getheaders", R"([["not-a-hash"],""])"), deserialization.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getinfo__ten_block_store__nine)
{
    const auto response = rpc("getinfo");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BC_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BC_REQUIRE_EQUAL(result.at("testnet").as_bool(), false);
    BC_REQUIRE(result.at("proxy").as_string().empty());
}

BC_AUTO_TEST_CASE(btcd_rpc__getinfo__http_post__dispatched)
{
    // getinfo is btcd-only, so this exercises the btcd post transport (real
    // clients issue post-based capability checks before any ws traffic).
    const auto response = http_rpc("getinfo");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").as_object().contains("blocks"));
}

BC_AUTO_TEST_CASE(btcd_rpc__getnettotals__no_channels__zero)
{
    const auto response = rpc("getnettotals");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BC_REQUIRE_EQUAL(result.at("totalbytesrecv").as_int64(), 0);
    BC_REQUIRE_EQUAL(result.at("totalbytessent").as_int64(), 0);
    BC_REQUIRE(result.at("timemillis").as_int64() > 0);
}

BC_AUTO_TEST_CASE(btcd_rpc__getnetworkhashps__ten_block_store__number)
{
    const auto response = rpc("getnetworkhashps");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_double());
}

BC_AUTO_TEST_CASE(btcd_rpc__version__default__api_semver)
{
    const auto response = rpc("version");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& api = response.at("result").at("btcdjsonrpcapi");
    BC_REQUIRE_EQUAL(as_text(api.at("versionstring")), "1.3.0");
    BC_REQUIRE_EQUAL(api.at("major").as_int64(), 1);
    BC_REQUIRE_EQUAL(api.at("minor").as_int64(), 3);
    BC_REQUIRE_EQUAL(api.at("patch").as_int64(), 0);
}

// tools
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__createrawtransaction__one_input_one_output__decodes)
{
    constexpr auto request = R"([[{{"txid":"{}","vout":0}}],{{"{}":0.01}}])";
    const auto response = rpc("createrawtransaction", std::format(request, coinbase_txid(test::block1), found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_string());

    data_chunk data{};
    BC_REQUIRE(decode_base16(data, as_text(response.at("result"))));

    const chain::transaction tx{ data, false };
    BC_REQUIRE(tx.is_valid());
    BC_REQUIRE_EQUAL(tx.inputs_ptr()->size(), 1u);
    BC_REQUIRE_EQUAL(tx.outputs_ptr()->size(), 1u);
}

BC_AUTO_TEST_CASE(btcd_rpc__decoderawtransaction__block1_coinbase__expected_txid)
{
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    constexpr auto request = R"(["{}"])";
    const auto response = rpc("decoderawtransaction", std::format(request, encode_base16(coinbase.to_data(false))));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BC_REQUIRE_EQUAL(as_text(response.at("result").at("txid")), encode_hash(coinbase.hash(false)));
}

BC_AUTO_TEST_CASE(btcd_rpc__decoderawtransaction__malformed_hex__deserialization)
{
    const auto result = rpc_error("decoderawtransaction", R"(["not-hex"])");
    BC_REQUIRE_EQUAL(result, deserialization.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__decodescript__pay_public_key__pubkey)
{
    // block1's coinbase output is a p2pk script (early mainnet convention).
    const auto& script = test::block1.transactions_ptr()->front()->outputs_ptr()->front()->script();
    constexpr auto request = R"(["{}"])";
    const auto response = rpc("decodescript", std::format(request, encode_base16(script.to_data(false))));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BC_REQUIRE_EQUAL(as_text(result.at("type")), "pubkey");
    BC_REQUIRE(result.contains("asm"));
    BC_REQUIRE(result.contains("p2sh"));
}

BC_AUTO_TEST_CASE(btcd_rpc__validateaddress__valid_address__isvalid_true)
{
    constexpr auto request = R"(["{}"])";
    const auto response = rpc("validateaddress", std::format(request, found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BC_REQUIRE_EQUAL(result.at("isvalid").as_bool(), true);
    BC_REQUIRE_EQUAL(as_text(result.at("address")), found_address);
    BC_REQUIRE_EQUAL(result.at("iswitness").as_bool(), false);
}

BC_AUTO_TEST_CASE(btcd_rpc__validateaddress__invalid_address__isvalid_false)
{
    constexpr auto request = R"(["{}"])";
    const auto response = rpc("validateaddress", std::format(request, bogus_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
    BC_REQUIRE_EQUAL(response.at("result").at("isvalid").as_bool(), false);
}

// subscription
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__notifyblocks__default__null_result)
{
    const auto response = rpc("notifyblocks");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__stopnotifyblocks__subscribed__null_result)
{
    // Subscribe first so stop is meaningful.
    const auto subscribed = rpc("notifyblocks");
    REQUIRE_NO_THROW_TRUE(subscribed.at("result").is_null());

    const auto response = rpc("stopnotifyblocks");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__notifynewtransactions__default__null_result)
{
    const auto response = rpc("notifynewtransactions", "[false]");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__stopnotifynewtransactions__subscribed__null_result)
{
    const auto subscribed = rpc("notifynewtransactions", "[false]");
    REQUIRE_NO_THROW_TRUE(subscribed.at("result").is_null());

    const auto response = rpc("stopnotifynewtransactions");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__txaccepted__transaction__delivered)
{
    rpc("notifynewtransactions", "[false]");

    const auto& tx = *test::block1.transactions_ptr()->front();
    const node::transaction_t link{ query_.to_tx(tx.hash(false)).value };
    notify(node::chases::transaction{ link });

    const auto accepted = receive_notification();
    BC_REQUIRE_EQUAL(as_text(accepted.at("method")), "txaccepted");

    const auto& params = accepted.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 2u);
    BC_REQUIRE_EQUAL(as_text(params[0]), encode_hash(tx.hash(false)));
    BC_REQUIRE_EQUAL(params[1].as_double(), 50.0);
}

BC_AUTO_TEST_CASE(btcd_rpc__txacceptedverbose__transaction__delivered)
{
    rpc("notifynewtransactions", "[true]");

    const auto& tx = *test::block1.transactions_ptr()->front();
    const node::transaction_t link{ query_.to_tx(tx.hash(false)).value };
    notify(node::chases::transaction{ link });

    const auto accepted = receive_notification();
    BC_REQUIRE_EQUAL(as_text(accepted.at("method")), "txacceptedverbose");

    const auto& params = accepted.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(params[0].at("txid")), encode_hash(tx.hash(false)));
}

// filters
// ----------------------------------------------------------------------------
// blocks 1-9 are p2pk coinbases with no inter-block spends, so they cannot
// exercise address matching -- mock_block10 is added where a match is required.

BC_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__valid_address__null_result)
{
    const auto response = rpc("loadtxfilter", std::format(R"([true,["{}"],[]])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__invalid_address__invalid_argument)
{
    const auto result = rpc_error("loadtxfilter", std::format(R"([true,["{}"],[]])", bogus_address));
    BC_REQUIRE_EQUAL(result, invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__valid_outpoint__null_result)
{
    constexpr auto request = R"([true,[],[{{"hash":"{}","index":0}}]])";
    const auto response = rpc("loadtxfilter", std::format(request, coinbase_txid(test::block1)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__malformed_outpoint__invalid_argument)
{
    const auto result = rpc_error("loadtxfilter", R"([true,[],[{"hash":"00"}]])");
    BC_REQUIRE_EQUAL(result, invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescanblocks__unknown_hash__not_found)
{
    // 'blockhashes' is one positional arg that is itself an array, so the
    // wire params need double-wrapping: [[...]], not [...].
    const auto result = rpc_error("rescanblocks", std::format(R"([["{}"]])", encode_hash(null_hash)));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescanblocks__no_filter_match__empty_result)
{
    rpc("loadtxfilter", std::format(R"([true,["{}"],[]])", found_address));

    const auto response = rpc("rescanblocks", std::format(R"([["{}"]])", encode_hash(test::block1_hash)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").as_array().empty());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescanblocks__address_match__matched_transaction)
{
    const auto block10 = encode_hash(test::mock_block10.hash());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    rpc("loadtxfilter", std::format(R"([true,["{}"],[]])", found_address));

    const auto response = rpc("rescanblocks", std::format(R"([["{}"]])", block10));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.front().at("hash")), block10);
    BC_REQUIRE_EQUAL(result.front().at("transactions").as_array().size(), 1u);
}

BC_AUTO_TEST_CASE(btcd_rpc__filteredblockconnected__address_match__delivered)
{
    rpc("notifyblocks");
    rpc("loadtxfilter", std::format(R"([true,["{}"],[]])", found_address));

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    notify(node::chases::organized{ 10 });

    const auto blockconnected = receive_notification();
    BC_REQUIRE_EQUAL(as_text(blockconnected.at("method")), "blockconnected");

    const auto filtered = receive_notification();
    BC_REQUIRE_EQUAL(as_text(filtered.at("method")), "filteredblockconnected");

    const auto& params = filtered.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 3u);
    BC_REQUIRE_EQUAL(params[0].as_int64(), 10);
    BC_REQUIRE_EQUAL(params[2].as_array().size(), 1u);
}

BC_AUTO_TEST_CASE(btcd_rpc__blockdisconnected__reorganized__delivered)
{
    rpc("notifyblocks");
    notify(node::chases::reorganized{ 9 });

    const auto disconnected = receive_notification();
    BC_REQUIRE_EQUAL(as_text(disconnected.at("method")), "blockdisconnected");

    const auto& params = disconnected.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 3u);
    BC_REQUIRE_EQUAL(as_text(params[0]), block9);
    BC_REQUIRE_EQUAL(params[1].as_int64(), 9);
    BC_REQUIRE_EQUAL(params[2].as_int64(), test::block9.header().timestamp());

    const auto filtered = receive_notification();
    BC_REQUIRE_EQUAL(as_text(filtered.at("method")), "filteredblockdisconnected");

    const auto& filtered_params = filtered.at("params").as_array();
    BC_REQUIRE_EQUAL(filtered_params.size(), 2u);
    BC_REQUIRE_EQUAL(filtered_params[0].as_int64(), 9);
    BC_REQUIRE_EQUAL(as_text(filtered_params[1]), encode_base16(test::header9_data));
}

// transactions
// ----------------------------------------------------------------------------
// found_address is paid only by mock_block10's second transaction (see above),
// so it carries exactly one history entry once that block is confirmed.

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__unused_address__no_tx_info)
{
    const auto result = rpc_error("searchrawtransactions", std::format(R"(["{}"])", found_address));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__invalid_address__invalid_address)
{
    const auto result = rpc_error("searchrawtransactions", std::format(R"(["{}"])", bogus_address));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__public_key__p2pk_transaction)
{
    // A public key implies both the p2pk and derived p2pkh scripts.
    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto& point = coinbase.outputs_ptr()->front()->script().ops().front().data();
    const auto response = rpc("searchrawtransactions", std::format(R"(["{}"])", encode_base16(point)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.front().at("txid")), encode_hash(coinbase.hash(false)));
    BC_REQUIRE_EQUAL(result.front().at("confirmations").as_int64(), 9);
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__zero_count__null_result)
{
    const auto response = rpc("searchrawtransactions", std::format(R"(["{}",1,0,0])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__paid_address__verbose_transaction)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto& paying = *test::mock_block10.transactions_ptr()->at(1);
    const auto response = rpc("searchrawtransactions", std::format(R"(["{}"])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 1u);

    const auto& entry = result.front();
    BC_REQUIRE_EQUAL(as_text(entry.at("txid")), encode_hash(paying.hash(false)));
    BC_REQUIRE_EQUAL(as_text(entry.at("blockhash")), encode_hash(test::mock_block10.hash()));
    BC_REQUIRE_EQUAL(entry.at("confirmations").as_int64(), 1);
    BC_REQUIRE_EQUAL(entry.at("vin").as_array().size(), 2u);
    BC_REQUIRE_EQUAL(entry.at("vout").as_array().size(), 2u);
    BC_REQUIRE(entry.as_object().contains("hex"));
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__not_verbose__serialized_transaction)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto& paying = *test::mock_block10.transactions_ptr()->at(1);
    const auto response = rpc("searchrawtransactions", std::format(R"(["{}",0])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.front()), encode_base16(paying.to_data(true)));
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__skip_past_history__no_tx_info)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto result = rpc_error("searchrawtransactions", std::format(R"(["{}",1,1])", found_address));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__vinextra__prevout_injected)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto response = rpc("searchrawtransactions", std::format(R"(["{}",1,0,100,1])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    // The paying transaction spends the block1 and block2 coinbases (50 btc).
    // The coinbases are p2pk, which has no address (btcd fabricates one).
    const auto& vin = response.at("result").as_array().front().at("vin").as_array();
    BC_REQUIRE_EQUAL(vin.size(), 2u);
    BC_REQUIRE_EQUAL(vin.front().at("prevOut").at("value").as_double(), 50.0);
    BC_REQUIRE(vin.front().at("prevOut").at("addresses").as_array().empty());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__filteraddrs__filtered_vin_vout)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto request = std::format(R"(["{0}",1,0,100,0,false,["{0}"]])", found_address);
    const auto response = rpc("searchrawtransactions", request);
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());

    // No prevout pays found_address and only the first output does.
    const auto& entry = response.at("result").as_array().front();
    BC_REQUIRE_EQUAL(entry.at("vin").as_array().size(), 0u);
    BC_REQUIRE_EQUAL(entry.at("vout").as_array().size(), 1u);
    BC_REQUIRE_EQUAL(entry.at("vout").as_array().front().at("n").as_int64(), 0);
}

// deprecated
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__notifyreceived__invalid_address__invalid_parameter)
{
    const auto result = rpc_error("notifyreceived", std::format(R"([["{}"]])", bogus_address));
    BC_REQUIRE_EQUAL(result, invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__notifyreceived__valid_address__null_result)
{
    const auto response = rpc("notifyreceived", std::format(R"([["{}"]])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__stopnotifyreceived__default__null_result)
{
    const auto response = rpc("stopnotifyreceived", std::format(R"([["{}"]])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__notifyspent__invalid_outpoint__invalid_parameter)
{
    const auto result = rpc_error("notifyspent", R"([[{"hash":"00"}]])");
    BC_REQUIRE_EQUAL(result, invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__notifyspent__valid_outpoint__null_result)
{
    constexpr auto request = R"([[{{"hash":"{}","index":0}}]])";
    const auto response = rpc("notifyspent", std::format(request, coinbase_txid(test::block1)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__stopnotifyspent__default__null_result)
{
    constexpr auto request = R"([[{{"hash":"{}","index":0}}]])";
    const auto response = rpc("stopnotifyspent", std::format(request, coinbase_txid(test::block1)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__recvtx__address_match__delivered_without_notifyblocks)
{
    // recvtx does not require notifyblocks (unlike filteredblockconnected).
    rpc("notifyreceived", std::format(R"([["{}"]])", found_address));

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    notify(node::chases::organized{ 10 });

    const auto recvtx = receive_notification();
    BC_REQUIRE_EQUAL(as_text(recvtx.at("method")), "recvtx");

    const auto& params = recvtx.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 2u);
    BC_REQUIRE(params[0].is_string());
    BC_REQUIRE_EQUAL(params[1].at("height").as_int64(), 10);
    BC_REQUIRE_EQUAL(as_text(params[1].at("hash")), encode_hash(test::mock_block10.hash()));
}

BC_AUTO_TEST_CASE(btcd_rpc__redeemingtx__spent_in_arming_block__delivered)
{
    // The receive match arms the spent watch, and the spender is in that block.
    rpc("notifyreceived", std::format(R"([["{}"]])", found_address));

    BC_REQUIRE(query_.set(test::mock_block13, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block13.hash()), true));

    notify(node::chases::organized{ 10 });

    const auto recvtx = receive_notification();
    BC_REQUIRE_EQUAL(as_text(recvtx.at("method")), "recvtx");

    const auto redeemingtx = receive_notification();
    BC_REQUIRE_EQUAL(as_text(redeemingtx.at("method")), "redeemingtx");
    BC_REQUIRE_EQUAL(redeemingtx.at("params").as_array()[1].at("height").as_int64(), 10);
}

BC_AUTO_TEST_CASE(btcd_rpc__redeemingtx__notified_outpoint_spent__delivered_once)
{
    // The paying transaction in mock_block10 spends block1's coinbase.
    constexpr auto request = R"([[{{"hash":"{}","index":0}}]])";
    rpc("notifyspent", std::format(request, coinbase_txid(test::block1)));

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    notify(node::chases::organized{ 10 });

    const auto redeemingtx = receive_notification();
    BC_REQUIRE_EQUAL(as_text(redeemingtx.at("method")), "redeemingtx");

    const auto& params = redeemingtx.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 2u);
    BC_REQUIRE_EQUAL(params[1].at("height").as_int64(), 10);
}

BC_AUTO_TEST_CASE(btcd_rpc__rescan__unknown_beginblock__not_found)
{
    const auto result = rpc_error("rescan", std::format(R"(["{}",[],[],""])", encode_hash(null_hash)));
    BC_REQUIRE_EQUAL(result, block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescan__invalid_beginblock__not_found)
{
    BC_REQUIRE_EQUAL(rpc_error("rescan", R"(["not-a-hash",[],[],""])"), block_not_found.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescan__no_addresses_or_outpoints__rescan_finished)
{
    const auto response = rpc("rescan", std::format(R"(["{}",[],[],""])", block9));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());

    const auto finished = receive_notification();
    BC_REQUIRE_EQUAL(as_text(finished.at("method")), "rescanfinished");

    const auto& params = finished.at("params").as_array();
    BC_REQUIRE_EQUAL(params.size(), 3u);
    BC_REQUIRE_EQUAL(as_text(params[0]), block9);
    BC_REQUIRE_EQUAL(params[1].as_int64(), 9);
}

BC_AUTO_TEST_CASE(btcd_rpc__rescan__with_addresses__not_implemented)
{
    constexpr auto request = R"(["{}",["{}"],[],""])";
    const auto result = rpc_error("rescan", std::format(request, block9, found_address));
    BC_REQUIRE_EQUAL(result, unimplemented.value());
}

// bitcoind interface methods (bridged into the ws dispatcher)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__getblockcount__ten_block_store__nine)
{
    const auto response = rpc("getblockcount");
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

// tcp (downgrade)
// ----------------------------------------------------------------------------
// btcd is served over the same universal json-rpc channel as bitcoind, so raw
// json on a connection downgrades it from http to a newline-delimited stream.

BC_AUTO_TEST_CASE(btcd_rpc__getblockcount__downgraded__nine)
{
    const auto response = tcp_rpc("getblockcount");
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

BC_AUTO_TEST_CASE(btcd_rpc__getblockcount__downgraded_second__nine)
{
    BC_REQUIRE_EQUAL(tcp_rpc("getblockcount").at("result").as_int64(), 9);

    // The downgrade is latched, so the connection remains tcp.
    BC_REQUIRE_EQUAL(tcp_rpc("getblockcount").at("result").as_int64(), 9);
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilter__downgraded_numeric_hash__dropped)
{
    REQUIRE_NO_THROW_TRUE(tcp_rpc("getcfilter", "[42,0]").at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilter__http_post_numeric_hash__dropped)
{
    REQUIRE_NO_THROW_TRUE(http_rpc("getcfilter", "[42,0]", false).at("dropped").as_bool());
}

// service settings
// ----------------------------------------------------------------------------

// btcd settings derive bitcoind settings, and btcd is an independent service,
// so it reports its own configured section (not that of [bitcoind]).
BC_AUTO_TEST_CASE(btcd_rpc__getnetworkinfo__derived_settings__btcd_section)
{
    const auto response = rpc("getnetworkinfo");
    const auto& result = response.at("result").as_object();
    BC_REQUIRE_EQUAL(result.at("subversion").as_string(),
        "/libbitcoin:btcd/");
}

// response envelope
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__response__sequential_requests__id_matches_request)
{
    // request_id_ increments each call: first call uses id=0, second id=1.
    const auto r0 = rpc("session");
    const auto r1 = rpc("notifyblocks");
    BC_REQUIRE_EQUAL(r0.at("id").as_int64(), 0);
    BC_REQUIRE_EQUAL(r1.at("id").as_int64(), 1);
}

BC_AUTO_TEST_CASE(btcd_rpc__unknown_method__default__method_not_found)
{
    BC_REQUIRE_EQUAL(rpc_error("nosuchmethod"), method_not_found.value());

    // The connection survives an unknown method.
    const auto follow_up = rpc("session");
    REQUIRE_NO_THROW_TRUE(follow_up.at("result").is_object());
}

// The top header context has no taproot flags.
BC_AUTO_TEST_CASE(btcd_rpc__getblockchaininfo__bip9_softforks_taproot__not_yet_active__absent)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BC_REQUIRE(result.as_object().contains("bip9_softforks"));
    BC_REQUIRE(!result.at("bip9_softforks").as_object().contains("taproot"));
}

// The override serves the base fields, and answers exactly once -- the
// blockchain subgroup also defines the method, so without the claim guard
// both would respond and the follow-up would read the stray response.
BC_AUTO_TEST_CASE(btcd_rpc__getblockchaininfo__override__serves_core_fields_once)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BC_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BC_REQUIRE(result.as_object().contains("bestblockhash"));
    BC_REQUIRE(result.as_object().contains("mediantime"));

    const auto follow_up = rpc("getcurrentnet");
    REQUIRE_NO_THROW_TRUE(follow_up.at("result").is_number());
}

// The overridden name is published once (btcd and blockchain both define it).
BC_AUTO_TEST_CASE(btcd_rpc__help__overridden_method__listed_once)
{
    const auto response = rpc("help");
    const auto names = as_text(response.at("result"));
    const auto first = names.find("getblockchaininfo");
    BC_REQUIRE_NE(first, std::string::npos);
    BC_REQUIRE_EQUAL(names.find("getblockchaininfo", add1(first)), std::string::npos);
}

// validation
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__stopnotifyreceived__bogus_address__invalid_parameter)
{
    const auto result = rpc_error("stopnotifyreceived", std::format(R"([["{}"]])", bogus_address));
    BC_REQUIRE_EQUAL(result, invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__stopnotifyspent__bogus_outpoint__invalid_parameter)
{
    BC_REQUIRE_EQUAL(rpc_error("stopnotifyspent", R"([[{"hash":"00"}]])"), invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescanblocks__not_array__invalid_params)
{
    BC_REQUIRE_EQUAL(rpc_error("rescanblocks", R"(["x"])"), invalid_params.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescanblocks__not_hash__internal_error)
{
    const code internal_error{ server::error::btcd::internal_error };
    BC_REQUIRE_EQUAL(rpc_error("rescanblocks", R"([["not-a-hash"]])"), internal_error.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__fractional_verbose__invalid_params)
{
    const auto result = rpc_error("searchrawtransactions", std::format(R"(["{}",1.5])", found_address));
    BC_REQUIRE_EQUAL(result, invalid_params.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__numeric_filteraddr__invalid_params)
{
    const auto request = std::format(R"(["{}",1,0,100,0,false,[1]])", found_address);
    BC_REQUIRE_EQUAL(rpc_error("searchrawtransactions", request), invalid_params.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__reverse__found)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto response = rpc("searchrawtransactions", std::format(R"(["{}",1,0,100,0,true])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_array());
    BC_REQUIRE_EQUAL(response.at("result").as_array().size(), 1u);
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__count_below_history__truncated)
{
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    const auto& coinbase = *test::block1.transactions_ptr()->front();
    const auto& point = coinbase.outputs_ptr()->front()->script().ops().front().data();
    const auto all = rpc("searchrawtransactions", std::format(R"(["{}"])", encode_base16(point)));
    BC_REQUIRE_MESSAGE(all.is_object() && all.as_object().contains("result"), serialize(all));
    BC_REQUIRE_EQUAL(all.at("result").as_array().size(), 2u);

    const auto response = rpc("searchrawtransactions", std::format(R"(["{}",1,0,1])", encode_base16(point)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.front().at("txid")), encode_hash(coinbase.hash(false)));
}

BC_AUTO_TEST_CASE(btcd_rpc__searchrawtransactions__vinextra_addressable_prevout__prevout_address)
{
    BC_REQUIRE(query_.set(test::mock_block13, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block13.hash()), true));

    const auto& spending = *test::mock_block13.transactions_ptr()->at(2);
    const auto response = rpc("searchrawtransactions", std::format(R"(["{}",1,0,100,1])", found_address));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 2u);
    BC_REQUIRE_EQUAL(as_text(result.back().at("txid")), encode_hash(spending.hash(false)));

    const auto& prevout = result.back().at("vin").as_array().front().at("prevOut");
    BC_REQUIRE_EQUAL(prevout.at("addresses").as_array().size(), 1u);
    BC_REQUIRE_EQUAL(as_text(prevout.at("addresses").as_array().front()), found_address);
    BC_REQUIRE_EQUAL(prevout.at("value").as_double(), 0.00000009);
}

// cfilters
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__getcfilter__genesis__single_element_filter)
{
    const auto response = rpc("getcfilter", std::format(R"(["{}",0])", encode_hash(test::block0_hash)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));

    data_chunk filter{};
    BC_REQUIRE(decode_base16(filter, as_text(response.at("result"))));
    BC_REQUIRE_EQUAL(filter.size(), 4u);
    BC_REQUIRE_EQUAL(filter.front(), 0x01u);
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilterheader__genesis__chained_filter_hash)
{
    const auto body = rpc("getcfilter", std::format(R"(["{}",0])", encode_hash(test::block0_hash)));
    const auto response = rpc("getcfilterheader", std::format(R"(["{}",0])", encode_hash(test::block0_hash)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));

    data_chunk filter{};
    BC_REQUIRE(decode_base16(filter, as_text(body.at("result"))));

    const auto expected = bitcoin_hash(splice(bitcoin_hash(filter), null_hash));
    BC_REQUIRE_EQUAL(as_text(response.at("result")), encode_hash(expected));
}

BC_AUTO_TEST_CASE(btcd_rpc__getcfilterheader__malformed_hash__deserialization)
{
    BC_REQUIRE_EQUAL(rpc_error("getcfilterheader", R"(["not-a-hash",0])"), deserialization.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getheaders__malformed_hashstop__deserialization)
{
    BC_REQUIRE_EQUAL(rpc_error("getheaders", std::format(R"([["{}"],"not-a-hash"])", encode_hash(test::block5_hash))), deserialization.value());
}

// node
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__node__connect_temp__null_result)
{
    const auto response = rpc("node", R"(["connect","127.0.0.1:1","temp"])");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__node__connect_perm__null_result)
{
    const auto response = rpc("node", R"(["connect","127.0.0.1:1","perm"])");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_rpc__node__disconnect_empty_address__invalid_parameter)
{
    BC_REQUIRE_EQUAL(rpc_error("node", R"(["disconnect",""])"), invalid_parameter.value());
}

// post transport
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__getblockcount__http_post__nine)
{
    const auto response = http_rpc("getblockcount");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    BC_REQUIRE_EQUAL(response.at("result").as_int64(), 9);
}

// outpoint filters
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(btcd_rpc__loadtxfilter__undecodable_outpoint_hash__invalid_parameter)
{
    BC_REQUIRE_EQUAL(rpc_error("loadtxfilter", R"([true,[],[{"hash":"zz","index":0}]])"), invalid_parameter.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescanblocks__outpoint_spent__spending_transaction)
{
    const auto block10 = encode_hash(test::mock_block10.hash());
    const auto& paying = *test::mock_block10.transactions_ptr()->at(1);
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    constexpr auto request = R"([true,[],[{{"hash":"{}","index":0}}]])";
    REQUIRE_NO_THROW_TRUE(rpc("loadtxfilter", std::format(request, coinbase_txid(test::block1))).at("result").is_null());

    const auto response = rpc("rescanblocks", std::format(R"([["{}"]])", block10));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));

    const auto& result = response.at("result").as_array();
    BC_REQUIRE_EQUAL(result.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.front().at("hash")), block10);
    BC_REQUIRE_EQUAL(result.front().at("transactions").as_array().size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.front().at("transactions").as_array().front()), encode_base16(paying.to_data(true)));
}

BC_AUTO_TEST_CASE(btcd_rpc__filteredblockconnected__outpoint_spent__delivered)
{
    const auto& paying = *test::mock_block10.transactions_ptr()->at(1);
    rpc("notifyblocks");

    constexpr auto request = R"([true,[],[{{"hash":"{}","index":0}}]])";
    REQUIRE_NO_THROW_TRUE(rpc("loadtxfilter", std::format(request, coinbase_txid(test::block1))).at("result").is_null());

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    notify(node::chases::organized{ 10 });

    const auto blockconnected = receive_notification();
    BC_REQUIRE_EQUAL(as_text(blockconnected.at("method")), "blockconnected");

    const auto filtered = receive_notification();
    BC_REQUIRE_EQUAL(as_text(filtered.at("method")), "filteredblockconnected");

    const auto& txs = filtered.at("params").as_array()[2].as_array();
    BC_REQUIRE_EQUAL(txs.size(), 1u);
    BC_REQUIRE_EQUAL(as_text(txs.front()), encode_base16(paying.to_data(true)));
}

BC_AUTO_TEST_CASE(btcd_rpc__blockdisconnected__address_watches__delivered)
{
    rpc("notifyblocks");
    REQUIRE_NO_THROW_TRUE(rpc("loadtxfilter", std::format(R"([true,["{}"],[]])", found_address)).at("result").is_null());
    REQUIRE_NO_THROW_TRUE(rpc("notifyreceived", std::format(R"([["{}"]])", found_address)).at("result").is_null());

    notify(node::chases::reorganized{ 9 });

    const auto disconnected = receive_notification();
    BC_REQUIRE_EQUAL(as_text(disconnected.at("method")), "blockdisconnected");
    BC_REQUIRE_EQUAL(as_text(disconnected.at("params").as_array()[0]), block9);

    const auto filtered = receive_notification();
    BC_REQUIRE_EQUAL(as_text(filtered.at("method")), "filteredblockdisconnected");
}

BC_AUTO_TEST_CASE(btcd_rpc__blockdisconnected__not_subscribed__not_delivered)
{
    notify(node::chases::reorganized{ 9 });

    const auto response = rpc("session");
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_object());
}

BC_AUTO_TEST_CASE(btcd_rpc__blockconnected__dangling_link__not_delivered)
{
    rpc("notifyblocks");
    notify(node::chases::organized{ 42 });
    notify(node::chases::reorganized{ 9 });

    const auto disconnected = receive_notification();
    BC_REQUIRE_EQUAL(as_text(disconnected.at("method")), "blockdisconnected");
    BC_REQUIRE_EQUAL(as_text(disconnected.at("params").as_array()[0]), block9);
}

BC_AUTO_TEST_CASE(btcd_rpc__blockdisconnected__dangling_link__not_delivered)
{
    rpc("notifyblocks");
    notify(node::chases::reorganized{ 42 });
    notify(node::chases::reorganized{ 9 });

    const auto disconnected = receive_notification();
    BC_REQUIRE_EQUAL(as_text(disconnected.at("method")), "blockdisconnected");
    BC_REQUIRE_EQUAL(as_text(disconnected.at("params").as_array()[0]), block9);
}

BC_AUTO_TEST_SUITE_END()

// Filter limit (btcd.maximum_filters): loadtxfilter watches are bounded per
// channel (DoS guard).
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(btcd_limited_filter_tests, btcd_limited_filter_setup_fixture)

BC_AUTO_TEST_CASE(btcd_limited_filter__loadtxfilter__over_limit__subscription_limit)
{
    // The fixture allows one watch; the second address exceeds it.
    constexpr auto request = R"([true,["{}","{}"],[]])";
    const auto result = rpc_error("loadtxfilter", std::format(request, found_address, other_address));
    BC_REQUIRE_EQUAL(result, misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_limited_filter__loadtxfilter__outpoints_over_limit__misc_error)
{
    constexpr auto request = R"([true,[],[{{"hash":"{0}","index":0}},{{"hash":"{0}","index":1}}]])";
    BC_REQUIRE_EQUAL(rpc_error("loadtxfilter", std::format(request, coinbase_txid(test::block1))), misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_limited_filter__loadtxfilter__addresses_over_limit_with_outpoint__misc_error)
{
    constexpr auto request = R"([true,["{}","{}"],[{{"hash":"{}","index":0}}]])";
    BC_REQUIRE_EQUAL(rpc_error("loadtxfilter", std::format(request, found_address, other_address, coinbase_txid(test::block1))), misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_limited_filter__notifyreceived__over_limit__misc_error)
{
    BC_REQUIRE_EQUAL(rpc_error("notifyreceived", std::format(R"([["{}","{}"]])", found_address, other_address)), misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_limited_filter__notifyspent__over_limit__misc_error)
{
    constexpr auto request = R"([[{{"hash":"{0}","index":0}},{{"hash":"{0}","index":1}}]])";
    BC_REQUIRE_EQUAL(rpc_error("notifyspent", std::format(request, coinbase_txid(test::block1))), misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_limited_filter__recvtx__spent_watch_over_limit__dropped)
{
    REQUIRE_NO_THROW_TRUE(rpc("notifyreceived", std::format(R"([["{}"]])", found_address)).at("result").is_null());

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));

    notify(node::chases::organized{ 10 });
    REQUIRE_NO_THROW_TRUE(receive_notification(false).at("dropped").as_bool());
}

BC_AUTO_TEST_SUITE_END()

// Address index requirement: address watching and rescan are index queries,
// not_implemented when the index is disabled.
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(btcd_no_index_tests, btcd_no_index_setup_fixture)

BC_AUTO_TEST_CASE(btcd_no_index__loadtxfilter__address__not_implemented)
{
    constexpr auto request = R"([true,["{}"],[]])";
    const auto result = rpc_error("loadtxfilter", std::format(request, found_address));
    BC_REQUIRE_EQUAL(result, unimplemented.value());
}

BC_AUTO_TEST_CASE(btcd_no_index__loadtxfilter__outpoint_only__null_result)
{
    // Outpoint watching is spender lookup, not an address index query.
    constexpr auto request = R"([true,[],[{{"hash":"{}","index":0}}]])";
    const auto response = rpc("loadtxfilter", std::format(request, coinbase_txid(test::block1)));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_no_index__rescanblocks__any__not_implemented)
{
    constexpr auto request = R"([["{}"]])";
    const auto result = rpc_error("rescanblocks", std::format(request, encode_hash(test::block1_hash)));
    BC_REQUIRE_EQUAL(result, unimplemented.value());
}

BC_AUTO_TEST_CASE(btcd_no_index__searchrawtransactions__any__misc_error)
{
    const auto result = rpc_error("searchrawtransactions", std::format(R"(["{}"])", found_address));
    BC_REQUIRE_EQUAL(result, misc_error.value());
}

BC_AUTO_TEST_CASE(btcd_no_index__notifyreceived__address__not_implemented)
{
    const auto result = rpc_error("notifyreceived", std::format(R"([["{}"]])", found_address));
    BC_REQUIRE_EQUAL(result, unimplemented.value());
}

BC_AUTO_TEST_SUITE_END()

// Method-scoped credentials (channel_http::permitted()): a credential's
// optional 'user:pass:method,...' suffix restricts which methods it may
// call, enforced over ws by protocol_btcd::dispatch_websocket.
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(btcd_scoped_credential_tests, btcd_scoped_credential_setup_fixture)

BC_AUTO_TEST_CASE(btcd_scoped_credential__session__listed_method__permitted)
{
    BC_REQUIRE(authenticate());

    // BTCD_TEST_SCOPED_METHOD ("session") is the credential's only method.
    const auto session = rpc("session");
    REQUIRE_NO_THROW_TRUE(session.at("result").is_object());
}

BC_AUTO_TEST_CASE(btcd_scoped_credential__notifyblocks__unlisted_method__invalid_params)
{
    BC_REQUIRE(authenticate());

    // notifyblocks is implemented, so rejection is permitted()'s doing.
    BC_REQUIRE_EQUAL(rpc_error("notifyblocks"), invalid_params.value());
}

BC_AUTO_TEST_CASE(btcd_scoped_credential__notifyblocks__http_post_unlisted_method__invalid_params)
{
    const auto response = http_rpc("notifyblocks", "[]", BTCD_TEST_USERNAME, BTCD_TEST_PASSWORD);
    REQUIRE_NO_THROW_TRUE(response.at("error").is_object());
    BC_REQUIRE_EQUAL(response.at("error").at("code").as_int64(), invalid_params.value());
}

BC_AUTO_TEST_SUITE_END()

// authenticate (protocol_btcd::handle_authenticate): exercises the
// credential-configured branch, not covered by btcd_tests above (whose
// fixture leaves btcd.credential unset, so every one of those connections
// takes the no-op path).
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(btcd_auth_tests, btcd_credentialed_setup_fixture)

BC_AUTO_TEST_CASE(btcd_auth__authenticate__correct_credentials__null_result)
{
    constexpr auto request = R"(["{}","{}"])";
    const auto response = rpc("authenticate", std::format(request, BTCD_TEST_USERNAME, BTCD_TEST_PASSWORD));
    BC_REQUIRE_MESSAGE(response.is_object() && response.as_object().contains("result"), serialize(response));
    REQUIRE_NO_THROW_TRUE(response.at("result").is_null());
}

BC_AUTO_TEST_CASE(btcd_auth__session__authenticated__permitted)
{
    BC_REQUIRE(authenticate());

    // Connection survives, and other methods now work.
    const auto session = rpc("session");
    REQUIRE_NO_THROW_TRUE(session.at("result").is_object());
}

BC_AUTO_TEST_CASE(btcd_auth__authenticate__wrong_password__unauthorized)
{
    constexpr auto request = R"(["{}","definitely-wrong"])";
    const auto result = rpc_error("authenticate", std::format(request, BTCD_TEST_USERNAME));
    BC_REQUIRE_EQUAL(result, unauthorized.value());
}

BC_AUTO_TEST_CASE(btcd_auth__session__not_authenticated__unauthorized)
{
    // 'session' as the first message, with a credential configured: any
    // method but authenticate is invalid until authorization is established.
    BC_REQUIRE_EQUAL(rpc_error("session"), unauthorized.value());
}

BC_AUTO_TEST_CASE(btcd_auth__authenticate__already_authenticated__unauthorized)
{
    BC_REQUIRE(authenticate());

    // authenticate is invalid once authorization is established.
    constexpr auto request = R"(["{}","{}"])";
    const auto result = rpc_error("authenticate", std::format(request, BTCD_TEST_USERNAME, BTCD_TEST_PASSWORD));
    BC_REQUIRE_EQUAL(result, unauthorized.value());
}

BC_AUTO_TEST_SUITE_END()

// taproot active
// ----------------------------------------------------------------------------
// Taproot is flagged in the top header context.

BC_FIXTURE_TEST_SUITE(btcd_taproot_active_tests, btcd_taproot_active_setup_fixture)

BC_AUTO_TEST_CASE(btcd_rpc__getblockchaininfo__bip9_softforks_taproot__active__present)
{
    const auto response = rpc("getblockchaininfo");
    const auto& result = response.at("result");
    BC_REQUIRE(result.at("bip9_softforks").as_object().contains("taproot"));
    BC_REQUIRE_EQUAL(as_text(result.at("bip9_softforks").at("taproot").at("status")), "active");
    BC_REQUIRE_EQUAL(result.at("bip9_softforks").at("taproot").at("since").as_int64(), 9);
}

BC_AUTO_TEST_SUITE_END()

// dangling
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(btcd_dangling_tests, btcd_dangling_setup_fixture)

BC_AUTO_TEST_CASE(btcd_rpc__getblockchaininfo__dangling__internal_error)
{
    BC_REQUIRE_EQUAL(rpc_error("getblockchaininfo"), internal.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getdifficulty__dangling__internal_error)
{
    BC_REQUIRE_EQUAL(rpc_error("getdifficulty"), internal.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__getinfo__dangling__internal_error)
{
    BC_REQUIRE_EQUAL(rpc_error("getinfo"), internal.value());
}

BC_AUTO_TEST_CASE(btcd_rpc__rescan__dangling__internal_error)
{
    BC_REQUIRE_EQUAL(rpc_error("rescan", std::format(R"(["{}",[],[],""])", encode_hash(test::block1_hash))), internal.value());
}

BC_AUTO_TEST_SUITE_END()
