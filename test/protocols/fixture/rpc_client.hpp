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
#ifndef LIBBITCOIN_SERVER_TEST_PROTOCOLS_FIXTURE_RPC_CLIENT_HPP
#define LIBBITCOIN_SERVER_TEST_PROTOCOLS_FIXTURE_RPC_CLIENT_HPP

#include "../../test.hpp"
#include "../../mocks/blocks.hpp"

/// One client connection to a universal json-rpc service, over any of its
/// three transports: raw tcp (which the server downgrades to a newline
/// delimited stream), http POST, and websocket (by http upgrade). The service
/// fixtures compose this, as they differ only in settings and in how they
/// shape a request.
class rpc_client
{
public:
    using status = boost::beast::http::status;
    using error_code = libbitcoin::network::boost_code;
    using request = boost::beast::http::request<
        boost::beast::http::string_body>;

    DELETE_COPY_MOVE(rpc_client);

    explicit rpc_client(boost::asio::io_context& service);
    ~rpc_client();

    /// Connect and disconnect (websocket closed first if upgraded).
    void connect(const boost::asio::ip::tcp::endpoint& endpoint);
    void close();

    /// Raw tcp, newline delimited (the server detects and downgrades).
    /// Returns {"dropped":true} on transport error, which checked asserts.
    boost::json::value send(const std::string& text, bool checked=false);
    boost::json::value receive(bool checked=false);

    /// http POST of a json body to target, and the request builders.
    boost::json::value post(const std::string& body,
        const std::string& target="/", bool checked=false);
    status post_status(const std::string& body,
        const std::string& target="/");
    boost::json::value post_authorized(const std::string& body,
        const std::string& username, const std::string& password);
    status post_status_authorized(const std::string& body,
        const std::string& username, const std::string& password);
    static request create_get(std::string_view target);
    static request create_post(std::string_view target,
        std::string_view body);

    /// http GET (returns the response for the caller to interpret).
    boost::beast::http::response<boost::beast::http::string_body> get(
        std::string_view target);

    /// Upgrade to websocket, optionally with basic authorization.
    error_code upgrade();
    error_code upgrade(const std::string& username,
        const std::string& password);
    bool upgraded() const;

    /// Framed json over the upgraded websocket.
    boost::json::value frame(const std::string& text, bool checked=false);
    boost::json::value read_frame(bool checked=false);
    void write_frame(std::string_view text);

    /// The underlying stream (for a transport this does not cover).
    boost::beast::tcp_stream& stream();

private:
    using tcp_stream = boost::beast::tcp_stream;
    using websocket_stream = boost::beast::websocket::stream<tcp_stream&>;

    static boost::json::value dropped();

    tcp_stream socket_;
    std::optional<websocket_stream> websocket_{};

    // Retained, as a read may buffer beyond the delimiter.
    std::string buffer_{};
};

#endif
