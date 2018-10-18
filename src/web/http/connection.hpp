/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_CONNECTION_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_CONNECTION_HPP

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <memory>
#include <vector>

#include "http.hpp"

namespace libbitcoin {
namespace server {
namespace http {

class connection;

typedef std::shared_ptr<connection> connection_ptr;
typedef std::set<connection_ptr> connection_set;
typedef std::vector<connection_ptr> connection_list;
typedef std::function<bool(connection_ptr&, const event, void* data)>
    event_handler;

// This class is instantiated from accepted/incoming HTTP clients.
// Initiating outgoing HTTP connections are not currently supported.
class connection
{
  public:
    static const uint32_t maximum_read_length = (1 << 10); // 1 KB
    static const uint32_t default_high_water_mark = (1 << 21); // 2 MB
    static const uint32_t default_incoming_frame_length = (1 << 19); // 512 KB
    typedef std::function<int32_t(const uint8_t*, uint32_t)> write_method;

    connection();
    connection(sock_t connection, struct sockaddr_in& address);
    ~connection();

    // Properties.
    // ------------------------------------------------------------------------

    connection_state state() const;
    void set_state(connection_state state);

    void* user_data();
    void set_user_data(void* user_data);

    bool websocket() const;
    void set_websocket(bool websocket);

    bool json_rpc() const;
    void set_json_rpc(bool json_rpc);

    size_t high_water_mark() const;
    void set_high_water_mark(size_t high_water_mark);

    size_t maximum_incoming_frame_length() const;
    void set_maximum_incoming_frame_length(size_t length);

    // Websocket endpoints are HTTP specific endpoints such as '/'.
    const std::string& websocket_endpoint() const;
    void set_websocket_endpoint(const std::string endpoint);

    // Readers and Writers.
    // ------------------------------------------------------------------------
    read_buffer& read_buffer();
    data_chunk& write_buffer();

    // Signed integer results use negative range for error code.
    int32_t read_length();
    int32_t read();
    int32_t write(const data_chunk& buffer);
    int32_t write(const std::string& buffer);
    int32_t write(const uint8_t* data, size_t length);
    int32_t do_write(const uint8_t* data, size_t length, bool frame);

    // Other.
    // ------------------------------------------------------------------------

    void set_socket_non_blocking();
    struct sockaddr_in address() const;
    bool reuse_address() const;
    bool closed() const;
    void close();
    sock_t& socket();
    http::ssl& ssl_context();
    bool ssl_enabled() const;
    http::file_transfer& file_transfer();
    http::websocket_transfer& websocket_transfer();

    // Operator overloads.
    // ------------------------------------------------------------------------

    bool operator==(const connection& other);

  private:
    void* user_data_;
    connection_state state_;
    sock_t socket_;
    sockaddr_in address_;
    asio::time_point last_active_;
    size_t high_water_mark_;
    size_t maximum_incoming_frame_length_;
    ssl ssl_context_;
    std::string websocket_endpoint_;
    bool websocket_;
    bool json_rpc_;

    // Transfer states used for read continuations, particularly for when the
    // read_buffer_ size is too small to hold all of the incoming data.
    http::file_transfer file_transfer_;
    http::websocket_transfer websocket_transfer_;

    int32_t bytes_read_;
    http::read_buffer read_buffer_;
    data_chunk write_buffer_;
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
