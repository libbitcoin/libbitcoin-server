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

static std::string as_text(const boost::json::value& value) NOEXCEPT
{
    return { value.as_string().c_str() };
}

namespace {

const auto block0 = encode_hash(test::block0_hash);
const auto block5 = encode_hash(test::block5_hash);
const auto block9 = encode_hash(test::block9_hash);
const auto header9 = encode_base16(test::header9_data);

std::string block_hash_hex(const data_chunk& wire) NOEXCEPT
{
    return encode_hash(bitcoin_hash(chain::header::serialized_size(),
        wire.data()));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(bitcoind_rest_tests, bitcoind_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rest__chaininfo_json__main_nine)
{
    const auto result = rest_json("/rest/chaininfo.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BOOST_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bestblockhash")), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo_json__top__height_nine)
{
    const auto result = rest_json("/rest/deploymentinfo.json");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE(result.at("deployments").is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo_json__block5__height_five)
{
    const auto result = rest_json("/rest/deploymentinfo/" + block5 + ".json");
    BOOST_REQUIRE_EQUAL(result.at("height").as_int64(), 5);
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block5);
}

// bitcoind reports an unknown deploymentinfo block as a bad request.
BOOST_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo_unknown__bad_request)
{
    const std::string unknown(64, '1');
    const auto result = rest_status("/rest/deploymentinfo/" + unknown + ".json");
    BOOST_REQUIRE(result == bitcoind_setup_fixture::status::bad_request);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_json__block9_with_txs)
{
    const auto result = rest_json("/rest/block/" + block9 + ".json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE(result.at("tx").at(0).is_object());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__tx_json__coinbase_txid)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto result = rest_json("/rest/tx/" + txid + ".json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("txid")), txid);
    BOOST_REQUIRE(result.at("vin").is_array());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__tx_unknown__not_found)
{
    const auto txid = encode_hash(null_hash);
    BOOST_REQUIRE_EQUAL(rest_status("/rest/tx/" + txid + ".json"), status::not_found);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__malformed_hash__bad_request)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/block/nothex.json"), status::bad_request);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__malformed_height__bad_request)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockhashbyheight/abc.json"), status::bad_request);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__unknown_target__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/bogus"), status::not_found);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_hex__hashes_to_block9)
{
    const auto hex = rest_text("/rest/block/" + block9 + ".hex");
    data_chunk wire{};
    BOOST_REQUIRE(decode_base16(wire, hex));
    BOOST_REQUIRE_EQUAL(block_hash_hex(wire), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_bin__hashes_to_block9)
{
    const auto wire = rest_data("/rest/block/" + block9 + ".bin");
    BOOST_REQUIRE_EQUAL(block_hash_hex(wire), block9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_json__txid_list)
{
    const auto result = rest_json("/rest/block/notxdetails/" + block9 + ".json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BOOST_REQUIRE(result.at("tx").is_array());
    BOOST_REQUIRE(result.at("tx").at(0).is_string());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__getutxos_json__block1_coinbase__hit)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto result = rest_json("/rest/getutxos/" + txid + "-0.json");
    BOOST_REQUIRE_EQUAL(result.at("chainHeight").as_int64(), 9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("chaintipHash")), block9);
    BOOST_REQUIRE_EQUAL(as_text(result.at("bitmap")), "1");
    BOOST_REQUIRE_EQUAL(result.at("utxos").as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(result.at("utxos").at(0).at("height").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__getutxos_json__checkmempool_miss__empty)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto result = rest_json("/rest/getutxos/checkmempool/" + txid + "-1.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("bitmap")), "0");
    BOOST_REQUIRE(result.at("utxos").as_array().empty());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__getutxos_bin__miss__bip64_framing)
{
    const std::string unknown(64, '1');
    const auto wire = rest_data("/rest/getutxos/" + unknown + "-0.bin");
    BOOST_REQUIRE_EQUAL(wire.size(), 39u);
    BOOST_REQUIRE_EQUAL(wire.at(0), 9u);
}

// A coinbase-only block undo is one empty per-tx prevout list.
BOOST_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_json__block9__coinbase_only)
{
    const auto result = rest_json("/rest/spenttxouts/" + block9 + ".json");
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 1u);
    BOOST_REQUIRE(result.at(0).as_array().empty());
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_bin__block9__undo_framing)
{
    const auto wire = rest_data("/rest/spenttxouts/" + block9 + ".bin");
    BOOST_REQUIRE_EQUAL(encode_base16(wire), "0100");
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__height_five__block5)
{
    const auto result = rest_json("/rest/blockhashbyheight/5.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__genesis__block0)
{
    const auto result = rest_json("/rest/blockhashbyheight/0.json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("blockhash")), block0);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_json__count_three_from_block5)
{
    const auto result = rest_json("/rest/headers/3/" + block5 + ".json");
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 3u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_json__query_count_two_from_block5)
{
    const auto result = rest_json("/rest/headers/" + block5 + ".json?count=2");
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 2u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_json__no_query__default_count_five)
{
    const auto result = rest_json("/rest/headers/" + block5 + ".json");
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 5u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_hex__one_header__eighty_bytes)
{
    const auto hex = rest_text("/rest/headers/1/" + block9 + ".hex");
    data_chunk wire{};
    BOOST_REQUIRE(decode_base16(wire, hex));
    BOOST_REQUIRE_EQUAL(wire.size(), 80u);
    BOOST_REQUIRE_EQUAL(encode_base16(wire), header9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_bin__block9_header)
{
    const auto target = "/rest/blockpart/" + block9 + ".bin?offset=0&size=80";
    const auto wire = rest_data(target);
    BOOST_REQUIRE_EQUAL(wire.size(), 80u);
    BOOST_REQUIRE_EQUAL(encode_base16(wire), header9);
}

// bitcoind reports missing part parameters as bad requests.
BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_no_query__bad_request)
{
    const auto result = rest_status("/rest/blockpart/" + block9 + ".bin");
    BOOST_REQUIRE(result == bitcoind_setup_fixture::status::bad_request);
}

// bitcoind reports an out of range part as a bad request.
BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_excess__bad_request)
{
    const auto target = "/rest/blockpart/" + block9 + ".bin?offset=0&size=1000000";
    BOOST_REQUIRE(rest_status(target) == bitcoind_setup_fixture::status::bad_request);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilter_basic__filters_disabled__not_ok)
{
    const auto target = "/rest/blockfilter/basic/" + block9 + ".json";
    BOOST_REQUIRE(rest_status(target) != boost::beast::http::status::ok);
}

static const auto& coinbase1 = *test::block1.transactions_ptr()->front();
static const auto txid1 = encode_hash(coinbase1.hash(false));
static const auto unknown_hash = encode_hash(null_hash);

// tx
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__tx_hex__coinbase__expected)
{
    BOOST_REQUIRE_EQUAL(rest_text("/rest/tx/" + txid1 + ".hex"), encode_base16(coinbase1.to_data(true)));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__tx_bin__coinbase__expected)
{
    BOOST_REQUIRE_EQUAL(rest_data("/rest/tx/" + txid1 + ".bin"), coinbase1.to_data(true));
}

// block
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/block/" + unknown_hash + ".json"), status::not_found);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_bin__block9__expected)
{
    BOOST_REQUIRE_EQUAL(rest_data("/rest/block/notxdetails/" + block9 + ".bin"), to_chunk(test::block9_data));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_hex__block9__expected)
{
    BOOST_REQUIRE_EQUAL(rest_text("/rest/block/notxdetails/" + block9 + ".hex"), encode_base16(test::block9_data));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/block/notxdetails/" + unknown_hash + ".json"), status::not_found);
}

// blockhashbyheight
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_bin__height_five__block5)
{
    BOOST_REQUIRE_EQUAL(rest_data("/rest/blockhashbyheight/5.bin"), to_chunk(test::block5_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_hex__height_five__block5)
{
    BOOST_REQUIRE_EQUAL(rest_text("/rest/blockhashbyheight/5.hex"), encode_base16(test::block5_hash));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_above_top__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockhashbyheight/10.json"), status::not_found);
}

// headers
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_bin__count_two_from_block5__expected)
{
    const auto expected = splice(test::header5_data, test::header6_data);
    BOOST_REQUIRE_EQUAL(rest_data("/rest/headers/2/" + block5 + ".bin"), to_chunk(expected));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_count_zero__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/headers/0/" + block5 + ".json"), status::not_found);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/headers/1/" + unknown_hash + ".json"), status::not_found);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__headers_invalid_query_count__bad_request)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/headers/" + block5 + ".json?count=abc"), status::bad_request);
}

// blockpart
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_hex__block9_header)
{
    BOOST_REQUIRE_EQUAL(rest_text("/rest/blockpart/" + block9 + ".hex?offset=0&size=80"), header9);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_json__bad_request)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockpart/" + block9 + ".json?offset=0&size=80"), status::bad_request);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockpart_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockpart/" + unknown_hash + ".bin?offset=0&size=80"), status::not_found);
}

// spenttxouts
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_hex__block9__undo_framing)
{
    BOOST_REQUIRE_EQUAL(rest_text("/rest/spenttxouts/" + block9 + ".hex"), "0100");
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/spenttxouts/" + unknown_hash + ".bin"), status::not_found);
}

// blockfilter
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilter_hex__genesis__expected)
{
    const auto filter = rest_data("/rest/blockfilter/basic/" + block0 + ".bin");
    BOOST_REQUIRE(!filter.empty());
    BOOST_REQUIRE_EQUAL(rest_text("/rest/blockfilter/basic/" + block0 + ".hex"), encode_base16(filter));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilter_json__genesis__expected)
{
    const auto filter = rest_data("/rest/blockfilter/basic/" + block0 + ".bin");
    const auto result = rest_json("/rest/blockfilter/basic/" + block0 + ".json");
    BOOST_REQUIRE_EQUAL(as_text(result.at("filter")), encode_base16(filter));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilter_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockfilter/basic/" + unknown_hash + ".bin"), status::not_found);
}

// blockfilterheaders
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_bin__genesis__chains_from_filter)
{
    const auto filter = rest_data("/rest/blockfilter/basic/" + block0 + ".bin");
    const auto expected = bitcoin_hash(splice(bitcoin_hash(filter), null_hash));
    BOOST_REQUIRE_EQUAL(rest_data("/rest/blockfilterheaders/basic/1/" + block0 + ".bin"), to_chunk(expected));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_hex__genesis__expected)
{
    const auto head = rest_data("/rest/blockfilterheaders/basic/1/" + block0 + ".bin");
    BOOST_REQUIRE_EQUAL(rest_text("/rest/blockfilterheaders/basic/1/" + block0 + ".hex"), encode_base16(head));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_json__genesis__expected)
{
    const auto filter = rest_data("/rest/blockfilter/basic/" + block0 + ".bin");
    const auto expected = bitcoin_hash(splice(bitcoin_hash(filter), null_hash));
    const auto result = rest_json("/rest/blockfilterheaders/basic/1/" + block0 + ".json");
    BOOST_REQUIRE(result.is_array());
    BOOST_REQUIRE_EQUAL(result.as_array().size(), 1u);
    BOOST_REQUIRE_EQUAL(as_text(result.at(0)), encode_hash(expected));
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_unknown__not_found)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockfilterheaders/basic/1/" + unknown_hash + ".bin"), status::not_found);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_missing_filter_head__internal_server_error)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/blockfilterheaders/basic/2/" + block0 + ".bin"), status::internal_server_error);
}

// getutxos
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(bitcoind_rest__getutxos_hex__miss__bip64_framing)
{
    const auto expected = "09000000" + encode_base16(test::block9_hash) + "01" "00" "00";
    BOOST_REQUIRE_EQUAL(rest_text("/rest/getutxos/" + unknown_hash + "-0.hex"), expected);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__getutxos_bin__hit__bip64_framing)
{
    const auto prefix = base16_chunk("09000000");
    const auto bitmap = base16_chunk("0101" "01");
    const auto utxo = base16_chunk("00000000" "01000000");
    const auto output = coinbase1.outputs_ptr()->front()->to_data();
    const auto expected = build_chunk({ prefix, test::block9_hash, bitmap, utxo, output });
    BOOST_REQUIRE_EQUAL(rest_data("/rest/getutxos/" + txid1 + "-0.bin"), expected);
}

BOOST_AUTO_TEST_CASE(bitcoind_rest__getutxos_over_limit__bad_request)
{
    const auto item = unknown_hash + "-0/";
    const auto items = item + item + item + item + item + item + item + item + item + item + item + item + item + item + item + item;
    BOOST_REQUIRE_EQUAL(rest_status("/rest/getutxos/" + items + unknown_hash + "-1.json"), status::bad_request);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(bitcoind_rest_host_tests, bitcoind_hosted_setup_fixture)

BOOST_AUTO_TEST_CASE(bitcoind_rest__disallowed_host__bad_request)
{
    BOOST_REQUIRE_EQUAL(rest_status("/rest/chaininfo.json"), status::bad_request);
}

BOOST_AUTO_TEST_SUITE_END()
