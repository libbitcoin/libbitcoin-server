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
namespace http = network::http;
namespace ws = network::ws;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

rpc_client::rpc_client(network::io::context& service)
  : socket_(service)
{
}

rpc_client::~rpc_client()
{
    close();
}

void rpc_client::connect(const network::io::endpoint& endpoint)
{
    REQUIRE_SUCCESS(socket_, connect, endpoint);
}

void rpc_client::close()
{
    if (websocket_.has_value())
    {
        error_code ec{};
        ec = websocket_.value().close(network::ws::close_code::normal);

        // Expected and harmless during teardown:
        // websocket_closed   : normal (graceful handshake).
        // operation_canceled : hard (invalid request test).
        // peer_disconnect    : peer gone (failed authenticate test).
        if (ec &&
            ec != network::error::websocket_closed &&
            ec != network::error::operation_canceled &&
            ec != network::error::peer_disconnect)
        {
            BC_WARN_MESSAGE(false, ec.message());
        }

        websocket_.reset();
        return;
    }

    error_code ignore{};
    socket_.close(ignore);
}

system::json::value rpc_client::dropped()
{
    return system::json::parse(R"({"dropped":true})");
}

network::io::socket& rpc_client::stream()
{
    return socket_;
}

// tcp
// ----------------------------------------------------------------------------

// A write failure is never expected, so it is checked on every transport.
// Checked governs the read, as a dropped channel is a tested outcome.
system::json::value rpc_client::send(const std::string& text, bool checked)
{
    error_code ec{};
    network::io::write(socket_, network::io::buffer(text), ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    return receive(checked);
}

system::json::value rpc_client::receive(bool checked)
{
    error_code ec{};
    const auto size = network::io::read_until(socket_, buffer_, "\n", ec);

    if (checked)
    {
        BC_CHECK_MESSAGE(!ec, ec.message());
    }

    if (ec)
        return dropped();

    // read_until includes the delimiter, which the parse must not see.
    const auto line = buffer_.substr(zero, sub1(size));
    buffer_.erase(zero, size);

    try
    {
        return system::json::parse(line);
    }
    catch (const system::json::exception&)
    {
        // Carry the text, as a bare {} reports nothing to a failed test.
        return system::json::value{ { "unparsed", line } };
    }
}

// http
// ----------------------------------------------------------------------------

rpc_client::request rpc_client::create_get(std::string_view target)
{
    request out{ http::verb::get, target, network::http::version_1_1 };
    out.set(http::field::host, "localhost");
    out.set(http::field::user_agent, "libbitcoin");
    out.keep_alive(true);
    return out;
}

rpc_client::request rpc_client::create_post(std::string_view target,
    std::string_view body, std::string_view content_type)
{
    request out{ http::verb::post, target, network::http::version_1_1 };
    out.set(http::field::host, "localhost");
    out.set(http::field::user_agent, "libbitcoin");
    if (!content_type.empty())
        out.set(http::field::content_type, content_type);

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

http::message<false, http::string_body> rpc_client::get(std::string_view target)
{
    error_code ec{};
    http::write(socket_, create_get(target), ec);

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response;
}

http::message<false, http::string_body> rpc_client::get_authorized(
    std::string_view target, const std::string& username,
    const std::string& password)
{
    auto out = create_get(target);
    out.set(http::field::authorization, basic(username, password));

    error_code ec{};
    http::write(socket_, out, ec);

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response;
}

system::json::value rpc_client::post(const std::string& body,
    const std::string& target, bool checked, std::string_view content_type)
{
    error_code ec{};
    http::write(socket_, create_post(target, body, content_type), ec);
    if (checked)
        BC_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    if (checked)
        BC_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    return test::parse_json(response.body());
}

rpc_client::status rpc_client::options_status(std::string_view target)
{
    request out{ http::verb::options, target, network::http::version_1_1 };
    out.set(http::field::host, "localhost");
    out.set(http::field::user_agent, "libbitcoin");
    out.keep_alive(true);

    error_code ec{};
    http::write(socket_, out, ec);

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

rpc_client::status rpc_client::post_status(const std::string& body,
    const std::string& target)
{
    error_code ec{};
    http::write(socket_, create_post(target, body), ec);

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

    return response.result();
}

system::json::value rpc_client::post_authorized(const std::string& body,
    const std::string& username, const std::string& password)
{
    auto out = create_post("/", body);
    out.set(http::field::authorization, basic(username, password));

    error_code ec{};
    http::write(socket_, out, ec);
    if (ec)
        return dropped();

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
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

    http::flat_buffer buffer{};
    http::message<false, http::string_body> response{};
    http::read(socket_, buffer, response, ec);
    BC_CHECK_MESSAGE(!ec, ec.message());

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
    BC_CHECK(!websocket_.has_value());

    websocket_.emplace(socket_);
    ec = websocket_.value().handshake("localhost", "/");

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

    BC_CHECK(!websocket_.has_value());
    websocket_.emplace(socket_);
    network::http::fields extra{};
    extra.set(http::field::authorization, credential);
    network::ws::response response{};
    ec = websocket_.value().handshake(response, "localhost", "/", extra);

    if (ec)
        websocket_.reset();

    return ec;
}

void rpc_client::write_frame(std::string_view text)
{
    error_code ec{};
    BC_CHECK(websocket_.has_value());

    ec = websocket_.value().write(text);
    BC_CHECK_MESSAGE(!ec, ec.message());
}

system::json::value rpc_client::read_frame(bool checked)
{
    error_code ec{};
    BC_CHECK(websocket_.has_value());

    http::flat_buffer buffer{};
    ec = websocket_.value().read(buffer);
    if (checked)
    {
        BC_CHECK_MESSAGE(!ec, ec.message());
    }

    if (ec)
        return dropped();

    return test::parse_json(test::to_string(buffer.data()));
}

system::json::value rpc_client::frame(const std::string& text, bool checked)
{
    error_code ec{};
    BC_CHECK(websocket_.has_value());

    ec = websocket_.value().write(text);
    BC_CHECK_MESSAGE(!ec, ec.message());

    if (ec)
        return dropped();

    return read_frame(checked);
}

BC_POP_WARNING()
