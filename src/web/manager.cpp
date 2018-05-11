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
#include <bitcoin/server/web/manager.hpp>

#include <exception>
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
// websocket message size is set to a fraction of this, closer to 62K.
// TODO: Add "websocket_max_message_size" option to protocol.
static constexpr auto websocket_message_size_divisor = 16u;

static int is_websocket(manager::connection_ptr connection)
{
    BITCOIN_ASSERT(connection != nullptr);
    return (connection->flags & MG_F_IS_WEBSOCKET) != 0;
}

static std::string get_connection_address(manager::connection_ptr connection)
{
    BITCOIN_ASSERT(connection != nullptr);
    std::array<char, max_address_length> address{};
    mg_sock_addr_to_str(&connection->sa, address.data(), address.size() - 1,
        MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

    return address.data();
}

// static
void manager::handle_event(connection_ptr connection, int event, void* data)
{
    BITCOIN_ASSERT(connection != nullptr);
    switch (event)
    {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            auto instance = static_cast<manager*>(connection->user_data);
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
            const auto message = static_cast<websocket_message*>(data);
            BITCOIN_ASSERT(message != nullptr);

            const auto input = std::string(reinterpret_cast<char*>(
                message->data), message->size);

            // TODO: move safe input parsing to bc::property_tree.
            ptree input_tree;
            uint32_t sequence;
            std::string command;
            std::string arguments;
            auto instance = static_cast<manager*>(connection->user_data);
            BITCOIN_ASSERT(instance != nullptr);

            try
            {
                stream<array_source> input_stream(input.c_str(), input.size());
                read_json(input_stream, input_tree);

                sequence = input_tree.get<uint32_t>("sequence");
                command = input_tree.get<std::string>("command");
                arguments = input_tree.get<std::string>("arguments");
            }
            catch (const std::exception& error)
            {
                std::stringstream message;
                message << "Ignoring invalid incoming data: " << error.what();
                instance->send(connection, message.str());
                LOG_DEBUG(LOG_SERVER) << message;
                break;
            }

            if (command.find("query ") == std::string::npos)
            {
                std::stringstream message;
                message << "Ignoring unrecognized incoming data: " << command;
                instance->send(connection, message.str());
                LOG_DEBUG(LOG_SERVER) << message;
                break;
            }

            instance->notify_query_work(connection, command, sequence,
                arguments);
            break;
        }

        case MG_EV_HTTP_REQUEST:
        {
            const auto instance = static_cast<manager*>(connection->user_data);
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
                auto instance = static_cast<manager*>(connection->user_data);
                BITCOIN_ASSERT(instance != nullptr);
                instance->remove_connection(connection);

                LOG_VERBOSE(LOG_SERVER)
                    << "Websocket client disconnected ["
                    << get_connection_address(connection) << "] ("
                    << instance->connection_count() << ")";
            }

            break;
        }

        // Take no action for expected event types.
        case MG_EV_POLL:
        case MG_EV_SEND:
        case MG_EV_RECV:
        case MG_EV_WEBSOCKET_CONTROL_FRAME:
            break;

        default:
            LOG_VERBOSE(LOG_SERVER)
                << "Unrecognized web event: " << event;
            break;
    }
}

manager::manager(zmq::authenticator& authenticator, server_node& node,
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

bool manager::start()
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
                << server_settings_.websockets_server_certificate << "' does not exist.";
            return false;
        }

        if (!exists(server_settings_.websockets_server_private_key))
        {
            LOG_ERROR(LOG_SERVER)
                << "Required server private key '"
                << server_settings_.websockets_server_private_key << "' does not exist.";
            return false;
        }
    }

    return zmq::worker::start();
}

void manager::handle_websockets()
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

    // Initialize manager_.
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
    // TODO: Add "websocket_max_message_size" option to protocol settings??
    connection->recv_mbuf_limit = protocol_settings_.message_size_limit /
        websocket_message_size_divisor;
    mg_set_protocol_http_websocket(connection);

    thread_status_.set_value(true);
    while (!stopped())
        poll(poll_interval_milliseconds);

    // Cleans up internal wesocket connections and state.
    mg_mgr_free(&manager_);
}

bool manager::start_websocket_handler()
{
    std::future<bool> status = thread_status_.get_future();
    thread_ = std::make_shared<asio::thread>(&manager::handle_websockets, this);
    status.wait();
    return status.get();
}

bool manager::stop_websocket_handler()
{
    thread_->join();
    return true;
}

const config::endpoint manager::retrieve_zeromq_connect_endpoint() const
{
    // TODO: replace with simple endpoint (not string) modification utility.
    // TODO: Take apart and reconstruct endpoint, if host == "*" then replace.
    auto endpoint_string = retrieve_zeromq_endpoint().to_string();
    boost::replace_all(endpoint_string, "*", "localhost");
    return endpoint_string;
}

void manager::poll(size_t timeout_milliseconds)
{
    auto deadline = steady_clock::now() + milliseconds(timeout_milliseconds);
    while (steady_clock::now() < deadline)
        mg_mgr_poll(&manager_, websocket_poll_interval_milliseconds);
}

size_t manager::connection_count() const
{
    return connections_.size();
}

void manager::add_connection(connection_ptr connection)
{
    BITCOIN_ASSERT(connections_.find(connection) == connections_.end());
    // Initialize a new query_work_map for this connection.
    connections_[connection] = {};
}

void manager::remove_connection(connection_ptr connection)
{
    auto it = connections_.find(connection);
    if (it != connections_.end())
    {
        // TODO: tearing down a connection is still O(n) where n is
        // the amount of remaining outstanding queries

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

void manager::notify_query_work(connection_ptr connection,
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

    const auto ec = service_->send(request);

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Query send failure: " << ec.message();

        static const std::string error = "Failed to send query to service.";
        send(connection, error);
    }
}

void manager::send(connection_ptr connection, const std::string& json)
{
    mg_send_websocket_frame(connection, WEBSOCKET_OP_TEXT, json.c_str(),
        json.size());
}

void manager::broadcast(const std::string& json)
{
    auto sender = [this, &json](std::pair<connection_ptr, query_work_map> entry)
    {
        send(entry.first, json);
    };

    std::for_each(connections_.begin(), connections_.end(), sender);
}

// ZMQ service publisher handlers.
//-----------------------------------------------------------------------------

bool manager::handle_heartbeat(zmq::socket& subscriber)
{
    if (stopped())
        return false;

    zmq::message response;
    subscriber.receive(response);

    static constexpr size_t heartbeat_message_size = 2;
    if (response.empty() || response.size() != heartbeat_message_size)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling heartbeat notification: invalid data.";

        // Don't let a failure here prevent future notifications.
        return true;
    }

    uint16_t sequence;
    uint64_t height;
    response.dequeue<uint16_t>(sequence);
    response.dequeue<uint64_t>(height);

    broadcast(to_json(height, sequence));

    LOG_VERBOSE(LOG_SERVER)
        << "Sent " << security_ << " heartbeat [" << height << "]";

    return true;
}

bool manager::handle_transaction(zmq::socket& subscriber)
{
    if (stopped())
        return false;

    zmq::message response;
    subscriber.receive(response);

    static constexpr size_t transaction_message_size = 2;
    if (response.empty() || response.size() != transaction_message_size)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling transaction notification: invalid data";

        // Don't let a failure here prevent future notifications.
        return true;
    }

    uint16_t sequence;
    data_chunk transaction_data;
    response.dequeue<uint16_t>(sequence);
    response.dequeue(transaction_data);

    chain::transaction tx;
    tx.from_data(transaction_data, true, true);

    broadcast(to_json(tx, sequence));

    LOG_VERBOSE(LOG_SERVER)
        << "Sent " << security_ << " tx [" << encode_hash(tx.hash()) << "]";

    return true;
}

bool manager::handle_block(zmq::socket& subscriber)
{
    if (stopped())
        return false;

    zmq::message response;
    subscriber.receive(response);

    static constexpr size_t block_message_size = 3;
    if (response.empty() || response.size() != block_message_size)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling block notification: invalid data";

        // Don't let a failure here prevent future notifications.
        return true;
    }

    uint16_t sequence;
    uint32_t height;
    data_chunk block_data;
    response.dequeue<uint16_t>(sequence);
    response.dequeue<uint32_t>(height);
    response.dequeue(block_data);

    chain::block block;
    block.from_data(block_data);

    // Format and send transaction to websocket subscribers.
    broadcast(to_json(block, height, sequence));

    LOG_VERBOSE(LOG_SERVER)
        << "Sent " << security_ << " block [" << height << "]";
    return true;
}

// Returns true to continue future notifications.
bool manager::handle_query(zmq::socket& dealer)
{
    if (stopped())
        return false;

    zmq::message response;
    auto ec = dealer.receive(response);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to receive response from dealer: " << ec.message();
        return true;
    }

    static constexpr size_t query_message_size = 3;
    if (response.empty() || response.size() != query_message_size)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling query response: invalid data size.";
        return true;
    }

    uint32_t sequence;
    data_chunk data;
    std::string command;

    if (!response.dequeue(command) ||
        !response.dequeue<uint32_t>(sequence) ||
        !response.dequeue(data))
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling query response: invalid data parts.";
        return true;
    }

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    correlation_lock_.lock_upgrade();

    // Use internal sequence number to find connection and work id.
    auto correlation = correlations_.find(sequence);
    if (correlation == correlations_.end())
    {
        correlation_lock_.unlock_upgrade();

        // This will happen anytime the client disconnects before this
        // handler is called. We can safely discard the result here.
        LOG_DEBUG(LOG_SERVER)
            << "Unmatched websocket query work item sequence: " << sequence;
        return true;
    }

    auto connection = correlation->second.first;
    const auto id = correlation->second.second;

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    correlation_lock_.unlock_upgrade_and_lock();
    correlations_.erase(correlation);
    // TODO: Can this 2 step unlock be done atomically?
    correlation_lock_.unlock_and_lock_upgrade();
    correlation_lock_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////

    // Use connection to locate connection state.
    auto it = connections_.find(connection);
    if (it == connections_.end())
    {
        LOG_ERROR(LOG_SERVER)
            << "Query work completed for unknown connection";
        return true;
    }

    // Use work id to locate the query work item.
    auto& query_work_map = it->second;
    auto query_work = query_work_map.find(id);
    if (query_work == query_work_map.end())
    {
        // This can happen anytime the client disconnects before this
        // code is reached. We can safely discard the result here.
        LOG_DEBUG(LOG_SERVER)
            << "Unmatched websocket query work id: " << id;
        return true;
    }

    const auto work = query_work->second;
    query_work_map.erase(query_work);

    BITCOIN_ASSERT(work.id == id);
    BITCOIN_ASSERT(work.correlation_id == sequence);

    data_source istream(data);
    istream_reader source(istream);
    ec = source.read_error_code();

    if (ec)
    {
        send(work.connection, to_json(ec, id));
        return true;
    }

    const auto handler = handlers_.find(work.command);
    if (handler == handlers_.end())
    {
        static constexpr auto error = bc::error::not_implemented;
        send(work.connection, to_json(error, id));
        return true;
    }

    // Decode response and send query output to websocket client
    const auto payload = source.read_bytes();
    if (work.connection->user_data != nullptr)
        handler->second.decode(payload, id, work.connection);
    return true;
}

// Object to JSON converters.
//-----------------------------------------------------------------------------

std::string manager::to_json(uint64_t height, uint32_t sequence) const
{
    // TODO: use bc::property_tree.
    boost::property_tree::ptree property_tree;
    property_tree.put("height", height);
    property_tree.put("sequence", sequence);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const bc::chain::transaction& transaction,
    uint32_t sequence) const
{
    static constexpr auto json_encoding = true;

    auto property_tree = bc::property_tree(bc::config::transaction(
        transaction), json_encoding);
    property_tree.put("sequence", sequence);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const bc::chain::header& header,
    uint32_t sequence) const
{
    auto property_tree = bc::property_tree(bc::config::header(header));
    property_tree.put("sequence", sequence);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const bc::chain::block& block, uint32_t height,
    uint32_t sequence) const
{
    auto property_tree = bc::property_tree(bc::config::header(block.header()));
    property_tree.put("height", height);
    property_tree.put("sequence", sequence);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const std::error_code& code,
    uint32_t sequence) const
{
    // TODO: use bc::property_tree.
    boost::property_tree::ptree property_tree;
    property_tree.put("sequence", sequence);
    property_tree.put("type", "error");
    property_tree.put("message", code.message());
    property_tree.put("code", code.value());

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

} // namespace server
} // namespace libbitcoin
