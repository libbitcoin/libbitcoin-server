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

#include <set>
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
typedef std::function<bool(connection_ptr&,
    const event, void* data)> event_handler;

// This class is instantiated from accepted/incoming HTTP clients.
// Initiating outgoing HTTP connections are not currently supported.
class connection
{
  public:
    static const int32_t maximum_read_length = (1 << 10); // 1 KB
    static const int32_t default_high_water_mark = (1 << 21); // 2 MB
    static const int32_t default_incoming_frame_length = (1 << 19); //512 KB
    typedef std::function<int32_t(const unsigned char*, int32_t)> write_method;

    connection();
    connection(socket_connection connection, struct sockaddr_in& address);
    ~connection();

    void set_high_water_mark(int32_t high_water_mark);
    int32_t high_water_mark();
    void set_maximum_incoming_frame_length(int32_t length);
    int32_t maximum_incoming_frame_length();
    void set_user_data(void* user_data);
    void* user_data();
    void set_socket_non_blocking();
    struct sockaddr_in address();
    bool reuse_address();
    connection_state state();
    void set_state(connection_state state);
    bool closed();
    int32_t read_length();
    buffer& read_buffer();
    data_buffer& write_buffer();

    int32_t read();
    int32_t write(const std::string& buffer);
    // This is a buffered write call so long as we're under the high
    // water mark.
    int32_t write(const unsigned char* data, int32_t length);
    // This is a write call that does not buffer internally and keeps
    // trying until an error is received, or the entire specified
    // length is sent.
    int32_t do_write(const unsigned char* data, int32_t length,
        bool write_frame);
    void close();
    socket_connection& socket();
    http::ssl& ssl_context();
    bool ssl_enabled();
    bool websocket();
    void set_websocket(bool websocket);
    bool json_rpc();
    void set_json_rpc(bool json_rpc);
    // Websocket endpoints are HTTP specific endpoints such as '/'.
    const std::string& websocket_endpoint();
    void set_websocket_endpoint(const std::string endpoint);
    http::file_transfer& file_transfer();
    http::websocket_transfer& websocket_transfer();
    bool operator==(const connection& other);

  private:
    websocket_frame generate_websocket_frame(int32_t length, websocket_op code);

    void* user_data_;
    connection_state state_;
    socket_connection socket_;
    struct sockaddr_in address_;
    std::chrono::steady_clock::time_point last_active_;
    int32_t high_water_mark_;
    int32_t maximum_incoming_frame_length_;
    buffer read_buffer_;
    data_buffer write_buffer_;
    int32_t bytes_read_;
    ssl ssl_context_;
    bool websocket_;
    std::string websocket_endpoint_;
    bool json_rpc_;
    // Transfer states used for read continuations, particularly for
    // when the read_buffer_ size is too small to hold all of the
    // incoming data.
    http::file_transfer file_transfer_;
    http::websocket_transfer websocket_transfer_;
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
