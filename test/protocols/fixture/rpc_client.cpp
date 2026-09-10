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
#include "rpc_client.hpp"

using namespace libbitcoin;
using namespace boost::beast;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

rpc_client::rpc_client(boost::asio::io_context& service)
  : socket_(service.get_executor())
{
}

rpc_client::~rpc_client()
{
    close();
}

void rpc_client::connect(const boost::asio::ip::tcp::endpoint& endpoint)
{
    socket_.socket().connect(endpoint);
}

void rpc_client::close()
{
    if (websocket_.has_value())
    {
        error_code ec{};
        websocket_.value().close(websocket::close_code::normal, ec);

        // Expected and harmless during teardown:
        // websocket_closed   : normal (graceful handshake).
        // operation_canceled : hard (invalid request test).
        // peer_disconnect    : peer gone (failed authenticate test).
        const auto reason = network::error::ws_to_error_code(ec);
        if (ec &&
            reason != network::error::websocket_closed &&
            reason != network::error::operation_canceled &&
            reason != network::error::peer_disconnect)
        {
            BOOST_WARN_MESSAGE(false, ec.message());
        }

        websocket_.reset();
        return;
    }

    error_code ignore{};
    socket_.socket().close(ignore);
}

boost::json::value rpc_client::dropped()
{
    return boost::json::parse(R"({"dropped":true})");
}

boost::beast::tcp_stream& rpc_client::stream()
{
    return socket_;
}

// tcp
// ----------------------------------------------------------------------------

// A write failure is never expected, so it is checked on every transport.
// Checked governs the read, as a dropped channel is a tested outcome.
boost::json::value rpc_client::send(const std::string& text, bool checked)
{
    error_code ec{};
    net::write(socket_, net::buffer(text), ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    return receive(checked);
}

boost::json::value rpc_client::receive(bool checked)
{
    error_code ec{};
    const auto size = net::read_until(socket_,
        net::dynamic_buffer(buffer_), '\n', ec);

    if (checked)
    {
        BOOST_CHECK_MESSAGE(!ec, ec.message());
    }

    if (ec)
        return dropped();

    // read_until includes the delimiter, which the parse must not see.
    const auto line = buffer_.substr(zero, sub1(size));
    buffer_.erase(zero, size);

    try
    {
        return boost::json::parse(line);
    }
    catch (const boost::system::system_error&)
    {
        // Carry the text, as a bare {} reports nothing to a failed test.
        return boost::json::value{ { "unparsed", line } };
    }
}

// http
// ----------------------------------------------------------------------------

rpc_client::request rpc_client::create_get(std::string_view target)
{
    request out{ http::verb::get, target, network::http::version_1_1 };
    out.set(http::field::host, "localhost");
    out.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    out.keep_alive(true);
    return out;
}

rpc_client::request rpc_client::create_post(std::string_view target,
    std::string_view body)
{
    request out{ http::verb::post, target, network::http::version_1_1 };
    out.set(http::field::host, "localhost");
    out.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    out.set(http::field::content_type, "application/json");
    out.body() = std::string{ body };
    out.prepare_payload();
    out.keep_alive(true);
    return out;
}

static std::string basic(const std::string& username,
    const std::string& password)
{
    const std::string plain{ username + ":" + password };
    return "Basic " + libbitcoin::system::encode_base64(plain);
}

http::response<http::string_body> rpc_client::get(std::string_view target)
{
    error_code ec{};
    http::write(socket_, create_get(target), ec);

    flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    return response;
}

boost::json::value rpc_client::post(const std::string& body,
    const std::string& target, bool checked)
{
    error_code ec{};
    http::write(socket_, create_post(target, body), ec);
    if (checked)
        BOOST_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    if (checked)
        BOOST_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    return test::parse_json(response.body());
}

rpc_client::status rpc_client::post_status(const std::string& body,
    const std::string& target)
{
    error_code ec{};
    http::write(socket_, create_post(target, body), ec);

    flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

boost::json::value rpc_client::post_authorized(const std::string& body,
    const std::string& username, const std::string& password)
{
    auto out = create_post("/", body);
    out.set(http::field::authorization, basic(username, password));

    error_code ec{};
    http::write(socket_, out, ec);
    if (ec)
        return dropped();

    flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    if (ec)
        return dropped();

    return test::parse_json(response.body());
}

rpc_client::status rpc_client::post_status_authorized(const std::string& body,
    const std::string& username, const std::string& password)
{
    auto out = create_post("/", body);
    out.set(http::field::authorization, basic(username, password));

    error_code ec{};
    http::write(socket_, out, ec);

    flat_buffer buffer{};
    http::response<http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

// websocket
// ----------------------------------------------------------------------------

bool rpc_client::upgraded() const
{
    return websocket_.has_value();
}

rpc_client::error_code rpc_client::upgrade()
{
    error_code ec{};
    BOOST_CHECK(!websocket_.has_value());

    websocket_.emplace(socket_);
    websocket_.value().text(true);
    websocket_.value().handshake("localhost", "/", ec);

    // A refused upgrade leaves the connection in http (teardown as such).
    if (ec)
        websocket_.reset();

    return ec;
}

rpc_client::error_code rpc_client::upgrade(const std::string& username,
    const std::string& password)
{
    error_code ec{};
    const auto credential = basic(username, password);

    BOOST_CHECK(!websocket_.has_value());
    websocket_.emplace(socket_);
    websocket_.value().text(true);
    websocket_.value().set_option(websocket::stream_base::decorator(
        [credential](websocket::request_type& out) NOEXCEPT
        {
            out.set(http::field::authorization, credential);
        }));

    websocket_.value().handshake("localhost", "/", ec);

    if (ec)
        websocket_.reset();

    return ec;
}

void rpc_client::write_frame(std::string_view text)
{
    error_code ec{};
    BOOST_CHECK(websocket_.has_value());

    websocket_.value().write(net::buffer(text), ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());
}

boost::json::value rpc_client::read_frame(bool checked)
{
    error_code ec{};
    BOOST_CHECK(websocket_.has_value());

    flat_buffer buffer{};
    websocket_.value().read(buffer, ec);
    if (checked)
    {
        BOOST_CHECK_MESSAGE(!ec, ec.message());
    }

    if (ec)
        return dropped();

    return test::parse_json(buffers_to_string(buffer.data()));
}

boost::json::value rpc_client::frame(const std::string& text, bool checked)
{
    error_code ec{};
    BOOST_CHECK(websocket_.has_value());

    websocket_.value().write(net::buffer(text), ec);
    BOOST_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    return read_frame(checked);
}

BC_POP_WARNING()
