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
#include <bitcoin/server/web/socket.hpp>

#include <string>
#include <utility>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace asio;
using namespace bc::chain;
using namespace bc::protocol;
using namespace boost::filesystem;
using namespace boost::iostreams;
using namespace boost::property_tree;

using role = zmq::socket::role;

// This value is based on both mongoose sample code and internal usage.
static constexpr auto max_address_length = 32u;

static constexpr auto poll_interval_milliseconds = 100u;
static constexpr auto websocket_poll_interval_milliseconds = 1u;

// The protocol message_size_limit is on the order of 1M.  The maximum
// websocket message size is set to a much smaller fraction of this
// because the supported websocket protocol does not yet support large
// message.
static constexpr auto maximum_incoming_websocket_message_size = 4096u;

static int is_websocket(socket::connection_ptr connection)
{
    BITCOIN_ASSERT(connection != nullptr);
    return (connection->flags & MG_F_IS_WEBSOCKET) != 0;
}

static std::string get_connection_address(socket::connection_ptr connection)
{
    BITCOIN_ASSERT(connection != nullptr);
    std::array<char, max_address_length> address{};
    mg_sock_addr_to_str(&connection->sa, address.data(), address.size() - 1,
        MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

    return address.data();
}

// static
// Callback made internally via socket::poll on the web socket thread.
void socket::handle_event(connection_ptr connection, int event, void* data)
{
    BITCOIN_ASSERT(connection != nullptr);
    switch (event)
    {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            auto instance = static_cast<socket*>(connection->user_data);
            BITCOIN_ASSERT(instance != nullptr);
            instance->add_connection(connection);

            LOG_VERBOSE(LOG_SERVER)
                << "Websocket client connection established ["
                << get_connection_address(connection) << "] ("
                << instance->connection_count() << ")";
            break;
        }

        case MG_EV_WEBSOCKET_FRAME:
        {
            auto instance = static_cast<socket*>(connection->user_data);
            BITCOIN_ASSERT(instance != nullptr);
            const auto message = static_cast<websocket_message*>(data);
            BITCOIN_ASSERT(message != nullptr);

            ptree input_tree;
            if (!bc::property_tree(input_tree, std::string(
                reinterpret_cast<char*>(message->data), message->size)))
            {
                std::stringstream message;
                message << "Ignoring invalid json request";
                instance->send(connection, message.str());
                LOG_DEBUG(LOG_SERVER) << message.str();
                break;
            }

            const auto sequence = input_tree.get<uint32_t>("sequence");
            const auto command = input_tree.get<std::string>("command");
            const auto arguments = input_tree.get<std::string>("arguments");
            if (command.find("query ") == std::string::npos)
            {
                std::stringstream message;
                message << "Ignoring unrecognized command: " << command;
                instance->send(connection, message.str());
                LOG_DEBUG(LOG_SERVER) << message.str();
                break;
            }

            instance->notify_query_work(connection, command, sequence,
                arguments);
            break;
        }

        case MG_EV_HTTP_REQUEST:
        {
            const auto instance = static_cast<socket*>(connection->user_data);
            BITCOIN_ASSERT(instance != nullptr);
            const auto message = static_cast<http_message*>(data);
            BITCOIN_ASSERT(message != nullptr);

            mg_serve_http(connection, message, instance->options_);
            break;
        }

        case MG_EV_CLOSE:
        {
            if (is_websocket(connection))
            {
                auto instance = static_cast<socket*>(connection->user_data);
                BITCOIN_ASSERT(instance != nullptr);
                instance->remove_connection(connection);

                LOG_VERBOSE(LOG_SERVER)
                    << "Websocket client disconnected ["
                    << get_connection_address(connection) << "] ("
                    << instance->connection_count() << ")";
            }

            break;
        }

        // Allow pass through handing of other event types.
        default:
            break;
    }
}

socket::socket(zmq::authenticator& authenticator, server_node& node,
    bool secure, const std::string& domain)
  : worker(priority(node.server_settings().priority)),
    authenticator_(authenticator),
    sequence_(0),
    secure_(secure),
    security_(secure ? "secure" : "public"),
    server_settings_(node.server_settings()),
    protocol_settings_(node.protocol_settings()),
    domain_(domain),
    root_(node.server_settings().websockets_root.generic_string()),
    options_({ root_.c_str(), nullptr, nullptr, nullptr, nullptr, "no" })
{
}

bool socket::start()
{
    if (!server_settings_.websockets_root.empty() &&
        !exists(server_settings_.websockets_root))
    {
        LOG_ERROR(LOG_SERVER)
            << "Configured HTTP root path '" << server_settings_.websockets_root
            << "' does not exist.";
        return false;
    }

    if (secure_)
    {
        if (!exists(server_settings_.websockets_server_certificate))
        {
            LOG_ERROR(LOG_SERVER)
                << "Required server certificate '"
                << server_settings_.websockets_server_certificate
                << "' does not exist.";
            return false;
        }

        if (!exists(server_settings_.websockets_server_private_key))
        {
            LOG_ERROR(LOG_SERVER)
                << "Required server private key '"
                << server_settings_.websockets_server_private_key
                << "' does not exist.";
            return false;
        }
    }

    return zmq::worker::start();
}

void socket::handle_websockets()
{
    const char* error;
    mg_bind_opts bind_options{};
    bind_options.error_string = &error;

    // These must remain in scope for the mg_bind_opt() call.
    // TODO: server_settings_.websockets_client_certificates not implemented.
    ////auto clients = server_settings_.websockets_client_certificates.generic_string();
    std::string ca;
    if (exists(server_settings_.websockets_ca_certificate))
        ca = server_settings_.websockets_ca_certificate.generic_string();
    auto key = server_settings_.websockets_server_private_key.generic_string();
    auto cert = server_settings_.websockets_server_certificate.generic_string();

    if (secure_)
    {
        bind_options.ssl_key = key.c_str();
        bind_options.ssl_cert = cert.c_str();
        if (!ca.empty())
            bind_options.ssl_ca_cert = ca.c_str();
    }

    // Initialize the mongoose websocket manager_.
    mg_mgr_init(&manager_, nullptr);
    const auto& endpoint = retrieve_websocket_endpoint();
    const auto port = std::to_string(endpoint.port());
    const auto connection = mg_bind_opt(&manager_, port.c_str(), handle_event,
        bind_options);

    if (connection == nullptr)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind listener websocket to port " << port << ": "
            << error;
        thread_status_.set_value(false);
        return;
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " " << domain_ << " websocket to port "
        << port;

    connection->user_data = static_cast<void*>(this);
    connection->recv_mbuf_limit = maximum_incoming_websocket_message_size;
    mg_set_protocol_http_websocket(connection);
    thread_status_.set_value(true);

    while (!stopped())
        poll(poll_interval_milliseconds);

    // Cleans up internal wesocket connections and state.
    mg_mgr_free(&manager_);
}

const std::shared_ptr<zmq::socket> socket::get_service() const
{
    return nullptr;
}

bool socket::start_websocket_handler()
{
    std::future<bool> status = thread_status_.get_future();
    thread_ = std::make_shared<asio::thread>(&socket::handle_websockets, this);
    status.wait();
    return status.get();
}

bool socket::stop_websocket_handler()
{
    thread_->join();
    return true;
}

const config::endpoint socket::retrieve_zeromq_connect_endpoint() const
{
    return retrieve_zeromq_endpoint().to_local();
}

void socket::poll(size_t timeout_milliseconds)
{
    auto deadline = steady_clock::now() + milliseconds(timeout_milliseconds);
    while (steady_clock::now() < deadline)
        mg_mgr_poll(&manager_, websocket_poll_interval_milliseconds);
}

size_t socket::connection_count() const
{
    return connections_.size();
}

// Called by the websocket handling thread via handle_event.
void socket::add_connection(connection_ptr connection)
{
    BITCOIN_ASSERT(connections_.find(connection) == connections_.end());
    // Initialize a new query_work_map for this connection.
    connections_[connection] = {};
}

// Called by the websocket handling thread via handle_event.
// Correlation lock usage is required because it protects the shared
// correlation map of ids, which can also used by the zmq service
// thread on response handling (i.e. query_socket::handle_query).
void socket::remove_connection(connection_ptr connection)
{
    if (connections_.empty())
        return;

    auto it = connections_.find(connection);
    if (it != connections_.end())
    {
        // Tearing down a connection is O(n) where n is the amount of
        // remaining outstanding queries.

        ////////////////////////////////////////////////////////////////////
        // Critical Section
        correlation_lock_.lock_upgrade();
        auto& query_work_map = it->second;
        for (auto query_work: query_work_map)
        {
            const auto id = query_work.second.correlation_id;
            auto correlation = correlations_.find(id);
            if (correlation != correlations_.end())
            {
                // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                correlation_lock_.unlock_upgrade_and_lock();
                correlations_.erase(correlation);
                correlation_lock_.unlock_and_lock_upgrade();
            }
        }
        correlation_lock_.unlock_upgrade();
        ////////////////////////////////////////////////////////////////////

        // Clear the query_work_map for this connection before removal.
        query_work_map.clear();
        connections_.erase(it);
    }
}

// Called by the websocket handling thread via handle_event.
// Correlation lock usage is required because it protects the shared
// correlation map of ids, which is also used by the zmq service
// thread on response handling.
void socket::notify_query_work(connection_ptr connection,
    const std::string& command, const uint32_t id,
    const std::string& arguments)
{
    BITCOIN_ASSERT(!handlers_.empty());

    const auto handler = handlers_.find(command);
    if (handler == handlers_.end())
    {
        static const std::string error = "Unrecognized query command.";
        send(connection, error);
        return;
    }

    auto it = connections_.find(connection);
    if (it == connections_.end())
    {
        LOG_ERROR(LOG_SERVER)
            << "Query work provided for unknown connection " << connection;
        return;
    }

    auto& query_work_map = it->second;
    if (query_work_map.find(id) != query_work_map.end())
    {
        static const std::string error = "Query work id is not unique.";
        send(connection, error);
        LOG_ERROR(LOG_SERVER) << error << " (" << id << ")";
        return;
    }

    query_work_map.emplace(std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(id, sequence_, connection, command, arguments));

    // Encode request based on query work and send to query_websocket.
    zmq::message request;
    handler->second.encode(request, handler->second.command, arguments,
        sequence_);

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    correlation_lock_.lock();

    // While each connection has its own id map (meaning correlation
    // ids passed from the web client are unique on a per connection
    // basis, potentially utilizing the full range available), we need
    // an internal mapping that will allow us to correlate each zmq
    // request/response pair with the connection and original id
    // number that originated it.  The client never sees this
    // sequence_ value.
    correlations_[sequence_++] = { connection, id };

    correlation_lock_.unlock();
    ///////////////////////////////////////////////////////////////////////

    const auto ec = get_service()->send(request);
    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Query send failure: " << ec.message();

        static const std::string error = "Failed to send query to service.";
        send(connection, error);
    }
}

void socket::send(connection_ptr connection, const std::string& json)
{
    mg_send_websocket_frame(connection, WEBSOCKET_OP_TEXT, json.c_str(),
        json.size());
}

void socket::broadcast(const std::string& json)
{
    auto sender = [this, &json](std::pair<connection_ptr, query_work_map> entry)
    {
        send(entry.first, json);
    };

    std::for_each(connections_.begin(), connections_.end(), sender);
}

} // namespace server
} // namespace libbitcoin
