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
#ifndef LIBBITCOIN_SERVER_WEB_MANAGER_HPP
#define LIBBITCOIN_SERVER_WEB_MANAGER_HPP

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/messages/message.hpp>

// TODO: avoid public exposure of this header (move types to cpp).
#include <bitcoin/server/web/external/mongoose/mongoose.h>

namespace libbitcoin {
namespace server {

class server_node;

// TODO: bc::server::manager is ambiguous name.
// RESP: We're in the subdirectory of web, so web_manager seemed redundant.  Rename to socket_manager?
class BCS_API manager
  : public bc::protocol::zmq::worker
{
public:
    // TODO: Avoid use of dumb pointers.
    typedef struct mg_connection* connection_ptr;

    // Tracks websocket queries via the query_work_map.  Used for
    // matching websocket client requests to zmq query responses.
    struct query_work_item
    {
        // Constructor provided for in-place construction.
        query_work_item(uint32_t id, uint32_t correlation_id,
            connection_ptr connection, const std::string& command,
            const std::string& arguments)
        :   id(id),
            correlation_id(correlation_id),
            command(command),
            arguments(arguments),
            connection(connection)
        {
        }

        uint32_t id;
        uint32_t correlation_id;
        std::string command;
        std::string arguments;
        connection_ptr connection;
    };

    typedef std::function<void(bc::protocol::zmq::message&, const std::string&,
        const std::string&, const uint32_t)> encode_handler;

    typedef std::function<void(const data_chunk&, const uint32_t,
        connection_ptr)> decode_handler;

    // Handles translation of incoming JSON to zmq protocol methods and
    // converting the result back to JSON for web clients.
    struct handlers
    {
        std::string command;
        encode_handler encode;
        decode_handler decode;
    };

    typedef std::unordered_map<uint32_t, query_work_item> query_work_map;
    typedef std::unordered_map<connection_ptr, query_work_map>
        connection_work_map;
    typedef std::unordered_map<uint32_t, std::pair<connection_ptr, uint32_t>>
        query_correlation_map;
    typedef std::unordered_map<std::string, handlers> handler_map;

    /// Construct a manager class.
    manager(bc::protocol::zmq::authenticator& authenticator, server_node& node,
        bool secure, const std::string& domain);

    /// Start the service.
    bool start() override;

    size_t connection_count() const;
    void add_connection(connection_ptr connection);
    void remove_connection(connection_ptr connection);
    void notify_query_work(connection_ptr connection,
        const std::string& command, const uint32_t sequence,
        const std::string& arguments);

protected:
    typedef struct mg_mgr connection_manager;
    typedef bc::protocol::zmq::socket socket;

    // Initialize the websocket event loop and start a thread to poll events.
    virtual bool start_websocket_handler();

    // Terminate the websocket event loop.
    virtual bool stop_websocket_handler();

    virtual void handle_websockets();

    virtual const config::endpoint& retrieve_zeromq_endpoint() const = 0;
    virtual const config::endpoint& retrieve_websocket_endpoint() const = 0;

    bool handle_block(socket& subscriber);
    bool handle_heartbeat(socket& subscriber);
    bool handle_transaction(socket& subscriber);
    bool handle_query(socket& dealer);

    std::string to_json(uint64_t height, uint32_t sequence) const;
    std::string to_json(const std::error_code& code, uint32_t sequence) const;
    std::string to_json(const chain::header& header, uint32_t sequence) const;
    std::string to_json(const chain::block& block, uint32_t height,
         uint32_t sequence) const;
    std::string to_json(const chain::transaction& transaction,
         uint32_t sequence) const;

    // Send a message to the websocket client.
    void send(connection_ptr connection, const std::string& json);

    // Send a message to every connected websocket client.
    void broadcast(const std::string& json);

    // Poll websocket connections until specified timeout.
    void poll(size_t timeout_milliseconds);

    // Retrieve the appropriate zeromq override endpoint and return a
    // connectable endpoint (as opposed to a bindable endpoint).
    const config::endpoint retrieve_zeromq_connect_endpoint() const;

    // The zmq socket operates on only this one thread.
    bc::protocol::zmq::authenticator& authenticator_;
    const bool secure_;
    const std::string security_;
    const bc::server::settings& server_settings_;
    const bc::protocol::settings& protocol_settings_;
    // handlers_ is effectively const.
    handler_map handlers_;

    // The service_ socket operates on only the below member thread_.
    std::shared_ptr<socket> service_;
    std::shared_ptr<asio::thread> thread_;

private:
    static void handle_event(connection_ptr connection, int event, void* data);

    connection_manager manager_;

    uint32_t sequence_;
    connection_work_map connections_;
    query_correlation_map correlations_;

    mutable upgrade_mutex correlation_lock_;

    const std::string domain_;
    const std::string root_;
    const mg_serve_http_opts options_;
};

} // namespace server
} // namespace libbitcoin

#endif
