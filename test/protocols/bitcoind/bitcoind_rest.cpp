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

static std::string as_text(const system::json::value& value) NOEXCEPT
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

BC_FIXTURE_TEST_SUITE(bitcoind_rest_tests, bitcoind_ten_block_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__chaininfo_json__main_nine)
{
    const auto result = rest_json("/rest/chaininfo.json");
    BC_REQUIRE_EQUAL(as_text(result.at("chain")), "main");
    BC_REQUIRE_EQUAL(result.at("blocks").as_int64(), 9);
    BC_REQUIRE_EQUAL(as_text(result.at("bestblockhash")), block9);
}

BC_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo_json__top__height_nine)
{
    const auto result = rest_json("/rest/deploymentinfo.json");
    BC_REQUIRE_EQUAL(result.at("height").as_int64(), 9);
    BC_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BC_REQUIRE(result.at("deployments").is_object());
}

BC_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo_json__block5__height_five)
{
    const auto result = rest_json("/rest/deploymentinfo/" + block5 + ".json");
    BC_REQUIRE_EQUAL(result.at("height").as_int64(), 5);
    BC_REQUIRE_EQUAL(as_text(result.at("hash")), block5);
}

// bitcoind reports an unknown deploymentinfo block as a bad request.
BC_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo_unknown__bad_request)
{
    const std::string unknown(64, '1');
    const auto result = rest_status("/rest/deploymentinfo/" + unknown + ".json");
    BC_REQUIRE(result == bitcoind_setup_fixture::status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_json__block9_with_txs)
{
    const auto result = rest_json("/rest/block/" + block9 + ".json");
    BC_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BC_REQUIRE(result.at("tx").is_array());
    BC_REQUIRE(result.at("tx").at(0).is_object());
}

BC_AUTO_TEST_CASE(bitcoind_rest__tx_json__coinbase_txid)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto result = rest_json("/rest/tx/" + txid + ".json");
    BC_REQUIRE_EQUAL(as_text(result.at("txid")), txid);
    BC_REQUIRE(result.at("vin").is_array());
}

BC_AUTO_TEST_CASE(bitcoind_rest__tx_unknown__not_found)
{
    const auto txid = encode_hash(null_hash);
    BC_REQUIRE_EQUAL(rest_status("/rest/tx/" + txid + ".json"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__malformed_hash__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/block/nothex.json"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__malformed_height__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockhashbyheight/abc.json"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__unknown_target__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/bogus"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_hex__hashes_to_block9)
{
    const auto text = rest_text("/rest/block/" + block9 + ".hex");
    const auto hex = text.substr(zero, sub1(text.size()));
    data_chunk wire{};

    BC_REQUIRE(text.ends_with('\n'));
    BC_REQUIRE(decode_base16(wire, hex));
    BC_REQUIRE_EQUAL(block_hash_hex(wire), block9);
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_bin__hashes_to_block9)
{
    const auto wire = rest_data("/rest/block/" + block9 + ".bin");
    BC_REQUIRE_EQUAL(block_hash_hex(wire), block9);
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_json__txid_list)
{
    const auto result = rest_json("/rest/block/notxdetails/" + block9 + ".json");
    BC_REQUIRE_EQUAL(as_text(result.at("hash")), block9);
    BC_REQUIRE(result.at("tx").is_array());
    BC_REQUIRE(result.at("tx").at(0).is_string());
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos_json__block1_coinbase__hit)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto result = rest_json("/rest/getutxos/" + txid + "-0.json");
    BC_REQUIRE_EQUAL(result.at("chainHeight").as_int64(), 9);
    BC_REQUIRE_EQUAL(as_text(result.at("chaintipHash")), block9);
    BC_REQUIRE_EQUAL(as_text(result.at("bitmap")), "1");
    BC_REQUIRE_EQUAL(result.at("utxos").as_array().size(), 1u);
    BC_REQUIRE_EQUAL(result.at("utxos").at(0).at("height").as_int64(), 1);
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos_json__checkmempool_miss__empty)
{
    const auto txid = encode_hash(test::block1.transactions_ptr()->front()->hash(false));
    const auto result = rest_json("/rest/getutxos/checkmempool/" + txid + "-1.json");
    BC_REQUIRE_EQUAL(as_text(result.at("bitmap")), "0");
    BC_REQUIRE(result.at("utxos").as_array().empty());
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos_bin__miss__bip64_framing)
{
    const std::string unknown(64, '1');
    const auto wire = rest_data("/rest/getutxos/" + unknown + "-0.bin");
    BC_REQUIRE_EQUAL(wire.size(), 39u);
    BC_REQUIRE_EQUAL(wire.at(0), 9u);
}

// A coinbase-only block undo is one empty per-tx prevout list.
BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_json__block9__coinbase_only)
{
    const auto result = rest_json("/rest/spenttxouts/" + block9 + ".json");
    BC_REQUIRE(result.is_array());
    BC_REQUIRE_EQUAL(result.as_array().size(), 1u);
    BC_REQUIRE(result.at(0).as_array().empty());
}

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_bin__block9__undo_framing)
{
    const auto wire = rest_data("/rest/spenttxouts/" + block9 + ".bin");
    BC_REQUIRE_EQUAL(encode_base16(wire), "0100");
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__height_five__block5)
{
    const auto result = rest_json("/rest/blockhashbyheight/5.json");
    BC_REQUIRE_EQUAL(as_text(result.at("blockhash")), block5);
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_json__genesis__block0)
{
    const auto result = rest_json("/rest/blockhashbyheight/0.json");
    BC_REQUIRE_EQUAL(as_text(result.at("blockhash")), block0);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_json__count_three_from_block5)
{
    const auto result = rest_json("/rest/headers/3/" + block5 + ".json");
    BC_REQUIRE(result.is_array());
    BC_REQUIRE_EQUAL(result.as_array().size(), 3u);
    BC_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_json__query_count_two_from_block5)
{
    const auto result = rest_json("/rest/headers/" + block5 + ".json?count=2");
    BC_REQUIRE(result.is_array());
    BC_REQUIRE_EQUAL(result.as_array().size(), 2u);
    BC_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_json__no_query__default_count_five)
{
    const auto result = rest_json("/rest/headers/" + block5 + ".json");
    BC_REQUIRE(result.is_array());
    BC_REQUIRE_EQUAL(result.as_array().size(), 5u);
    BC_REQUIRE_EQUAL(as_text(result.at(0).at("hash")), block5);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_hex__one_header__eighty_bytes)
{
    const auto text = rest_text("/rest/headers/1/" + block9 + ".hex");
    const auto hex = text.substr(zero, sub1(text.size()));
    data_chunk wire{};

    BC_REQUIRE(text.ends_with('\n'));
    BC_REQUIRE(decode_base16(wire, hex));
    BC_REQUIRE_EQUAL(wire.size(), 80u);
    BC_REQUIRE_EQUAL(encode_base16(wire), header9);
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockpart_bin__block9_header)
{
    const auto target = "/rest/blockpart/" + block9 + ".bin?offset=0&size=80";
    const auto wire = rest_data(target);
    BC_REQUIRE_EQUAL(wire.size(), 80u);
    BC_REQUIRE_EQUAL(encode_base16(wire), header9);
}

// bitcoind reports missing part parameters as bad requests.
BC_AUTO_TEST_CASE(bitcoind_rest__blockpart_no_query__bad_request)
{
    const auto result = rest_status("/rest/blockpart/" + block9 + ".bin");
    BC_REQUIRE(result == bitcoind_setup_fixture::status::bad_request);
}

// bitcoind reports an out of range part as a bad request.
BC_AUTO_TEST_CASE(bitcoind_rest__blockpart_excess__bad_request)
{
    const auto target = "/rest/blockpart/" + block9 + ".bin?offset=0&size=1000000";
    BC_REQUIRE(rest_status(target) == bitcoind_setup_fixture::status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilter_basic__filters_disabled__not_ok)
{
    const auto target = "/rest/blockfilter/basic/" + block9 + ".json";
    BC_REQUIRE(rest_status(target) != network::http::status::ok);
}

static const chain::transaction& coinbase1() NOEXCEPT
{
    return *test::block1.transactions_ptr()->front();
}

static std::string txid1() NOEXCEPT
{
    return encode_hash(coinbase1().hash(false));
}

static const auto unknown_hash = encode_hash(null_hash);

// tx
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__tx_hex__coinbase__expected)
{
    BC_REQUIRE_EQUAL(rest_text("/rest/tx/" + txid1() + ".hex"), encode_base16(coinbase1().to_data(true)) + "\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__tx_bin__coinbase__expected)
{
    BC_REQUIRE_EQUAL(rest_data("/rest/tx/" + txid1() + ".bin"), coinbase1().to_data(true));
}

// block
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__block_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/block/" + unknown_hash + ".json"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_bin__block9__expected)
{
    BC_REQUIRE_EQUAL(rest_data("/rest/block/notxdetails/" + block9 + ".bin"), to_chunk(test::block9_data));
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_hex__block9__expected)
{
    BC_REQUIRE_EQUAL(rest_text("/rest/block/notxdetails/" + block9 + ".hex"), encode_base16(test::block9_data) + "\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__block_notxdetails_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/block/notxdetails/" + unknown_hash + ".json"), status::not_found);
}

// blockhashbyheight
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_bin__height_five__block5)
{
    BC_REQUIRE_EQUAL(rest_data("/rest/blockhashbyheight/5.bin"), to_chunk(test::block5_hash));
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_hex__height_five__block5)
{
    BC_REQUIRE_EQUAL(rest_text("/rest/blockhashbyheight/5.hex"), encode_base16(test::block5_hash) + "\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockhashbyheight_above_top__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockhashbyheight/10.json"), status::not_found);
}

// headers
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__headers_bin__count_two_from_block5__expected)
{
    const auto expected = splice(test::header5_data, test::header6_data);
    BC_REQUIRE_EQUAL(rest_data("/rest/headers/2/" + block5 + ".bin"), to_chunk(expected));
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_count_zero__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/headers/0/" + block5 + ".json"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/headers/1/" + unknown_hash + ".json"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_invalid_query_count__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/headers/" + block5 + ".json?count=abc"), status::bad_request);
}

// blockpart
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__blockpart_hex__block9_header)
{
    BC_REQUIRE_EQUAL(rest_text("/rest/blockpart/" + block9 + ".hex?offset=0&size=80"), header9 + "\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockpart_json__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockpart/" + block9 + ".json?offset=0&size=80"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockpart_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockpart/" + unknown_hash + ".bin?offset=0&size=80"), status::not_found);
}

// spenttxouts
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_hex__block9__undo_framing)
{
    BC_REQUIRE_EQUAL(rest_text("/rest/spenttxouts/" + block9 + ".hex"), "0100\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/spenttxouts/" + unknown_hash + ".bin"), status::not_found);
}

// blockfilter
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilter_hex__genesis__expected)
{
    const auto filter = rest_data("/rest/blockfilter/basic/" + block0 + ".bin");
    BC_REQUIRE(!filter.empty());
    BC_REQUIRE_EQUAL(rest_text("/rest/blockfilter/basic/" + block0 + ".hex"), encode_base16(filter) + "\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilter_bin__genesis__type_hash_filter)
{
    const auto framed = rest_data("/rest/blockfilter/basic/" + block0 + ".bin");
    const auto result = rest_json("/rest/blockfilter/basic/" + block0 + ".json");
    data_chunk filter{};

    BC_REQUIRE(decode_base16(filter, as_text(result.at("filter"))));
    BC_REQUIRE_EQUAL(framed, build_chunk({ data_chunk{ 0x00 }, test::block0_hash, data_chunk{ possible_narrow_cast<uint8_t>(filter.size()) }, filter }));
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilter_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockfilter/basic/" + unknown_hash + ".bin"), status::not_found);
}

// blockfilterheaders
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_bin__genesis__chains_from_filter)
{
    const auto result = rest_json("/rest/blockfilter/basic/" + block0 + ".json");
    data_chunk filter{};

    BC_REQUIRE(decode_base16(filter, as_text(result.at("filter"))));
    BC_REQUIRE_EQUAL(rest_data("/rest/blockfilterheaders/basic/1/" + block0 + ".bin"), to_chunk(bitcoin_hash(splice(bitcoin_hash(filter), null_hash))));
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_hex__genesis__expected)
{
    const auto head = rest_data("/rest/blockfilterheaders/basic/1/" + block0 + ".bin");
    BC_REQUIRE_EQUAL(rest_text("/rest/blockfilterheaders/basic/1/" + block0 + ".hex"), encode_base16(head) + "\n");
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_json__genesis__expected)
{
    const auto filters = rest_json("/rest/blockfilter/basic/" + block0 + ".json");
    data_chunk filter{};

    BC_REQUIRE(decode_base16(filter, as_text(filters.at("filter"))));

    const auto expected = bitcoin_hash(splice(bitcoin_hash(filter), null_hash));
    const auto result = rest_json("/rest/blockfilterheaders/basic/1/" + block0 + ".json");
    BC_REQUIRE(result.is_array());
    BC_REQUIRE_EQUAL(result.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.at(0)), encode_hash(expected));
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockfilterheaders/basic/1/" + unknown_hash + ".bin"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__blockfilterheaders_missing_filter_head__internal_server_error)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/blockfilterheaders/basic/2/" + block0 + ".bin"), status::internal_server_error);
}

// getutxos
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos_hex__miss__bip64_framing)
{
    const auto expected = "09000000" + encode_base16(test::block9_hash) + "01" "00" "00" "\n";
    BC_REQUIRE_EQUAL(rest_text("/rest/getutxos/" + unknown_hash + "-0.hex"), expected);
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos_bin__hit__bip64_framing)
{
    const auto prefix = base16_chunk("09000000");
    const auto bitmap = base16_chunk("0101" "01");
    const auto utxo = base16_chunk("00000000" "01000000");
    const auto output = coinbase1().outputs_ptr()->front()->to_data();
    const auto expected = build_chunk({ prefix, test::block9_hash, bitmap, utxo, output });
    BC_REQUIRE_EQUAL(rest_data("/rest/getutxos/" + txid1() + "-0.bin"), expected);
}

// mempool

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_info__empty__zero_size)
{
    const auto result = rest_json("/rest/mempool/info.json");
    BC_REQUIRE(result.at("loaded").as_bool());
    BC_REQUIRE_EQUAL(result.at("size").as_int64(), 0);
    BC_REQUIRE_EQUAL(result.at("bytes").as_int64(), 0);
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__default__empty_object)
{
    const auto result = rest_json("/rest/mempool/contents.json");
    BC_REQUIRE(result.is_object());
    BC_REQUIRE(result.as_object().empty());
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__not_verbose__empty_array)
{
    const auto result = rest_json("/rest/mempool/contents.json?verbose=false");
    BC_REQUIRE(result.is_array());
    BC_REQUIRE(result.as_array().empty());
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__verbose_sequence__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/mempool/contents.json?verbose=true&mempool_sequence=true"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__invalid_boolean__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/mempool/contents.json?verbose=1"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__binary__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/mempool/contents.bin"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool__unknown__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/mempool/bogus.json"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos_over_limit__bad_request)
{
    const auto item = unknown_hash + "-0/";
    const auto items = item + item + item + item + item + item + item + item + item + item + item + item + item + item + item + item;
    BC_REQUIRE_EQUAL(rest_status("/rest/getutxos/" + items + unknown_hash + "-1.json"), status::bad_request);
}

// options and post
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(bitcoind_rest__options__allowed__ok)
{
    BC_REQUIRE_EQUAL(options_status("/"), status::ok);
}

// spenttxouts (spending block)
// ----------------------------------------------------------------------------

static const chain::output& spent3() NOEXCEPT
{
    return *test::block3.transactions_ptr()->front()->outputs_ptr()->front();
}

static const chain::output& spent13() NOEXCEPT
{
    return *test::mock_tx13.outputs_ptr()->front();
}

static std::string block13() NOEXCEPT
{
    return encode_hash(test::mock_block13.hash());
}

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_bin__block13__undo_framing)
{
    BC_REQUIRE(query_.set(test::mock_block13, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block13.hash()), true));

    const auto expected = build_chunk({ base16_chunk("030001"), spent3().to_data(), base16_chunk("01"), spent13().to_data() });
    BC_REQUIRE_EQUAL(rest_data("/rest/spenttxouts/" + block13() + ".bin"), expected);
}

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_hex__block13__undo_framing)
{
    BC_REQUIRE(query_.set(test::mock_block13, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block13.hash()), true));

    const auto expected = "030001" + encode_base16(spent3().to_data()) + "01" + encode_base16(spent13().to_data()) + "\n";
    BC_REQUIRE_EQUAL(rest_text("/rest/spenttxouts/" + block13() + ".hex"), expected);
}

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts_json__block13__prevouts_per_tx)
{
    BC_REQUIRE(query_.set(test::mock_block13, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block13.hash()), true));

    const auto result = rest_json("/rest/spenttxouts/" + block13() + ".json");
    BC_REQUIRE_EQUAL(result.as_array().size(), 3u);
    BC_REQUIRE(result.at(0).as_array().empty());
    BC_REQUIRE_EQUAL(result.at(1).as_array().size(), 1u);
    BC_REQUIRE_EQUAL(result.at(2).as_array().size(), 1u);
    BC_REQUIRE_EQUAL(result.at(1).at(0).at("value").to_number<double>(), 50.0);
    BC_REQUIRE_EQUAL(as_text(result.at(1).at(0).at("scriptPubKey").at("hex")), encode_base16(spent3().script().to_data(false)));
    BC_REQUIRE_EQUAL(result.at(2).at(0).at("value").to_number<double>(), 9 / 100'000'000.0);
    BC_REQUIRE_EQUAL(as_text(result.at(2).at(0).at("scriptPubKey").at("hex")), encode_base16(spent13().script().to_data(false)));
    BC_REQUIRE_EQUAL(as_text(result.at(2).at(0).at("scriptPubKey").at("address")), "1BaMPFdqMUQ46BV8iRcwbVfsam57oBLMM");
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(bitcoind_rest_host_tests, bitcoind_hosted_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__disallowed_host__bad_request)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/chaininfo.json"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__options_disallowed_host__bad_request)
{
    BC_REQUIRE_EQUAL(options_status("/"), status::bad_request);
}

BC_AUTO_TEST_CASE(bitcoind_rest__post_disallowed_host__bad_request)
{
    BC_REQUIRE_EQUAL(rpc_body_status(R"({"jsonrpc":"2.0","id":1,"method":"getblockcount","params":[]})"), status::bad_request);
}

BC_AUTO_TEST_SUITE_END()

struct bitcoind_origin_setup_fixture
  : bitcoind_setup_fixture
{
    inline bitcoind_origin_setup_fixture()
      : bitcoind_setup_fixture([](test::query_t& query)
        {
            return test::setup_ten_block_store(query);
        }, [](configuration& config)
        {
            config.server.bitcoind.origins = { { "example.com" } };
            config.server.bitcoind.connections = 2;
        })
    {
    }

    status origin_status(std::string_view target, std::string_view origin)
    {
        namespace http = network::http;
        rpc_client client{ io_ };
        client.connect(binding());

        auto request = rpc_client::create_get(target);
        request.set(http::field::origin, origin);
        test::write_http(client.stream(), request);

        http::flat_buffer buffer{};
        http::message<false, http::string_body> response{};
        network::code ec{};
        http::read(client.stream(), buffer, response, ec);
        BC_REQUIRE(!ec);
        return response.result();
    }
};

BC_FIXTURE_TEST_SUITE(bitcoind_rest_origin_tests, bitcoind_origin_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__disallowed_origin__forbidden)
{
    BC_REQUIRE_EQUAL(origin_status("/rest/chaininfo.json", "http://example.org"), status::forbidden);
}

BC_AUTO_TEST_CASE(bitcoind_rest__opaque_origin__forbidden)
{
    BC_REQUIRE_EQUAL(origin_status("/rest/chaininfo.json", "null"), status::forbidden);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(bitcoind_rest_dangling_tests, bitcoind_dangling_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__chaininfo__dangling__not_found)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/chaininfo.json"), status::not_found);
}

BC_AUTO_TEST_CASE(bitcoind_rest__deploymentinfo__dangling__internal_server_error)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/deploymentinfo.json"), status::internal_server_error);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_bin__dangling__internal_server_error)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/headers/2/" + encode_hash(test::block2_hash) + ".bin"), status::internal_server_error);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_hex__dangling__internal_server_error)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/headers/2/" + encode_hash(test::block2_hash) + ".hex"), status::internal_server_error);
}

BC_AUTO_TEST_CASE(bitcoind_rest__headers_json__dangling__internal_server_error)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/headers/2/" + encode_hash(test::block2_hash) + ".json"), status::internal_server_error);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(bitcoind_rest_witness_tests, bitcoind_witness_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__spenttxouts__missing_prevouts__internal_server_error)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/spenttxouts/" + encode_hash(test::block2a.hash()) + ".bin"), status::internal_server_error);
}

BC_AUTO_TEST_SUITE_END()

// broadcast mempool
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(bitcoind_rest_broadcast_tests, bitcoind_broadcast_setup_fixture)

static std::string broadcast_param() NOEXCEPT
{
    return "[\"" + encode_base16(test::tx1c.to_data(true)) + "\"]";
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__broadcast__txid)
{
    const auto sent = rpc("sendrawtransaction", broadcast_param());
    BC_REQUIRE_MESSAGE(sent.is_object() && sent.as_object().contains("result"), serialize(sent));

    const auto result = rest_json("/rest/mempool/contents.json?verbose=false");
    BC_REQUIRE_EQUAL(result.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(as_text(result.as_array().front()), encode_hash(test::tx1c.hash(false)));
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__broadcast_verbose__entry)
{
    const auto sent = rpc("sendrawtransaction", broadcast_param());
    BC_REQUIRE_MESSAGE(sent.is_object() && sent.as_object().contains("result"), serialize(sent));

    const auto result = rest_json("/rest/mempool/contents.json");
    const auto& entry = result.as_object().at(encode_hash(test::tx1c.hash(false)));
    BC_REQUIRE_EQUAL(entry.at("vsize").as_int64(), to_signed(test::tx1c.virtual_size()));
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_contents__broadcast_sequence__txids_with_sequence)
{
    const auto sent = rpc("sendrawtransaction", broadcast_param());
    BC_REQUIRE_MESSAGE(sent.is_object() && sent.as_object().contains("result"), serialize(sent));

    const auto result = rest_json("/rest/mempool/contents.json?verbose=false&mempool_sequence=true");
    BC_REQUIRE_EQUAL(result.at("txids").as_array().size(), 1u);
    BC_REQUIRE_EQUAL(result.at("mempool_sequence").as_int64(), 1);
}

BC_AUTO_TEST_CASE(bitcoind_rest__mempool_info__broadcast__one)
{
    const auto sent = rpc("sendrawtransaction", broadcast_param());
    BC_REQUIRE_MESSAGE(sent.is_object() && sent.as_object().contains("result"), serialize(sent));

    const auto result = rest_json("/rest/mempool/info.json");
    BC_REQUIRE_EQUAL(result.at("size").as_int64(), 1);
    BC_REQUIRE_EQUAL(result.at("bytes").as_int64(), to_signed(test::tx1c.virtual_size()));
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos__broadcast_output__checkmempool_hit)
{
    const auto sent = rpc("sendrawtransaction", broadcast_param());
    BC_REQUIRE_MESSAGE(sent.is_object() && sent.as_object().contains("result"), serialize(sent));

    const auto txid = encode_hash(test::tx1c.hash(false));
    const auto checked = rest_json("/rest/getutxos/checkmempool/" + txid + "-0.json");
    const auto unchecked = rest_json("/rest/getutxos/" + txid + "-0.json");
    BC_REQUIRE_EQUAL(as_text(checked.at("bitmap")), "1");
    BC_REQUIRE_EQUAL(checked.at("utxos").as_array().front().at("height").as_int64(), 0x7fffffff);
    BC_REQUIRE_EQUAL(as_text(unchecked.at("bitmap")), "0");
}

BC_AUTO_TEST_CASE(bitcoind_rest__getutxos__broadcast_spent__checkmempool_miss)
{
    const auto sent = rpc("sendrawtransaction", broadcast_param());
    BC_REQUIRE_MESSAGE(sent.is_object() && sent.as_object().contains("result"), serialize(sent));

    const auto funder = encode_hash(test::block1c.transactions_ptr()->front()->hash(false));
    const auto checked = rest_json("/rest/getutxos/checkmempool/" + funder + "-0.json");
    const auto unchecked = rest_json("/rest/getutxos/" + funder + "-0.json");
    BC_REQUIRE_EQUAL(as_text(checked.at("bitmap")), "0");
    BC_REQUIRE_EQUAL(as_text(unchecked.at("bitmap")), "1");
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(bitcoind_rest_credential_tests, bitcoind_rest_credential_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__chaininfo__rest_credential__ok)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/chaininfo.json", BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD), status::ok);
}

BC_AUTO_TEST_CASE(bitcoind_rest__getblockcount__rest_credential__forbidden)
{
    BC_REQUIRE_EQUAL(rpc_status("getblockcount", BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD), status::forbidden);
}

BC_AUTO_TEST_CASE(bitcoind_rest__chaininfo__no_credential__unauthorized)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/chaininfo.json"), status::unauthorized);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(bitcoind_rest_scoped_credential_tests, bitcoind_scoped_credential_setup_fixture)

BC_AUTO_TEST_CASE(bitcoind_rest__chaininfo__rpc_scoped_credential__forbidden)
{
    BC_REQUIRE_EQUAL(rest_status("/rest/chaininfo.json", BITCOIND_TEST_USERNAME, BITCOIND_TEST_PASSWORD), status::forbidden);
}

BC_AUTO_TEST_SUITE_END()
