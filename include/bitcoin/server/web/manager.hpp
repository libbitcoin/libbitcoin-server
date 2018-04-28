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
#include <boost/smart_ptr/detail/spinlock.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/messages/message.hpp>

// TODO: avoid public exposure of thie header (move types to cpp).
#include <bitcoin/server/web/external/mongoose/mongoose.h>

namespace libbitcoin {
namespace server {

class server_node;

// TODO: bc::server::manager is ambiguous name.
class BCS_API manager
  : public bc::protocol::zmq::worker
{
public:
    // TODO: avoid exposing a term such as "mg" on public types. 
    typedef struct mg_mgr mg_manager;

    // TODO: Avoid use of dumb pointers.
    typedef struct mg_connection* mg_connection_ptr;

    // Tracks websocket queries via the query_work_map.  Used for
    // matching websocket client requests to zmq query responses.
    struct query_work_item
    {
        // TODO: is a string really how we want to collect arguments?
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

    // Handles translation of incoming JSON to zmq protocol methods and
    // converting the result back to JSON for web clients.
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

    // Initialize the websocket event loop and start a thread to poll events.
    bool start_websocket_handler();

    // Terminate the websocket event loop.
    bool stop_websocket_handler();

    // Helper method for endpoint discovery based on domain. If zeromq is
    // true, the zmq endpoint is returned. If zeromq is false, the websocket
    // endpoint is returned. If worker is true and the domain is "query", the
    // inproc communication endpoint is returned, used for synchronizing
    // communication from websocket clients to the query socket service in order
    // to avoid thread synchronized communication queues.
    const config::endpoint& retrieve_endpoint(bool zeromq, bool worker=false);

    // Calls the above method but returns endpoints in a format that
    // can be connected to directly.
    const config::endpoint retrieve_connect_endpoint(bool zeromq,
        bool worker=false);

    bool handle_block(socket& sub);
    bool handle_heartbeat(socket& sub);
    bool handle_transaction(socket& sub);
    bool handle_query(socket& dealer);

    std::string to_json(uint32_t height);
    std::string to_json(const std::error_code& code);
    std::string to_json(const chain::header& header);
    std::string to_json(const chain::block& block, uint32_t height);
    std::string to_json(const chain::transaction& transaction);

    // TODO: why is data a string?
    // Send a message to the websocket client while taking a lock.
    void send_locked(mg_connection_ptr connection, const std::string& data);  
    
    // TODO: why is data a string?
    // Send a message to the websocket client without taking a lock.
    void send_unlocked(mg_connection_ptr connection, const std::string& data);

    // TODO: why is data a string?
    // Send a message to every connected websocket client.
    void broadcast(const std::string& data);

    // TODO: why must we poll connections?
    // Poll websocket connections until specified timeout.
    void poll(size_t timeout_milliseconds);

    // These are protected by?
    mg_manager mg_manager_;
    bc::protocol::zmq::authenticator& authenticator_;

    const bool secure_;
    const std::string security_;
    const bc::server::settings& external_;
    const bc::protocol::settings& internal_;

private:
    // Threaded event-loop driver.
    void handle_websockets();
    void remove_connections();

    // BUGBUG: sequence_ is unprotected.
    uint32_t sequence_;

    // TODO: it's not clear to me how this is working.
    connection_set connections_;
    boost::detail::spinlock connection_spinner_;

    // This is protected by mutex.
    query_work_sequence_map query_work_map_;
    upgrade_mutex query_work_map_mutex_;

    // handlers_ is effectively const (safe/okay).
    handler_map handlers_;

    // The internal socket must operate on only this one thread.
    std::shared_ptr<socket> service_;
    std::shared_ptr<asio::thread> thread_;

    const std::string domain_;
    const std::string root_;
    const std::string ca_certificate_;
    const std::string server_private_key_;
    const std::string server_certificate_;
    const std::string client_certificates_;
};

} // namespace server
} // namespace libbitcoin

#endif
