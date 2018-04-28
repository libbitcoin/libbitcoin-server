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

// TODO: include boost spinlock header.
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>

extern "C"
{

// Required implementation provided for mbedtls random data usage.
int mg_ssl_if_mbed_random(void* connection, uint8_t* buffer, size_t length)
{
    bc::data_chunk data;
    data.reserve(length);
    bc::pseudo_random_fill(data);
    std::memcpy(buffer, data.data(), data.size());
    return 0;
}

}

namespace libbitcoin {
namespace server {

using namespace asio;
using namespace bc::chain;
using namespace bc::protocol;
using namespace boost::filesystem;
using namespace boost::iostreams;
using namespace boost::property_tree;

using role = zmq::socket::role;

// TODO: eliminate use of static state, use mamber and pass by parameter.
mg_serve_http_opts websocket_options;

// TODO: where does this magic number come from?
static constexpr auto max_address_length = 32u;

static constexpr auto poll_interval_milliseconds = 100u;
static constexpr auto websocket_poll_interval_milliseconds = 1u;
static constexpr uint32_t websocket_id_mask = (1 << 30);

static int is_websocket(manager::mg_connection_ptr connection)
{
    return (connection->flags & MG_F_IS_WEBSOCKET) != 0;
}

static std::string get_connection_address(manager::mg_connection_ptr connection)
{
    std::array<char, max_address_length> address{};
    mg_sock_addr_to_str(&connection->sa, address.data(), address.size() - 1,
        MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

    return address.data();
}

// This is called with the connection_spinner_ held across mg_mgr_poll method.
void handle(manager::mg_connection_ptr connection, int event, void* data)
{
    switch (event)
    {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            auto instance = static_cast<manager*>(connection->user_data);
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
            const auto input = std::string(reinterpret_cast<char*>(
                message->data), message->size);

            // TODO: move safe input parsing to bc::property_tree.
            ptree input_tree;

            try
            {
                stream<array_source> input_stream(input.c_str(), input.size());
                read_json(input_stream, input_tree);
            }
            catch (std::exception& error)
            {
                LOG_DEBUG(LOG_SERVER)
                    << "Ignoring invalid incoming data " << error.what();
                break;
            }

            const auto command = input_tree.get<std::string>("command");
            if (command.find("query ") == std::string::npos)
            {
                LOG_DEBUG(LOG_SERVER)
                    << "Ignoring unrecognized incoming data: " << command;
                break;
            }

            const auto arguments = input_tree.get<std::string>("arguments");
            auto instance = static_cast<manager*>(connection->user_data);
            instance->notify_query_work(connection, command, arguments);
            break;
        }

        case MG_EV_HTTP_REQUEST:
        {
            const auto message = static_cast<struct http_message*>(data);
            mg_serve_http(connection, message, websocket_options);
            break;
        }

        case MG_EV_CLOSE:
        {
            if (is_websocket(connection))
            {
                auto instance = static_cast<manager*>(connection->user_data);
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
    mg_manager_{},
    authenticator_(authenticator),
    secure_(secure),
    security_(secure_ ? "secure" : "public"),
    external_(node.server_settings()),
    internal_(node.protocol_settings()),
    sequence_(websocket_id_mask),
    domain_(domain),
    root_(external_.websockets_root.generic_string()),
    ca_certificate_(external_.websockets_ca_certificate.generic_string()),
    server_private_key_(external_.websockets_server_private_key.generic_string()),
    server_certificate_(external_.websockets_server_certificate.generic_string()),
    client_certificates_(external_.websockets_client_certificates.generic_string())
{
    // JSON to ZMQ request encoders.
    //-------------------------------------------------------------------------

    const auto encode_empty = [](zmq::message& request,
        const std::string& command, const std::string& arguments, uint32_t id)
    {
        request.enqueue(command);
        request.enqueue_little_endian(id);
        request.enqueue(data_chunk{});
    };

    const auto encode_hash = [](zmq::message& request,
        const std::string& command, const std::string& arguments, uint32_t id)
    {
        hash_digest hash;
        DEBUG_ONLY(const auto result =) decode_hash(hash, arguments);
        BITCOIN_ASSERT(result);
        request.enqueue(command);
        request.enqueue_little_endian(id);
        request.enqueue(to_chunk(hash));
    };

    // JSON to ZMQ response decoders.
    //-------------------------------------------------------------------------
    const auto decode_height = [this](const data_chunk& data,
        mg_connection_ptr connection)
    {
        data_source istream(data);
        istream_reader source(istream);
        const auto height = source.read_4_bytes_little_endian();
        send_locked(connection, to_json(height));
    };

    const auto decode_transaction = [this](const data_chunk& data,
        mg_connection_ptr connection)
    {
        bc::chain::transaction transaction;
        transaction.from_data(data, true, true);
        send_locked(connection, to_json(transaction));
    };

    const auto decode_block_header = [this](const data_chunk& data,
        mg_connection_ptr connection)
    {
        bc::chain::header header;
        header.from_data(data, true);
        send_locked(connection, to_json(header));
    };

    handlers_["query fetch-height"] = handler
    {
        "blockchain.fetch_last_height",
        encode_empty,
        decode_height
    };

    handlers_["query fetch-tx"] = handler
    {
        "transaction_pool.fetch_transaction2",
        encode_hash,
        decode_transaction
    };

    handlers_["query fetch-header"] = handler
    {
        "blockchain.fetch_block_header",
        encode_hash,
        decode_block_header
    };
}

bool manager::start()
{
    if (!external_.websockets_root.empty() &&
        !exists(external_.websockets_root))
    {
        LOG_ERROR(LOG_SERVER)
            << "Configured HTTP root path '" << external_.websockets_root
            << "' does not exist.";
        return false;
    }

    if (secure_)
    {
        if (!exists(external_.websockets_server_certificate))
        {
            LOG_ERROR(LOG_SERVER)
                << "Required server certificate '"
                << external_.websockets_server_certificate << "' does not exist.";
            return false;
        }

        if (!exists(external_.websockets_server_private_key))
        {
            LOG_ERROR(LOG_SERVER)
                << "Required server private key '"
                << external_.websockets_server_private_key << "' does not exist.";
            return false;
        }
    }

    return zmq::worker::start();
}

bool manager::start_websocket_handler()
{
    thread_ = std::make_shared<asio::thread>(&manager::handle_websockets, this);
    return true;
}

bool manager::stop_websocket_handler()
{
    thread_->join();
    return true;
}

// TODO: external_.websockets_client_certificates not implemented.
void manager::handle_websockets()
{
    if (domain_ == "query")
    {
        // A zmq socket must remain on its single thread.
        service_ = std::make_shared<socket>(authenticator_, role::pair, internal_);
        const auto ec = service_->connect(retrieve_endpoint(true, true));

        if (ec)
        {
            // BUGBUG: This startup failure will not prevent the server startup.
            LOG_ERROR(LOG_SERVER)
                << "Failed to connect " << security_ << " query sender socket: "
                << ec.message();
            return;
        }
    }

    mg_bind_opts bind_options{};
    mg_mgr_init(&mg_manager_, nullptr);
    const auto& endpoint = retrieve_endpoint(false);
    const auto port = std::to_string(endpoint.port());

    if (secure_)
    {
        bind_options.ssl_key = server_private_key_.c_str();
        bind_options.ssl_cert = server_certificate_.c_str();

        if (!ca_certificate_.empty())
            bind_options.ssl_ca_cert = ca_certificate_.c_str();
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " " << domain_
        << " websocket to port " << port;

    const char* error;
    bind_options.error_string = &error;
    const auto connection = mg_bind_opt(&mg_manager_, port.c_str(), handle, bind_options);

    if (connection == nullptr)
    {
        // BUGBUG: This startup failure will not prevent the server startup.
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind listener websocket to port " << port << ": "
            << error;
        return;
    }

    connection->user_data = static_cast<void*>(this);
    mg_set_protocol_http_websocket(connection);
    websocket_options.document_root = root_.c_str();
    websocket_options.enable_directory_listing = "no";

    while (!stopped())
        poll(poll_interval_milliseconds);

    if (domain_ == "query" && !service_->stop())
    {
        // BUGBUG: This startup failure will not prevent the server startup.
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query sender.";
    }

    remove_connections();
    mg_mgr_free(&mg_manager_);
}

const config::endpoint& manager::retrieve_endpoint(bool zeromq, bool worker)
{
    static const config::endpoint null_endpoint{};
    static const config::endpoint public_query("inproc://public_query_websockets");
    static const config::endpoint secure_query("inproc://secure_query_websockets");

    if (domain_ == "query")
    {
        if (zeromq && worker)
            return (secure_ ? secure_query : public_query);

        return (zeromq ? external_.websockets_query_endpoint(secure_) :
            external_.websockets_query_endpoint(secure_));
    }

    if (domain_ == "block")
        return (zeromq ? external_.websockets_block_endpoint(secure_) :
            external_.websockets_block_endpoint(secure_));

    if (domain_ == "heartbeat")
        return (zeromq ? external_.websockets_heartbeat_endpoint(secure_) :
            external_.websockets_heartbeat_endpoint(secure_));

    if (domain_ == "transaction")
        return (zeromq ? external_.websockets_transaction_endpoint(secure_) :
            external_.websockets_transaction_endpoint(secure_));

    return null_endpoint;
}

const config::endpoint manager::retrieve_connect_endpoint(bool zeromq,
    bool worker)
{
    auto endpoint_string = retrieve_endpoint(zeromq, worker).to_string();
    boost::replace_all(endpoint_string, "*", "localhost");
    return config::endpoint(endpoint_string);
}

void manager::poll(size_t timeout_milliseconds)
{
    auto deadline = steady_clock::now() + milliseconds(timeout_milliseconds);

    while (steady_clock::now() < deadline)
    {
        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        connection_spinner_.lock();
        mg_mgr_poll(&mg_manager_, websocket_poll_interval_milliseconds);
        connection_spinner_.unlock();
        ///////////////////////////////////////////////////////////////////////
    }
}

size_t manager::connection_count()
{
    return connections_.size();
}

bool manager::is_websocket_id(uint32_t id)
{
    return (id & websocket_id_mask) != 0;
}

void manager::add_connection(mg_connection_ptr connection)
{
    // TODO: is this id wraparound safe?
    // TODO: C++17, verify with VS2013(CTP).
    connections_.insert(connection);
}

void manager::remove_connection(mg_connection_ptr connection)
{
    auto it = connections_.find(connection);
    if (it != connections_.end())
    {
        // TODO: portability??
        closesocket(connection->sock);
        connections_.erase(it);
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    query_work_map_mutex_.lock_upgrade();

    // TODO: try to make constant time, under lock makes this worse.
    for (auto it = query_work_map_.begin(); it != query_work_map_.end();)
    {
        if (it->second.connection == connection)
        {
            query_work_map_mutex_.unlock_upgrade_and_lock();
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            it = query_work_map_.erase(it);
            //-----------------------------------------------------------------
            query_work_map_mutex_.unlock_and_lock_upgrade();
        }
        else
        {
            ++it;
        }
    }

    query_work_map_mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////
}

void manager::remove_connections()
{
    // TODO: portability??
    for (auto connection: connections_)
        closesocket(connection->sock);

    connections_.clear();

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    query_work_map_mutex_.lock();
    query_work_map_.clear();
    query_work_map_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

// This is called with the connection_lock_ held across the mg_mgr_poll method.
void manager::notify_query_work(mg_connection_ptr connection,
    const std::string& command, const std::string& arguments)
{
    const auto handler = handlers_.find(command);
    if (handler == handlers_.end())
    {
        static const std::string error = "Unrecognized query command";
        send_unlocked(connection, error);
        return;
    }

    // TODO: why does the sequence number start with the websocket_id_mask?
    // Wrap sequence number if needed.
    if (++sequence_ == max_uint32)
        sequence_ = websocket_id_mask;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    query_work_map_mutex_.lock();

    // TODO: why is the work map protected but not the sequence??
    query_work_map_.emplace(std::piecewise_construct,
        std::forward_as_tuple(sequence_),
        std::forward_as_tuple(connection, command, arguments));

    query_work_map_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // Encode request based on query work and send to query_websocket.
    zmq::message request;
    handler->second.encode(request, handler->second.command, arguments,
        sequence_);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////

    // TODO: requires more explanation.
    // ZMQ pair sockets block when at the exceptional state of high water
    // mark. To avoid backlogging futher, we drop the connection lock so
    // that the websocket handling can continue during that time.

    // BUGBUG: a given socket may only operate on a single thread.
    connection_spinner_.unlock();
    const auto ec = service_->send(request);
    connection_spinner_.lock();
    ///////////////////////////////////////////////////////////////////////////

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Query send failure: " << ec.message();

        static const std::string error = "Failed to send query to service";
        send_unlocked(connection, error);
    }
}

void manager::send_locked(mg_connection_ptr connection, const std::string& data)
{
    using namespace boost::detail;

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    std::lock_guard<spinlock> guard(connection_spinner_);

    // If user data is null, the connection is disconnected but we haven't
    // received the close event yet.
    if (connection->user_data != nullptr)
        send_unlocked(connection, data);
    ///////////////////////////////////////////////////////////////////////
}

void manager::send_unlocked(mg_connection_ptr connection,
    const std::string& data)
{
    mg_send_websocket_frame(connection, WEBSOCKET_OP_TEXT, data.c_str(),
        data.size());
}

void manager::broadcast(const std::string& data)
{
    using namespace boost::detail;

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    std::lock_guard<spinlock> guard(connection_spinner_);

    // TODO: must this be sequential?
    for (auto connection: connections_)
        if (connection->user_data != nullptr)
            send_unlocked(connection, data);
    ///////////////////////////////////////////////////////////////////////////
}

// ZMQ service publisher handlers.
//-----------------------------------------------------------------------------

bool manager::handle_heartbeat(zmq::socket& sub)
{
    if (stopped())
        return false;

    zmq::message response;
    sub.receive(response);

    static constexpr size_t heartbeat_message_size = 2;
    if (response.empty() || response.size() != heartbeat_message_size)
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling heartbeat notification: invalid data";

        // Don't let a failure here prevent future notifications.
        return true;
    }

    uint16_t sequence;
    uint64_t height;
    response.dequeue<uint16_t>(sequence);
    response.dequeue<uint64_t>(height);

    // TODO: why is there a type mismatch here?
    ////BITCOIN_ASSERT(height < max_int64);

    // BUGBUG: sequence has been lost.
    // Format and send heartbeat to websocket subscribers.
    broadcast(to_json(static_cast<uint32_t>(height)));

    LOG_VERBOSE(LOG_SERVER)
        << "Sent " << security_ << " heartbeat [" << height << "]";

    return true;
}

bool manager::handle_transaction(zmq::socket& sub)
{
    if (stopped())
        return false;

    zmq::message response;
    sub.receive(response);

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

    // BUGBUG: sequence has been lost.
    // Format and send transaction to websocket subscribers.
    broadcast(to_json(tx));

    LOG_VERBOSE(LOG_SERVER)
        << "Sent " << security_ << " tx [" << encode_hash(tx.hash()) << "]";

    return true;
}

bool manager::handle_block(zmq::socket& sub)
{
    if (stopped())
        return false;

    zmq::message response;
    sub.receive(response);

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

    bc::chain::block block;
    block.from_data(block_data);

    // BUGBUG: sequence has been lost.
    // Format and send transaction to websocket subscribers.
    broadcast(to_json(block, height));

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

    uint32_t id;
    data_chunk data;
    std::string command;

    if (!response.dequeue(command) ||
        !response.dequeue<uint32_t>(id) ||
        !response.dequeue(data))
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling query response: invalid data parts.";
        return true;
    }

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    query_work_map_mutex_.lock_upgrade();

    // BUGBUG: matching user data across users.
    auto it = query_work_map_.find(id);
    if (it == query_work_map_.end())
    {
        query_work_map_mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        LOG_WARNING(LOG_SERVER)
            << "Unmatched websocket query work item id: " << id;
        return true;
    }

    const auto work = it->second;
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    query_work_map_mutex_.unlock_upgrade_and_lock();
    query_work_map_.erase(it);
    query_work_map_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    data_source istream(data);
    istream_reader source(istream);
    ec = source.read_error_code();

    if (ec)
    {
        send_locked(work.connection, to_json(ec));
        return true;
    }

    const auto handler = handlers_.find(work.command);
    if (handler == handlers_.end())
    {
        static constexpr auto error = bc::error::not_implemented;
        send_locked(work.connection, to_json(error));
        return true;
    }

    // Decode response and send query output to websocket client.
    const auto payload = source.read_bytes();
    handler->second.decode(payload, work.connection);
    return true;
}

// Object to JSON converters.
//-----------------------------------------------------------------------------

std::string manager::to_json(uint32_t height)
{
    // TODO: use bc::property_tree.
    boost::property_tree::ptree property_tree;
    property_tree.put("height", height);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const bc::chain::transaction& transaction)
{
    static constexpr auto json_encoding = true;

    const auto property_tree = bc::property_tree(bc::config::transaction(
        transaction), json_encoding);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const bc::chain::header& header)
{
    auto property_tree = bc::property_tree(bc::config::header(header));

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const bc::chain::block& block, uint32_t height)
{
    auto property_tree = bc::property_tree(bc::config::header(block.header()));
    property_tree.put("height", height);

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

std::string manager::to_json(const std::error_code& code)
{
    // TODO: use bc::property_tree.
    boost::property_tree::ptree property_tree;

    property_tree.put("type", "error");
    property_tree.put("message", code.message());
    property_tree.put("code", code.value());

    std::stringstream json_stream;
    write_json(json_stream, property_tree);
    return json_stream.str();
}

} // namespace server
} // namespace libbitcoin
