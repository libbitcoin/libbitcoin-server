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
#include <memory>
#include <string>

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/web/external/mongoose/mongoose.h>

#include <boost/smart_ptr/detail/spinlock.hpp>

namespace libbitcoin {
namespace server {
using namespace bc::protocol;
using role = zmq::socket::role;
using spinlock = boost::detail::spinlock;

static constexpr auto poll_interval = 100u;
static constexpr auto websocket_poll_interval = 1u;

typedef struct mg_mgr mg_manager;
typedef struct mg_connection* mg_connection_ptr;

class server_node;

class BCS_API manager
  : public bc::protocol::zmq::worker
{
public:
    static constexpr uint32_t websocket_id_mask = (1 << 30);

    const std::string server_certificate_suffix = "server.pem";
    const std::string ca_certificate_suffix = "ca.pem";
    const std::string server_private_key_suffix = "key.pem";

    // Tracks websocket queries via the query_work_map.  Used for
    // matching websocket client requests to zmq query responses.
    struct query_work_item
    {
        // Constructor provided for in-place construction.
        query_work_item(mg_connection_ptr connection,
            const std::string& command, const std::string& arguments)
          : command(command),
            arguments(arguments),
            connection(connection)
        {
        }

        mg_connection_ptr connection;
        std::string command;
        std::string arguments;
    };

    typedef std::function<void(bc::protocol::zmq::message&, const std::string&,
        const std::string&, const uint32_t)> encode_handler;

    typedef std::function<void(const data_chunk&, mg_connection_ptr)>
        decode_handler;

    // Handles translation of incoming JSON to zmq protocol methods
    // and converting the result back to JSON for web clients.
    struct handler
    {
        std::string command;
        encode_handler encode;
        decode_handler decode;
    };

    typedef std::set<mg_connection_ptr> connection_set;
    typedef std::unordered_map<std::string, handler> handler_map;
    typedef std::unordered_map<uint32_t, query_work_item>
        query_work_sequence_map;

    /// Construct a manager class.
    manager(bc::protocol::zmq::authenticator& authenticator, server_node& node,
        bool secure, const std::string& domain);

    ~manager();

    /// Start the service.
    bool start() override;

    bool is_websocket_id(uint32_t id);
    size_t connection_count();
    void add_connection(mg_connection_ptr connection);
    void remove_connection(mg_connection_ptr connection);
    void notify_query_work(mg_connection_ptr connection,
        const std::string& command, const std::string& arguments);

protected:
    typedef bc::protocol::zmq::socket socket;

    // Send a message to the websocket client without taking a lock
    template <bool lock, typename std::enable_if<!lock>::type* = nullptr>
    void send_impl(mg_connection_ptr connection, const std::string& data)
    {
        mg_send_websocket_frame(connection, WEBSOCKET_OP_TEXT, data.c_str(),
            data.size());
    }

    // Send a message to the websocket client while taking a lock
    template <bool lock, typename std::enable_if<lock>::type* = nullptr>
    void send_impl(mg_connection_ptr connection, const std::string& data)
    {
        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        std::lock_guard<boost::detail::spinlock> guard(connection_lock_);
        // If user data is null, the connection is disconnected but we
        // haven't received the close event yet.
        if (connection->user_data != nullptr)
            mg_send_websocket_frame(connection, WEBSOCKET_OP_TEXT, data.c_str(),
                data.size());
        ///////////////////////////////////////////////////////////////////////
    }

    // Initialize the websocket event loop and start a thread to poll events.
    bool start_websocket_handler();

    // Terminate the websocket event loop.
    bool stop_websocket_handler();

    // Helper method for endpoint discovery based on domain.  If zeromq is
    // true, the zmq endpoint is returned.  If zeromq is false, the websocket
    // endpoint is returned.  If worker is true and the domain is "query", the
    // inproc communication endpoint is returned, used for synchronizing
    // communication from websocket clients to the query socket service in order
    // to avoid thread synchronized communication queues.
    const config::endpoint& retrieve_endpoint(bool zeromq, bool worker = false);

    // Calls the above method but returns endpoints in a format that
    // can be connected to directly.
    const config::endpoint retrieve_connect_endpoint(bool zeromq,
        bool worker = false);

    bool handle_block(socket& sub);
    bool handle_heartbeat(socket& sub);
    bool handle_transaction(socket& sub);
    bool handle_query(socket& dealer);

    std::string to_json(uint32_t height);
    std::string to_json(const std::error_code& code);
    std::string to_json(const bc::chain::header& header);
    std::string to_json(const bc::chain::block& block, uint32_t height);
    std::string to_json(const bc::chain::transaction& transaction);

    // Send a message to the websocket client.
    void send(mg_connection_ptr connection, const std::string& data);

    // Send a message to every connected websocket client.
    void broadcast(const std::string& data);

    // Poll websocket connections until specified timeout.
    void poll(size_t timeout_milliseconds);

    bc::protocol::zmq::authenticator& authenticator_;
    mg_manager mg_manager_;
    const bc::server::settings& settings_;

    bool websockets_ready_;
    bool secure_;
    const std::string security_;
    const bc::protocol::settings internal_;

private:
    // Threaded event-loop driver
    void handle_websockets();

    void remove_connections();
    void remove_query_work(mg_connection_ptr connection);

    uint32_t sequence_;
    connection_set connections_;
    spinlock connection_lock_;
    std::mutex query_work_map_lock_;
    query_work_sequence_map query_work_map_;

    handler_map translator_;
    const std::string domain_;

    std::string server_certificate_;
    std::string server_private_key_;
    std::string ca_certificate_;

    std::shared_ptr<socket> query_sender_;
    std::shared_ptr<std::thread> websocket_thread_;
};

} // namespace server
} // namespace libbitcoin

#endif
