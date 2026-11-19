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

static data_chunk to_top(const chain::header& header, size_t height)
{
    return splice(chain::checkpoint{ header.hash(), height }.to_data(),
        header.to_data());
}

static data_chunk to_tops(const chain::header& confirmed,
    size_t confirmed_height, const chain::header& candidate,
    size_t candidate_height)
{
    return splice(to_top(confirmed, confirmed_height),
        to_top(candidate, candidate_height));
}

static bool is_top(const json::value& value, const chain::header& header,
    int64_t height)
{
    if (!value.is_object())
        return false;

    const auto& object = value.as_object();
    return object.at("hash").as_string() == encode_hash(header.hash()) &&
        object.at("height").as_int64() == height &&
        object.at("header") == json::value_from(header);
}

static bool is_tops(const json::value& value, const chain::header& confirmed,
    int64_t confirmed_height, const chain::header& candidate,
    int64_t candidate_height)
{
    return value.is_object() && value.as_object().size() == 2u &&
        is_top(value.at("confirmed"), confirmed, confirmed_height) &&
        is_top(value.at("candidate"), candidate, candidate_height);
}

static data_chunk default_tops()
{
    return to_tops(test::block9.header(), 9, test::genesis.header(), 0);
}

BC_FIXTURE_TEST_SUITE(native_tests, native_ten_block_setup_fixture)

// top (http)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__top__json__expected)
{
    BC_REQUIRE(is_tops(get_json("/v1/top?format=json"), test::block9.header(), 9, test::genesis.header(), 0));
}

BC_AUTO_TEST_CASE(native__top__text__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/top?format=text"), encode_base16(default_tops()));
}

BC_AUTO_TEST_CASE(native__top__data__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/top?format=data"), default_tops());
}

BC_AUTO_TEST_CASE(native__top__pushed_candidate_json__candidate_top)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    BC_REQUIRE(is_tops(get_json("/v1/top?format=json"), test::block9.header(), 9, test::block1.header(), 1));
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
    BC_REQUIRE(is_tops(get_json("/v1/top/subscribe?format=json"), test::block9.header(), 9, test::genesis.header(), 0));
}

// top (websockets)
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__ws_upgrade__always__success)
{
    BC_REQUIRE(!ws_upgrade());
}

BC_AUTO_TEST_CASE(native__ws_top__json__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(is_tops(ws_get_json("/v1/top?format=json"), test::block9.header(), 9, test::genesis.header(), 0));
}

BC_AUTO_TEST_CASE(native__ws_top__text__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_text("/v1/top?format=text"), encode_base16(default_tops()));
}

BC_AUTO_TEST_CASE(native__ws_top__data__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_data("/v1/top?format=data"), default_tops());
}

BC_AUTO_TEST_CASE(native__ws_top__xml__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/top?format=xml").starts_with("Bad Request"));
}

// subscribe

BC_AUTO_TEST_CASE(native__ws_top_subscribe__json__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(is_tops(ws_get_json("/v1/top/subscribe?format=json"), test::block9.header(), 9, test::genesis.header(), 0));
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
    BC_REQUIRE(is_tops(ws_get_json("/v1/top/subscribe?format=json"), test::block9.header(), 9, test::genesis.header(), 0));
    BC_REQUIRE(is_tops(ws_get_json("/v1/top/subscribe?format=json"), test::block9.header(), 9, test::genesis.header(), 0));
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__block__emits_tops)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE_EQUAL(ws_get_data("/v1/top/subscribe?format=data"), default_tops());

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chases::block{ node::header_t{ 10 } });

    BC_REQUIRE_EQUAL(ws_receive(), to_tops(test::mock_block10.header(), 10, test::genesis.header(), 0));
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__reorganized__emits_tops)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    BC_REQUIRE_EQUAL(ws_get_data("/v1/top/subscribe?format=data"), to_tops(test::mock_block10.header(), 10, test::genesis.header(), 0));

    BC_REQUIRE(query_.pop_confirmed());
    notify(node::chases::reorganized{ node::header_t{ 10 } });

    BC_REQUIRE_EQUAL(ws_receive(), default_tops());
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__headers__emits_tops)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_data("/v1/top/subscribe?format=data"), default_tops());

    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    notify(node::chases::headers{ node::height_t{ 0 } });

    BC_REQUIRE_EQUAL(ws_receive(), to_tops(test::block9.header(), 9, test::block1.header(), 1));
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__regressed__emits_tops)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block2_hash)));
    BC_REQUIRE_EQUAL(ws_get_data("/v1/top/subscribe?format=data"), to_tops(test::block9.header(), 9, test::block2.header(), 2));

    BC_REQUIRE(query_.pop_candidate());
    notify(node::chases::regressed{ node::height_t{ 1 } });

    BC_REQUIRE_EQUAL(ws_receive(), to_tops(test::block9.header(), 9, test::block1.header(), 1));
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__notify_text__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE_EQUAL(ws_get_text("/v1/top/subscribe?format=text"), encode_base16(default_tops()));

    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    notify(node::chases::headers{ node::height_t{ 0 } });

    BC_REQUIRE_EQUAL(to_string(ws_receive()), encode_base16(to_tops(test::block9.header(), 9, test::block1.header(), 1)));
}

BC_AUTO_TEST_CASE(native__ws_top_subscribe__notify_json__expected)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(query_.set(test::mock_block10, database::context{ 0, 10, 0 }, {}, false, false));
    BC_REQUIRE(is_tops(ws_get_json("/v1/top/subscribe?format=json"), test::block9.header(), 9, test::genesis.header(), 0));

    BC_REQUIRE(query_.push_confirmed(query_.to_header(test::mock_block10.hash()), true));
    notify(node::chases::block{ node::header_t{ 10 } });

    BC_REQUIRE(is_tops(test::parse_json(to_string(ws_receive())), test::mock_block10.header(), 10, test::genesis.header(), 0));
}

// dispatch
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(native__dispatch__invalid_witness__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1?format=data&witness=maybe"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__invalid_candidate__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1?format=data&candidate=maybe"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__unsupported_candidate_top__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json&candidate=true"), http::status::bad_request);
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

BC_AUTO_TEST_CASE(native__ws_dispatch__html__not_acceptable)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/top?format=html").starts_with("Not Acceptable"));
}

BC_AUTO_TEST_CASE(native__ws_dispatch__invalid_target__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/bogus").starts_with("Bad Request"));
}

BC_AUTO_TEST_CASE(native__ws_dispatch__unsupported_parameter__bad_request)
{
    BC_REQUIRE(!ws_upgrade());
    BC_REQUIRE(ws_get_text("/v1/top?format=json&witness=false").starts_with("Bad Request"));
}

BC_AUTO_TEST_CASE(native__dispatch__absolute_form_target__bad_request)
{
    BC_REQUIRE_EQUAL(get_status("http://localhost/v1/top?format=json"), http::status::bad_request);
}

BC_AUTO_TEST_CASE(native__dispatch__opaque_origin__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "null"), http::status::forbidden);
}

BC_AUTO_TEST_CASE(native__dispatch__origin_json__access_control)
{
    const auto response = get_response("/v1/top?format=json", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::access_control_allow_origin], "http://example.com");
    BC_REQUIRE_EQUAL(response[http::field::vary], "origin, accept-encoding");
}

BC_AUTO_TEST_CASE(native__dispatch__origin_text__access_control)
{
    const auto response = get_response("/v1/top?format=text", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::access_control_allow_origin], "http://example.com");
    BC_REQUIRE_EQUAL(response[http::field::vary], "origin, accept-encoding");
}

BC_AUTO_TEST_CASE(native__dispatch__origin_data__access_control)
{
    const auto response = get_response("/v1/top?format=data", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::access_control_allow_origin], "http://example.com");
    BC_REQUIRE_EQUAL(response[http::field::vary], "origin, accept-encoding");
}

BC_AUTO_TEST_CASE(native__dispatch__origin_stream__access_control)
{
    const auto response = get_response("/v1/block/height/1?format=data", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::access_control_allow_origin], "http://example.com");
}

BC_AUTO_TEST_CASE(native__dispatch__no_origin__no_access_control)
{
    const auto response = get_response("/v1/top?format=json");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE(is_zero(response.count(http::field::access_control_allow_origin)));
    BC_REQUIRE_EQUAL(response[http::field::vary], "accept-encoding");
}

BC_AUTO_TEST_CASE(native__dispatch__http_1_0__version_1_0)
{
    const auto response = get_response("/v1/top?format=json", {}, network::http::version_1_0);
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response.version(), network::http::version_1_0);
}

BC_AUTO_TEST_CASE(native__dispatch__not_found_http_1_0__version_1_0)
{
    const auto response = get_response("/v1/block/height/10?format=data", {}, network::http::version_1_0);
    BC_REQUIRE_EQUAL(response.result(), http::status::not_found);
    BC_REQUIRE_EQUAL(response.version(), network::http::version_1_0);
}

// block
// ----------------------------------------------------------------------------

static const std::string block1_hash = "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048";
static const std::string coinbase1_hash = "0e3e2357e806b6cdb1f70b54c3a3a17b6714ee1f0e68bebb44a74b1efd512098";
static const chain::transaction& coinbase1() NOEXCEPT
{
    return *test::block1.transactions_ptr()->front();
}

BC_AUTO_TEST_CASE(native__block__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1?format=data&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block__candidate_pushed__expected)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1?format=data&candidate=true"), to_chunk(test::block1_data));
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

BC_AUTO_TEST_CASE(native__block_header__candidate_genesis__expected)
{
    const auto response = get_json("/v1/block/height/0/header?format=json&candidate=true");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("hash").as_string(), encode_hash(test::block0_hash));
}

BC_AUTO_TEST_CASE(native__block_header__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/header?format=data&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_header__candidate_pushed__expected)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    const auto response = get_json("/v1/block/height/1/header?format=json&candidate=true");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("hash").as_string(), block1_hash);
    BC_REQUIRE_EQUAL(response.as_object().at("height").as_int64(), 1);
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
    BC_REQUIRE_EQUAL(forks.size(), 20u);
    BC_REQUIRE(!forks.at("bip16").as_bool());
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

BC_AUTO_TEST_CASE(native__block_header_context__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/header/context?format=json&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_header_context__candidate_pushed__expected)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    const auto response = get_json("/v1/block/height/1/header/context?format=json&candidate=true");
    BC_REQUIRE(response.is_object());
    BC_REQUIRE_EQUAL(response.as_object().at("hash").as_string(), block1_hash);
    BC_REQUIRE_EQUAL(response.as_object().at("height").as_int64(), 1);
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

BC_AUTO_TEST_CASE(native__block_details__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/details?format=json&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_details__candidate_pushed__expected)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    BC_REQUIRE(get_json("/v1/block/height/1/details?format=json&candidate=true").is_object());
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

BC_AUTO_TEST_CASE(native__block_txs__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/txs?format=data&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_txs__candidate_pushed__expected)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1/txs?format=data&candidate=true"), to_chunk(coinbase1().hash(false)));
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

BC_AUTO_TEST_CASE(native__block_tx__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/tx/0?format=data&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_tx__candidate_pushed__expected)
{
    BC_REQUIRE(query_.push_candidate(query_.to_header(test::block1_hash)));
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/1/tx/0?format=data&candidate=true"), coinbase1().to_data(true));
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

BC_AUTO_TEST_CASE(native__block_filter__candidate_genesis__expected)
{
    BC_REQUIRE_EQUAL(get_data("/v1/block/height/0/filter/0?format=data&candidate=true"), get_data("/v1/block/height/0/filter/0?format=data"));
}

BC_AUTO_TEST_CASE(native__block_filter__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/filter/0?format=data&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_filter_hash__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/filter/0/hash?format=data&candidate=true"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__block_filter_header__candidate_unpushed__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/v1/block/height/1/filter/0/header?format=data&candidate=true"), http::status::not_found);
}

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

BC_AUTO_TEST_CASE(native__embedded__root__tagged)
{
    const auto response = get_response("/", http::field::user_agent, "libbitcoin");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::etag], "W/\"b633a587c652d02386c4f16f8c6f6aab7352d97f16367c3c40576214372dd628\"");
    BC_REQUIRE_EQUAL(response[http::field::cache_control], "no-cache");
    BC_REQUIRE_EQUAL(response.body(), "<html></html>");
}

BC_AUTO_TEST_CASE(native__embedded__css__tagged)
{
    const auto response = get_response("/style.css", http::field::user_agent, "libbitcoin");
    BC_REQUIRE_EQUAL(response[http::field::etag], "W/\"7c98040a541657584690ae2a1cc3b42a8b53b159cc60c5d3abbfecbaeac6c94a\"");
}

BC_AUTO_TEST_CASE(native__embedded__if_none_match_tag__not_modified)
{
    const auto response = get_response("/", http::field::if_none_match, "W/\"b633a587c652d02386c4f16f8c6f6aab7352d97f16367c3c40576214372dd628\"");
    BC_REQUIRE_EQUAL(response.result(), http::status::not_modified);
    BC_REQUIRE_EQUAL(response[http::field::etag], "W/\"b633a587c652d02386c4f16f8c6f6aab7352d97f16367c3c40576214372dd628\"");
    BC_REQUIRE_EQUAL(response[http::field::cache_control], "no-cache");
    BC_REQUIRE(response.body().empty());
}

BC_AUTO_TEST_CASE(native__embedded__if_none_match_strong_form__not_modified)
{
    BC_REQUIRE_EQUAL(get_response("/", http::field::if_none_match, "\"b633a587c652d02386c4f16f8c6f6aab7352d97f16367c3c40576214372dd628\"").result(), http::status::not_modified);
}

BC_AUTO_TEST_CASE(native__embedded__if_none_match_list__not_modified)
{
    BC_REQUIRE_EQUAL(get_response("/", http::field::if_none_match, "\"other\", W/\"b633a587c652d02386c4f16f8c6f6aab7352d97f16367c3c40576214372dd628\"").result(), http::status::not_modified);
}

BC_AUTO_TEST_CASE(native__embedded__if_none_match_any__not_modified)
{
    BC_REQUIRE_EQUAL(get_response("/", http::field::if_none_match, "*").result(), http::status::not_modified);
}

BC_AUTO_TEST_CASE(native__embedded__if_none_match_other__ok)
{
    const auto response = get_response("/", http::field::if_none_match, "W/\"other\"");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response.body(), "<html></html>");
}

BC_AUTO_TEST_CASE(native__embedded__if_none_match_css_tag__not_modified)
{
    BC_REQUIRE_EQUAL(get_response("/style.css", http::field::if_none_match, "W/\"7c98040a541657584690ae2a1cc3b42a8b53b159cc60c5d3abbfecbaeac6c94a\"").result(), http::status::not_modified);
}

BC_AUTO_TEST_CASE(native__embedded__unknown_type__not_found)
{
    BC_REQUIRE_EQUAL(get_status("/readme.txt"), http::status::not_found);
}

BC_AUTO_TEST_CASE(native__embedded__api__expected)
{
    BC_REQUIRE_EQUAL(get_text("/v1/top?format=text"), encode_base16(default_tops()));
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

// origins
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(native_origins_tests, native_origins_setup_fixture)

BC_AUTO_TEST_CASE(native__origins__unlisted_origin__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "http://example.org"), http::status::forbidden);
}

BC_AUTO_TEST_CASE(native__origins__listed_origin__access_control)
{
    const auto response = get_response("/v1/top?format=json", "http://example.com");
    BC_REQUIRE_EQUAL(response.result(), http::status::ok);
    BC_REQUIRE_EQUAL(response[http::field::access_control_allow_origin], "http://example.com");
}

BC_AUTO_TEST_CASE(native__origins__same_origin_unlisted_host__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "http://localhost"), http::status::forbidden);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(native_origins_hosts_tests, native_origins_hosts_setup_fixture)

BC_AUTO_TEST_CASE(native__origins__same_origin__ok)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "http://localhost"), http::status::ok);
}

BC_AUTO_TEST_CASE(native__origins__same_origin_default_port__ok)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "http://LOCALHOST:80"), http::status::ok);
}

BC_AUTO_TEST_CASE(native__origins__same_host_other_scheme__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "https://localhost"), http::status::forbidden);
}

BC_AUTO_TEST_CASE(native__origins__same_host_other_port__forbidden)
{
    BC_REQUIRE_EQUAL(get_status("/v1/top?format=json", http::field::origin, "http://localhost:8080"), http::status::forbidden);
}

BC_AUTO_TEST_SUITE_END()

// delayed
// ----------------------------------------------------------------------------

BC_FIXTURE_TEST_SUITE(native_delayed_tests, native_delayed_setup_fixture)

BC_AUTO_TEST_CASE(native__top__not_current__dropped)
{
    BC_REQUIRE(expect_dropped("/v1/top?format=json"));
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
