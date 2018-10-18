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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_MANAGER_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_MANAGER_HPP

#include "connection.hpp"

namespace libbitcoin {
namespace server {
namespace http {

class manager
{
  public:
    class task
    {
      public:
        virtual ~task() = default;
        virtual bool run() = 0;
        virtual connection_ptr& connection() = 0;
    };

    typedef std::vector<std::shared_ptr<task>> task_list;

    explicit manager(bool ssl, event_handler handler,
        boost::filesystem::path document_root);
    ~manager();

    // Required on the Windows platform.
    bool initialize();

    size_t maximum_incoming_frame_length();
    void set_maximum_incoming_frame_length(size_t length);

    // May invalidate any buffered write data on each connection.
    size_t high_water_mark();
    void set_high_water_mark(size_t length);

    void set_backlog(int32_t backlog);
    bool bind(std::string hostname, uint16_t port, const bind_options& options);
    bool accept_connection();
    void add_connection(const connection_ptr& connection);
    void remove_connection(connection_ptr& connection);
    size_t connection_count();
    bool ssl();
    bool listening();

    void start();
    void stop();
    bool stopped();
    void execute(std::shared_ptr<task> task);
    void run_tasks();

    void poll(size_t timeout_milliseconds);
    bool handle_connection(connection_ptr& connection, event current_event);

  private:
#ifdef WITH_MBEDTLS
    // Passed to mbedtls for internal use only.
    static int ssl_send(void* data, const uint8_t* buffer, size_t length);
    static int ssl_receive(void* data, uint8_t* buffer, size_t length);
#endif

    void run_once(size_t timeout_milliseconds);
    void select(size_t timeout_milliseconds, connection_list& sockets);
    bool transfer_file_data(connection_ptr& connection);
    bool send_http_file(connection_ptr& connection, const std::string& path,
        bool keep_alive);
    bool handle_websocket(connection_ptr& connection);
    bool send_response(connection_ptr& connection, const http_request& request);
    bool send_generated_reply(connection_ptr& connection,
        protocol_status status);
    bool upgrade_connection(connection_ptr& connection,
        const http_request& request);
    bool validate_origin(const std::string origin);
    bool initialize_ssl(connection_ptr& connection, bool listener);
    void remove_connections();

    bool ssl_;
    bool listening_;
    void* user_data_;
    bool running_;
    std::string key_;
    std::string certificate_;
    std::string ca_certificate_;
    event_handler handler_;
    boost::filesystem::path document_root_;
    connection_list connections_;
    size_t maximum_incoming_frame_length_;
    size_t high_water_mark_;
    int32_t backlog_;
    connection_ptr listener_;
    struct sockaddr_in listener_address_;
    task_list tasks_;
    std::mutex task_lock_;
    uint16_t port_;
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
