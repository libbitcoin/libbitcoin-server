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
#include "native_setup_fixture.hpp"

using namespace system;
namespace http = network::http;
namespace ws = network::ws;

BC_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// top (http)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__top__json__expected)
{
    const auto response = get_json("/v1/top?format=json");
    BC_REQUIRE(response.is_int64());
    BC_REQUIRE_EQUAL(response.as_int64(), 9);
}

BC_AUTO_TEST_CASE(native__top__text__expected)
{
    const auto body = get_text("/v1/top?format=text");
    BC_REQUIRE_EQUAL(body, "09");
}

BC_AUTO_TEST_CASE(native__top__data__expected)
{
    const auto body = get_data("/v1/top?format=data");
    BC_REQUIRE_EQUAL(body, base16_chunk("09"));
}

BC_AUTO_TEST_CASE(native__top__xml__bad_request)
{
    const auto status = get_status("/v1/top?format=xml");
    BC_REQUIRE_EQUAL(status, http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__top__default__not_acceptable)
{
    const auto status = get_status("/v1/top");
    BC_REQUIRE_EQUAL(status, http::status::not_acceptable);
}

// subscribe

BC_AUTO_TEST_CASE(native__top_subscribe__json__expected)
{
    const auto response = get_json("/v1/top/subscribe?format=json");
    BC_REQUIRE(response.is_int64());
    BC_REQUIRE_EQUAL(response.as_int64(), 9);
}

// top (websockets)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__ws_upgrade__always__success)
{
    const auto ec = ws_upgrade();
    BC_REQUIRE_MESSAGE(!ec, ec.message());
}

BC_AUTO_TEST_CASE(native__ws_top__json__expected)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/top?format=json");
    BC_REQUIRE(response.is_int64());
    BC_REQUIRE_EQUAL(response.as_int64(), 9);
}

BC_AUTO_TEST_CASE(native__ws_top__text__expected)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/top?format=text");
    BC_REQUIRE_EQUAL(response, "09");
}

BC_AUTO_TEST_CASE(native__ws_top__data__expected)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_data("/v1/top?format=data");
    BC_REQUIRE_EQUAL(response, base16_chunk("09"));
}

BC_AUTO_TEST_CASE(native__ws_top__xml__closed)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_dropped("/v1/top?format=xml"));
}

// subscribe

BC_AUTO_TEST_CASE(native__ws_top_subscribe__json__expected)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/top/subscribe?format=json");
    BC_REQUIRE(response.is_int64());
    BC_REQUIRE_EQUAL(response.as_int64(), 9);
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__stop__empty)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/top/subscribe?stop=true");
    BC_REQUIRE(response.empty());
}

BC_AUTO_TEST_CASE(native__ws_block__not_found_json__empty)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/block/height/10/header");
    BC_REQUIRE(response.empty());
}

BC_AUTO_TEST_CASE(native__ws_block__not_found_text__empty)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/block/height/10?format=text");
    BC_REQUIRE(response.empty());
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__repeat__idempotent)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response1 = ws_get_json("/v1/top/subscribe?format=json");
    BC_REQUIRE(response1.is_int64());
    BC_REQUIRE_EQUAL(response1.as_int64(), 9);

    const auto response2 = ws_get_json("/v1/top/subscribe?format=json");
    BC_REQUIRE(response2.is_int64());
    BC_REQUIRE_EQUAL(response2.as_int64(), 9);
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__progressive_notify__expected)
{
    BC_REQUIRE(!ws_upgrade());

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.set(test::mock_block11, database::context{ 0, 11, 0 }, {}, false, false));
    BC_REQUIRE(query_.set(test::mock_block12, database::context{ 0, 12, 0 }, {}, false, false));

    const auto response = ws_get_json("/v1/top/subscribe?format=json");
    BC_REQUIRE(response.is_int64());
    BC_REQUIRE_EQUAL(response.as_int64(), 9);

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    BC_REQUIRE_EQUAL(ws_get_text("/v1/top/subscribe?format=text"), "0a");

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block11.hash()), true));
    notify(node::chases::block{ 11 });

    BC_REQUIRE_EQUAL(to_string(ws_receive()), "0b");
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__reorganized__emits_top_height)
{
    BC_REQUIRE(!ws_upgrade());

    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.set(test::mock_block11, database::context{ 0, 11, 0 }, {}, false, false));
    BC_REQUIRE(query_.set(test::mock_block12, database::context{ 0, 12, 0 }, {}, false, false));

    const auto response = ws_get_json("/v1/top/subscribe?format=json");
    BC_REQUIRE(response.is_int64());
    BC_REQUIRE_EQUAL(response.as_int64(), 9);

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    BC_REQUIRE_EQUAL(ws_get_text("/v1/top/subscribe?format=text"), "0a");

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block11.hash()), true));
    notify(node::chases::reorganized{ 11 });

    BC_REQUIRE_EQUAL(to_string(ws_receive()), "0b");
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__notify_data__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE_EQUAL(ws_get_data("/v1/top/subscribe?format=data"), base16_chunk("09"));

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chases::block{ 10 });

    BC_REQUIRE_EQUAL(ws_receive(), base16_chunk("0a"));
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__notify_json__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE_EQUAL(ws_get_json("/v1/top/subscribe?format=json").as_int64(), 9);

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chases::block{ 10 });

    BC_REQUIRE_EQUAL(test::parse_json(to_string(ws_receive())).as_int64(), 10);
}

// dispatch
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__dispatch__invalid_witness__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1?format=data&witness=maybe"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__invalid_turbo__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1?format=data&turbo=maybe"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__invalid_stop__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/subscribe?format=data&stop=maybe"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__unsupported_witness__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json&witness=false"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__invalid_target__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/bogus"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__html__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=html"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__ws_dispatch__html__closed)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_dropped("/v1/top?format=html"));
}

BC_AUTO_TEST_CASE(native__ws_dispatch__invalid_target__closed)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_dropped("/v1/bogus"));
}

BC_AUTO_TEST_CASE(native__ws_dispatch__unsupported_parameter__closed)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_dropped("/v1/top?format=json&witness=false"));
}

BC_AUTO_TEST_CASE(native__dispatch__absolute_form_target__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("http://localhost/v1/top?format=json"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__opaque_origin__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "null"), http::status::forbidden);
}

// block
// ----------------------------------------------------------------------------

static const std::string block1_hash = "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048";
static const std::string coinbase1_hash = "0e3e2357e806b6cdb1f70b54c3a3a17b6714ee1f0e68bebb44a74b1efd512098";
static const chain::transaction& coinbase1() NOEXCEPT
{
    return *test::block1.transactions_ptr()->front();
}

BC_AUTO_TEST_CASE(native__block__height_data__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1?format=data"), to_chunk(test::block1_data));
}

BC_AUTO_TEST_CASE(native__block__hash_data__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/hash/" + block1_hash + "?format=data"), to_chunk(test::block1_data));
}

BC_AUTO_TEST_CASE(native__block__text__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/block/height/1?format=text"), encode_base16(test::block1_data));
}

BC_AUTO_TEST_CASE(native__block__no_witness__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1?format=data&witness=false"), to_chunk(test::block1_data));
}

BC_AUTO_TEST_CASE(native__block__json__expected)
{
    const auto response = get_json("/v1/block/height/1?format=json");
    BC_REQUIRE(response.is_object());

    const auto& header = response.as_object().at("header").as_object();
    BC_REQUIRE_EQUAL(header.at("hash").as_string(), block1_hash);
    BC_REQUIRE_EQUAL(header.at("height").as_int64(), 1);

    const auto& txs = response.as_object().at("transactions").as_array();
    BC_REQUIRE_EQUAL(txs.size(), 1u);
    BC_REQUIRE_EQUAL(txs.front().as_object().at("hash").as_string(), coinbase1_hash);
}

BC_AUTO_TEST_CASE(native__block__json_by_hash__injects_height)
{
    const auto response = get_json("/v1/block/hash/" + block1_hash + "?format=json");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("header").as_object().at("height").as_int64(), 1);
}

BC_AUTO_TEST_CASE(native__block__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10?format=data"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block__unknown_hash__not_found)
{
    const auto hash = encode_hash(null_hash);
    BC_REQUIRE_EQUAL(get_status("/v1/block/hash/" + hash + "?format=data"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__ws_block__default__json)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_json("/v1/block/height/1");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("header").as_object().at("hash").as_string(), block1_hash);
}

// block/header
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_header__data__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1/header?format=data"), to_chunk(test::header1_data));
}

BC_AUTO_TEST_CASE(native__block_header__text__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/block/hash/" + block1_hash + "/header?format=text"), encode_base16(test::header1_data));
}

BC_AUTO_TEST_CASE(native__block_header__json__expected)
{
    const auto response = get_json("/v1/block/height/1/header?format=json");
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BC_REQUIRE_EQUAL(object.at("hash").as_string(), block1_hash);
    BC_REQUIRE_EQUAL(object.at("previous").as_string(), encode_hash(test::block0_hash));
    BC_REQUIRE_EQUAL(object.at("height").as_int64(), 1);
}

BC_AUTO_TEST_CASE(native__block_header__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/header?format=data"), http::status::not_found);
}

// block/header/context
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_header_context__json__expected)
{
    const auto response = get_json("/v1/block/height/1/header/context?format=json");
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BC_REQUIRE_EQUAL(object.at("hash").as_string(), block1_hash);
    BC_REQUIRE_EQUAL(object.at("height").as_int64(), 1);
    BC_REQUIRE_EQUAL(object.at("mtp").as_int64(), 1231006505);

    const auto& state = object.at("state").as_object();
    BC_REQUIRE_EQUAL(state.at("size").as_int64(), 215);
    BC_REQUIRE_EQUAL(state.at("weight").as_int64(), 860);
    BC_REQUIRE_EQUAL(state.at("count").as_int64(), 1);
    BC_REQUIRE(state.at("validated").as_bool());
    BC_REQUIRE(state.at("confirmed").as_bool());
    BC_REQUIRE(state.at("confirmable").as_bool());
    BC_REQUIRE(!state.at("unconfirmable").as_bool());

    const auto& forks = object.at("forks").as_object();
    BC_REQUIRE_EQUAL(forks.size(), 19u);
    BC_REQUIRE(!forks.at("bip30").as_bool());
    BC_REQUIRE(!forks.at("bip34").as_bool());
    BC_REQUIRE(!forks.at("bip141").as_bool());
    BC_REQUIRE(!forks.at("bip342").as_bool());
    BC_REQUIRE(!forks.at("bip54_timewarp").as_bool());
    BC_REQUIRE(!forks.at("bip54_timespan").as_bool());
    BC_REQUIRE(!forks.at("bip54_sigops").as_bool());
    BC_REQUIRE(!forks.at("bip54_txsize").as_bool());
    BC_REQUIRE(!forks.at("bip54_coinbase").as_bool());
}

BC_AUTO_TEST_CASE(native__block_header_context__text__not_acceptable)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/header/context?format=text"), http::status::not_acceptable);
}

BC_AUTO_TEST_CASE(native__block_header_context__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/header/context?format=json"), http::status::not_found);
}

// block/details
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_details__json__expected)
{
    const auto response = get_json("/v1/block/height/1/details?format=json");
    BC_REQUIRE(response.is_object());

    const auto& object = response.as_object();
    BC_REQUIRE_EQUAL(object.at("hash").as_string(), block1_hash);
    BC_REQUIRE_EQUAL(object.at("height").as_int64(), 1);
    BC_REQUIRE_EQUAL(object.at("count").as_int64(), 1);
    BC_REQUIRE(!object.at("segregated").as_bool());
    BC_REQUIRE_EQUAL(object.at("nominal").as_int64(), 215);
    BC_REQUIRE_EQUAL(object.at("maximal").as_int64(), 215);
    BC_REQUIRE_EQUAL(object.at("weight").as_int64(), 860);
    BC_REQUIRE_EQUAL(object.at("virtual").as_int64(), 215);
    BC_REQUIRE_EQUAL(object.at("value").as_int64(), 0);
    BC_REQUIRE_EQUAL(object.at("fees").as_int64(), 0);
    BC_REQUIRE_EQUAL(object.at("subsidy").as_int64(), 5000000000);
    BC_REQUIRE_EQUAL(object.at("reward").as_int64(), 5000000000);
    BC_REQUIRE_EQUAL(object.at("claim").as_int64(), 5000000000);
}

BC_AUTO_TEST_CASE(native__block_details__by_hash__expected)
{
    const auto response = get_json("/v1/block/hash/" + block1_hash + "/details?format=json");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("height").as_int64(), 1);
}

BC_AUTO_TEST_CASE(native__block_details__data__not_acceptable)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/details?format=data"), http::status::not_acceptable);
}

BC_AUTO_TEST_CASE(native__block_details__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/details?format=json"), http::status::not_found);
}

// block/txs
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_txs__json__expected)
{
    const auto response = get_json("/v1/block/height/1/txs?format=json");
    BC_REQUIRE(response.is_array());
    BC_REQUIRE_EQUAL(response.as_array().size(), 1u);
    BC_REQUIRE_EQUAL(response.as_array().front().as_string(), coinbase1_hash);
}

BC_AUTO_TEST_CASE(native__block_txs__data__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1/txs?format=data"), to_chunk(coinbase1().hash(false)));
}

BC_AUTO_TEST_CASE(native__block_txs__text__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/block/height/1/txs?format=text"), encode_base16(coinbase1().hash(false)));
}

BC_AUTO_TEST_CASE(native__block_txs__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/txs?format=json"), http::status::not_found);
}

// block/tx
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_tx__data__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1/tx/0?format=data"), coinbase1().to_data(true));
}

BC_AUTO_TEST_CASE(native__block_tx__text__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/block/hash/" + block1_hash + "/tx/0?format=text"), encode_base16(coinbase1().to_data(true)));
}

BC_AUTO_TEST_CASE(native__block_tx__json__expected)
{
    const auto response = get_json("/v1/block/height/1/tx/0?format=json");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("hash").as_string(), coinbase1_hash);
}

BC_AUTO_TEST_CASE(native__block_tx__position_above_count__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/tx/1?format=data"), http::status::not_found);
}

// block/filter
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_filter__genesis__hashes_to_filter_hash)
{
    const auto filter = get_data("/v1/block/height/0/filter/0?format=data");
    BC_REQUIRE(!filter.empty());

    const auto filter_hash = get_data("/v1/block/height/0/filter/0/hash?format=data");
    BC_REQUIRE_EQUAL(filter_hash, to_chunk(bitcoin_hash(filter)));
}

BC_AUTO_TEST_CASE(native__block_filter__genesis__chains_to_filter_header)
{
    const auto filter_hash = get_data("/v1/block/height/0/filter/0/hash?format=data");
    const auto filter_header = get_data("/v1/block/height/0/filter/0/header?format=data");
    BC_REQUIRE_EQUAL(filter_header, to_chunk(bitcoin_hash(splice(filter_hash, null_hash))));
}

BC_AUTO_TEST_CASE(native__block_filter__text__expected)
{
    const auto filter = get_data("/v1/block/height/0/filter/0?format=data");
    BC_REQUIRE_EQUAL(get_text("/v1/block/height/0/filter/0?format=text"), encode_base16(filter));
}

BC_AUTO_TEST_CASE(native__block_filter__json__expected)
{
    const auto filter = get_data("/v1/block/height/0/filter/0?format=data");
    const auto response = get_json("/v1/block/height/0/filter/0?format=json");
    BC_REQUIRE(response.is_string());
    BC_REQUIRE_EQUAL(response.as_string(), encode_base16(filter));
}

BC_AUTO_TEST_CASE(native__block_filter_hash__text_json__expected)
{
    const auto digest = bitcoin_hash(get_data("/v1/block/height/0/filter/0?format=data"));
    const auto response = get_json("/v1/block/height/0/filter/0/hash?format=json");
    BC_REQUIRE_EQUAL(get_text("/v1/block/height/0/filter/0/hash?format=text"), encode_base16(digest));
    BC_REQUIRE_EQUAL(response.as_string(), encode_hash(digest));
}

BC_AUTO_TEST_CASE(native__block_filter_header__text_json__expected)
{
    const auto filter_hash = bitcoin_hash(get_data("/v1/block/height/0/filter/0?format=data"));
    const auto digest = bitcoin_hash(splice(filter_hash, null_hash));
    const auto response = get_json("/v1/block/height/0/filter/0/header?format=json");
    BC_REQUIRE_EQUAL(get_text("/v1/block/height/0/filter/0/header?format=text"), encode_base16(digest));
    BC_REQUIRE_EQUAL(response.as_string(), encode_hash(digest));
}

BC_AUTO_TEST_CASE(native__block_filter__unknown_type__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/0/filter/1?format=data"), http::status::not_implemented);
}

BC_AUTO_TEST_CASE(native__block_filter_hash__unknown_type__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/0/filter/1/hash?format=data"), http::status::not_implemented);
}

BC_AUTO_TEST_CASE(native__block_filter_header__unknown_type__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/0/filter/1/header?format=data"), http::status::not_implemented);
}

BC_AUTO_TEST_CASE(native__block_filter__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/filter/0?format=data"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_filter_hash__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/filter/0/hash?format=data"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_filter_header__above_top__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/10/filter/0/header?format=data"), http::status::not_found);
}

// block/subscribe
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__block_subscribe__json__top_hash)
{
    const auto response = get_json("/v1/block/subscribe?format=json");
    BC_REQUIRE(response.is_string());
    BC_REQUIRE_EQUAL(response.as_string(), encode_hash(test::block9_hash));
}

BC_AUTO_TEST_CASE(native__block_subscribe__data__top_hash)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/subscribe?format=data"), to_chunk(test::block9_hash));
}

BC_AUTO_TEST_CASE(native__block_subscribe__text__top_hash)
{
    BC_REQUIRE_EQUAL(get_text("/v1/block/subscribe?format=text"), encode_base16(test::block9_hash));
}

BC_AUTO_TEST_CASE(native__ws_block_subscribe__stop__empty)
{
    BC_REQUIRE(!ws_upgrade());

    const auto response = ws_get_text("/v1/block/subscribe?stop=true");
    BC_REQUIRE(response.empty());
}

BC_AUTO_TEST_CASE(native__ws_block_subscribe__notify_text__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE_EQUAL(ws_get_text("/v1/block/subscribe?format=text"), encode_base16(test::block9_hash));

    const auto link = query_.to_header(test::mock_block10.hash());
    BC_REQUIRE(query_.push_confirmed(link, true));
    notify(node::chases::block{ link.value });

    BC_REQUIRE_EQUAL(to_string(ws_receive()), encode_base16(test::mock_block10.hash()));
}

BC_AUTO_TEST_CASE(native__ws_block_subscribe__notify_data__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE_EQUAL(ws_get_data("/v1/block/subscribe?format=data"), to_chunk(test::block9_hash));

    const auto link = query_.to_header(test::mock_block10.hash());
    BC_REQUIRE(query_.push_confirmed(link, true));
    notify(node::chases::block{ link.value });

    BC_REQUIRE_EQUAL(ws_receive(), to_chunk(test::mock_block10.hash()));
}

BC_AUTO_TEST_CASE(native__ws_block_subscribe__notify_json__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE_EQUAL(ws_get_json("/v1/block/subscribe?format=json").as_string(), encode_hash(test::block9_hash));

    const auto link = query_.to_header(test::mock_block10.hash());
    BC_REQUIRE(query_.push_confirmed(link, true));
    notify(node::chases::block{ link.value });

    const auto response = test::parse_json(to_string(ws_receive()));
    BC_REQUIRE_EQUAL(response.as_string(), encode_hash(test::mock_block10.hash()));
}

BC_AUTO_TEST_SUITE_END()

// embedded site
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(native_embedded_tests, native_embedded_setup_fixture)

BC_AUTO_TEST_CASE(native__embedded__root__html)
{
    BC_REQUIRE_EQUAL(get_text("/"), "<html></html>");
}

BC_AUTO_TEST_CASE(native__embedded__css__css)
{
    BC_REQUIRE_EQUAL(get_text("/style.css"), "body{}");
}

BC_AUTO_TEST_CASE(native__embedded__js__ecma)
{
    BC_REQUIRE_EQUAL(get_text("/app.js"), "var x;");
}

BC_AUTO_TEST_CASE(native__embedded__woff__font)
{
    BC_REQUIRE_EQUAL(get_text("/font.woff"), "font");
}

BC_AUTO_TEST_CASE(native__embedded__png__icon)
{
    BC_REQUIRE_EQUAL(get_text("/icon.png"), "icon");
}

BC_AUTO_TEST_CASE(native__embedded__unknown_type__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/readme.txt"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__embedded__api__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/top?format=text"), "09");
}

BC_AUTO_TEST_SUITE_END()

// file site
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(native_file_tests, native_file_setup_fixture)

BC_AUTO_TEST_CASE(native__file__root__default_page)
{
    BC_REQUIRE_EQUAL(get_text("/"), "<p>index</p>");
}

BC_AUTO_TEST_CASE(native__file__extensionless__default_page)
{
    BC_REQUIRE_EQUAL(get_text("/some/route"), "<p>index</p>");
}

BC_AUTO_TEST_CASE(native__file__named__expected)
{
    BC_REQUIRE_EQUAL(get_text("/page.css"), "p{}");
}

BC_AUTO_TEST_CASE(native__file__missing__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/missing.css"), http::status::not_found);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(native_no_default_tests, native_no_default_setup_fixture)

BC_AUTO_TEST_CASE(native__no_default__extensionless__not_implemented)
{
    BC_REQUIRE_EQUAL(get_status("/some/route"), http::status::not_implemented);
}

BC_AUTO_TEST_SUITE_END()

// hosts
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(native_hosts_tests, native_hosts_setup_fixture)

BC_AUTO_TEST_CASE(native__hosts__unlisted_host__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__hosts__listed_host__expected)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::host, "example.com"), http::status::ok);
}

BC_AUTO_TEST_SUITE_END()

// faulted stores
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(native_unassociated_tests, native_unassociated_setup_fixture)

BC_AUTO_TEST_CASE(native__block__unassociated__internal_server_error)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/3?format=data"), http::status::internal_server_error);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(native_witness_tests, native_witness_setup_fixture)

BC_AUTO_TEST_CASE(native__block_details__missing_prevouts__internal_server_error)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/2/details?format=json"), http::status::internal_server_error);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(native_scoped_credential_tests, native_scoped_credential_setup_fixture)

BC_AUTO_TEST_CASE(native__top__scoped_credential__ok)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::authorization, NATIVE_TEST_AUTHORIZATION), http::status::ok);
}

BC_AUTO_TEST_CASE(native__block_header__scoped_credential_unlisted_method__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/0/header?format=json", http::field::authorization, NATIVE_TEST_AUTHORIZATION), http::status::forbidden);
}

BC_AUTO_TEST_CASE(native__top__no_credential__unauthorized)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json"), http::status::unauthorized);
}

BC_AUTO_TEST_SUITE_END()
