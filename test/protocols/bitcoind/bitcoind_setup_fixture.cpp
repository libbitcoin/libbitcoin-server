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
#include "../../mocks/blocks.hpp"
#include "bitcoind_setup_fixture.hpp"
#include <future>
#include <sstream>

using namespace boost::beast;

bool setup_candidate_store(test::query_t& query, bool associated) NOEXCEPT
{
    const auto& header = test::mock_block10.header();
    const database::context context{ 0, 10, 0 };
    return test::setup_ten_block_store(query) &&
        query.push_candidate(query.to_header(test::block1_hash)) &&
        query.push_candidate(query.to_header(test::block2_hash)) &&
        query.push_candidate(query.to_header(test::block3_hash)) &&
        query.push_candidate(query.to_header(test::block4_hash)) &&
        query.push_candidate(query.to_header(test::block5_hash)) &&
        query.push_candidate(query.to_header(test::block6_hash)) &&
        query.push_candidate(query.to_header(test::block7_hash)) &&
        query.push_candidate(query.to_header(test::block8_hash)) &&
        query.push_candidate(query.to_header(test::block9_hash)) &&
        (associated ?
            query.set(test::mock_block10, context, {}, false, false) :
            query.set(header, context, {}, false)) &&
        query.push_candidate(query.to_header(header.hash()));
}

bool setup_simultaneous_store(test::query_t& query) NOEXCEPT
{
    const auto& header = test::block3.header();
    const auto timestamp = test::block2.header().timestamp();
    const system::chain::header top{ header.version(), test::block2_hash, header.merkle_root(), timestamp, header.bits(), header.nonce() };
    return test::setup_three_block_store(query) &&
        query.set(top, database::context{ 0, 3, 0 }, {}, false) &&
        query.push_confirmed(query.to_header(top.hash()), false);
}

// A faulted store: the confirmed top has no parent.
bool setup_unrooted_store(test::query_t& query) NOEXCEPT
{
    const auto& header = test::block3.header();
    const system::chain::header top{ header.version(), system::null_hash, header.merkle_root(), header.timestamp(), header.bits(), header.nonce() };
    return test::setup_three_block_store(query) &&
        query.set(top, database::context{ 0, 3, 0 }, {}, false) &&
        query.push_confirmed(query.to_header(top.hash()), false);
}

// A faulted store: the candidate top is not a header.
bool setup_dangling_candidate_store(test::query_t& query) NOEXCEPT
{
    return test::setup_three_block_store(query) &&
        query.push_candidate(query.to_header(test::block1_hash)) &&
        query.push_candidate(query.to_header(test::block2_hash)) &&
        query.push_candidate(database::header_link{ 42 });
}

bitcoind_setup_fixture::bitcoind_setup_fixture(const initializer& setup,
    const configurator& configure, bool start)
  : rpc_setup_fixture(setup,
        [configure](configuration& config) NOEXCEPT
        {
            auto& bitcoind = config.server.bitcoind;
            bitcoind.binds = { { BITCOIND_ENDPOINT } };
            bitcoind.connections = 1;

            if (configure)
                configure(config);
        }, true, start)
{
    client_.connect(config_.server.bitcoind.binds.back().to_endpoint());
}

bitcoind_setup_fixture::~bitcoind_setup_fixture()
{
    client_.close();
}

static std::string body_of(std::string_view method, std::string_view params)
{
    std::ostringstream body{};
    body << R"({"jsonrpc":"2.0","id":0,"method":")" << method
        << R"(","params":)" << params << "}";
    return body.str();
}

boost::json::value bitcoind_setup_fixture::rpc(std::string_view method,
    std::string_view params)
{
    return client_.post(body_of(method, params), "/", true);
}

boost::json::value bitcoind_setup_fixture::tcp_rpc(std::string_view method,
    std::string_view params)
{
    return client_.send(body_of(method, params) + "\n");
}

boost::json::value bitcoind_setup_fixture::rpc_body(std::string_view body)
{
    return client_.post(std::string{ body });
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::rpc_body_status(std::string_view body)
{
    return client_.post_status(std::string{ body });
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::rpc_status(std::string_view method,
    const std::string& username, const std::string& password)
{
    return client_.post_status_authorized(body_of(method, "[]"), username,
        password);
}

network::boost_code bitcoind_setup_fixture::ws_upgrade()
{
    return client_.upgrade();
}

network::boost_code bitcoind_setup_fixture::ws_upgrade(
    const std::string& username, const std::string& password)
{
    return client_.upgrade(username, password);
}

boost::json::value bitcoind_setup_fixture::ws_rpc(std::string_view method,
    std::string_view params)
{
    return client_.frame(body_of(method, params), true);
}

boost::json::value bitcoind_setup_fixture::ws_rpc_dropped(
    std::string_view method, std::string_view params)
{
    return client_.frame(body_of(method, params));
}

void bitcoind_setup_fixture::ws_notify(std::string_view body)
{
    client_.write_frame(body);
}

boost::json::value bitcoind_setup_fixture::ws_read()
{
    return client_.read_frame(true);
}

static bitcoind_setup_fixture::status status_of(tcp_stream& socket,
    const rpc_client::request& request)
{
    network::boost_code ec{};
    http::write(socket, request, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(socket, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    return response.result();
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::rpc_origin_status(std::string_view method,
    std::string_view origin)
{
    auto request = rpc_client::create_post("/", body_of(method, "[]"));
    request.set(http::field::origin, origin);
    return status_of(client_.stream(), request);
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::options_origin_status(std::string_view target,
    std::string_view origin)
{
    rpc_client::request request{ http::verb::options, target,
        network::http::version_1_1 };
    request.set(http::field::host, "localhost");
    request.set(http::field::origin, origin);
    request.keep_alive(true);
    return status_of(client_.stream(), request);
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::rest_status(std::string_view target)
{
    return client_.get(target).result();
}

bitcoind_setup_fixture::status
bitcoind_setup_fixture::options_status(std::string_view target)
{
    return client_.options_status(target);
}

boost::json::value bitcoind_setup_fixture::rest_json(std::string_view target)
{
    const auto response = client_.get(target);
    BOOST_CHECK_EQUAL(response.result(), http::status::ok);
    return test::parse_json(response.body());
}

std::string bitcoind_setup_fixture::rest_text(std::string_view target)
{
    const auto response = client_.get(target);
    BOOST_CHECK_EQUAL(response.result(), http::status::ok);

    auto body = response.body();
    system::trim_right(body);
    return body;
}

// The rest chunk body is not a json-rpc transport, so it reads the stream.
system::data_chunk bitcoind_setup_fixture::rest_data(std::string_view target)
{
    auto& socket = client_.stream();
    http::write(socket, rpc_client::create_get(target));

    flat_buffer buffer{};
    network::boost_code ec{};
    http::response<network::http::chunk_body> response{};
    http::read(socket, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_CHECK_EQUAL(response.result(), http::status::ok);
    return system::to_chunk(response.body());
}
