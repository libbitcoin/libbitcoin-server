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

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

extern "C"
{

// Required implementation provided for mbedtls random data usage.
// Return 0 on success.
int mg_ssl_if_mbed_random(void* connection, unsigned char* buffer,
    size_t length)
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

struct mg_serve_http_opts websocket_options;

static int is_websocket(const mg_connection_ptr connection)
{
    return connection->flags & MG_F_IS_WEBSOCKET;
}

static std::string get_connection_address(mg_connection_ptr connection)
{
    static constexpr size_t max_address_length = 32;
    std::array<char, max_address_length> address{};

    mg_sock_addr_to_str(&connection->sa, address.data(), address.size() - 1,
        MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

    return std::string(address.data());
}

// Note that this is called with the connection_lock_ held across the
// mg_mgr_poll method.
void event_handler(mg_connection_ptr connection, int event, void* data)
{
    switch (event)
    {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            auto manager = static_cast<bc::server::manager*>(
                connection->user_data);
            manager->add_connection(connection);

            LOG_VERBOSE(LOG_SERVER)
                << "Websocket client connection established ["
                << get_connection_address(connection) << "] ("
                << manager->connection_count() << ")";
            break;
        }

        case MG_EV_WEBSOCKET_FRAME:
        {
            const auto message = static_cast<struct websocket_message*>(data);
            const auto input = std::string(reinterpret_cast<char*>(
                message->data), message->size);
            boost::property_tree::ptree input_tree;

            try
            {
                boost::iostreams::stream<
                    boost::iostreams::array_source> input_stream(
                        input.c_str(), input.size());
                boost::property_tree::read_json(input_stream, input_tree);
            }
            catch (std::exception& error)
            {
                LOG_DEBUG(LOG_SERVER)
                    << "Ignoring invalid incoming data " << error.what();
                break;
            }

            const auto command = input_tree.get<std::string>("command");
            if (command.find("query ") != std::string::npos)
            {
                const auto arguments = input_tree.get<std::string>("arguments");
                auto manager = static_cast<bc::server::manager*>(
                    connection->user_data);
                manager->notify_query_work(connection, command, arguments);
            }
            else
            {
                LOG_DEBUG(LOG_SERVER)
                    << "Ignoring unrecognized incoming data: "
                    << command;
            }

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
                auto manager = static_cast<libbitcoin::server::manager*>(
                    connection->user_data);
                manager->remove_connection(connection);

                LOG_VERBOSE(LOG_SERVER)
                    << "Websocket client disconnected ["
                    << get_connection_address(connection) << "] ("
                    << manager->connection_count() << ")";
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


manager::manager(zmq::authenticator& authenticator,
    server_node& node, bool secure, const std::string& domain)
  : worker(priority(node.server_settings().priority)),
    authenticator_(authenticator),
    mg_manager_{},
    settings_(node.server_settings()),
    sequence_(websocket_id_mask),
    secure_(secure),
    security_(secure_ ? "secure" : "public"),
    domain_(domain),
    internal_(node.protocol_settings().send_high_water,
        node.protocol_settings().receive_high_water),
    websockets_ready_(false),
    connection_lock_{},
    query_work_map_lock_{},
    ca_certificate_{}
{
    // JSON to ZMQ request encoders.
    //-----------------------------------------------------------------------------

    const auto encode_empty = [](zmq::message& request,
        const std::string& command, const std::string& arguments,
            uint32_t id)
    {
        data_chunk empty_data;
        request.enqueue(command);
        request.enqueue_little_endian(id);
        request.enqueue(empty_data);
    };

    const auto encode_hash = [](zmq::message& request,
        const std::string& command, const std::string& arguments, 
            uint32_t id)
    {
        hash_digest hash;
        decode_hash(hash, arguments);
        request.enqueue(command);
        request.enqueue_little_endian(id);
        request.enqueue(to_chunk(hash));
    };

    // JSON to ZMQ response decoders.
    //-----------------------------------------------------------------------------
    const auto decode_height = [this](const data_chunk& data,
        mg_connection_ptr connection)
    {
        data_source istream(data);
        istream_reader source(istream);
        const auto height = source.read_4_bytes_little_endian();
        send(connection, to_json(height));
    };

    const auto decode_transaction = [this](const data_chunk& data,
        mg_connection_ptr connection)
    {
        bc::chain::transaction transaction;
        transaction.from_data(data, true, true);
        send(connection, to_json(transaction));
    };

    const auto decode_block_header = [this](const data_chunk& data,
        mg_connection_ptr connection)
    {
        bc::chain::header header;
        header.from_data(data, true);
        send(connection, to_json(header));
    };

    translator_["query fetch-height"] = handler
    {
        "blockchain.fetch_last_height",
        encode_empty,
        decode_height
    };

    translator_["query fetch-tx"] = handler
    {
        "transaction_pool.fetch_transaction2",
        encode_hash,
        decode_transaction
    };

    translator_["query fetch-header"] = handler
    {
        "blockchain.fetch_block_header",
        encode_hash,
        decode_block_header
    };
}

manager::~manager()
{
    remove_connections();
    mg_mgr_free(&mg_manager_);
}

bool manager::start()
{
    if (!settings_.websockets_root_path.empty() &&
        !exists(settings_.websockets_root_path))
    {
        LOG_ERROR(LOG_SERVER)
            << "Root path '" << settings_.websockets_root_path
            << "' does not exist.";
        return false;
    }

    if (secure_ && settings_.websockets_server_certificates.empty())
    {
        LOG_ERROR(LOG_SERVER)
            << "Required SSL certificate and key is not specified.";
        return false;
    }

    const auto certificate_path = path(
        settings_.websockets_server_certificates);

    auto server_certificate = certificate_path;
    server_certificate /= server_certificate_suffix;
    server_certificate_ = server_certificate.native();

    if (secure_ && !exists(server_certificate_))
    {
        LOG_ERROR(LOG_SERVER)
            << "Required Server Certificate '" << server_certificate_
            << "' does not exist.";
        return false;
    }

    auto ca_certificate = certificate_path;
    ca_certificate /= ca_certificate_suffix;

    if (secure_ && exists(ca_certificate))
        ca_certificate_ = ca_certificate.native();

    auto server_private_key = certificate_path;
    server_private_key /= server_private_key_suffix;
    server_private_key_ = server_private_key.native();

    if (secure_ && !exists(server_private_key_))
    {
        LOG_ERROR(LOG_SERVER)
            << "Required Server Private Key '" << server_private_key_
            << "' does not exist.";
        return false;
    }

    return zmq::worker::start();
}

bool manager::start_websocket_handler()
{
    if (domain_ == "query")
    {
        query_sender_ = std::make_shared<socket>(authenticator_, role::pair,
            internal_);
        const auto ec = query_sender_->connect(retrieve_endpoint(true, true));

        if (ec)
        {
            LOG_ERROR(LOG_SERVER)
                << "Failed to connect " << security_ << " query sender socket: "
                << ec.message();
            return false;
        }
    }

    websockets_ready_ = true;
    websocket_thread_ = std::make_shared<std::thread>(
        &manager::handle_websockets, this);

    return true;
}

bool manager::stop_websocket_handler()
{
    const auto failure = (domain_ == "query" && !query_sender_->stop());

    if (failure)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query sender.";
    }

    websockets_ready_ = false;
    websocket_thread_->join();

    return !failure;
}

void manager::handle_websockets()
{
    const char* error;
    struct mg_bind_opts bind_opts{};
    mg_mgr_init(&mg_manager_, nullptr);

    const auto& endpoint = retrieve_endpoint(false);
    const auto port = std::to_string(endpoint.port());

    if (secure_)
    {
        bind_opts.ssl_cert = server_certificate_.c_str();
        bind_opts.ssl_key = server_private_key_.c_str();

        if (!ca_certificate_.empty())
            bind_opts.ssl_ca_cert = ca_certificate_.c_str();
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " " << domain_
        << " websocket to port " << port;

    bind_opts.error_string = &error;
    mg_connection_ptr connection = mg_bind_opt(&mg_manager_, port.c_str(),
        event_handler, bind_opts);

    if (connection == nullptr)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind listener websocket to port " << port << ": "
            << error;
        return;
    }

    connection->user_data = static_cast<void*>(this);
    mg_set_protocol_http_websocket(connection);

    websocket_options.document_root = settings_.websockets_root_path.c_str();
    websocket_options.enable_directory_listing = "no";

    while (!stopped() && websockets_ready_)
        poll(poll_interval);

    remove_connections();
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

        return (zeromq ? settings_.query_endpoint(secure_) :
            settings_.websockets_query_endpoint(secure_));
    }

    if (domain_ == "block")
        return (zeromq ? settings_.block_endpoint(secure_) :
            settings_.websockets_block_endpoint(secure_));

    if (domain_ == "heartbeat")
        return (zeromq ? settings_.heartbeat_endpoint(secure_) :
            settings_.websockets_heartbeat_endpoint(secure_));

    if (domain_ == "transaction")
        return (zeromq ? settings_.transaction_endpoint(secure_) :
            settings_.websockets_transaction_endpoint(secure_));

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
    const auto deadline = steady_clock::now() +
        milliseconds(timeout_milliseconds);

    while (steady_clock::now() < deadline)
    {
        ///////////////////////////////////////////////////////////////////////
        // Critical Section
        connection_lock_.lock();
        mg_mgr_poll(&mg_manager_, websocket_poll_interval);
        connection_lock_.unlock();
        ///////////////////////////////////////////////////////////////////////
    }
}

size_t manager::connection_count()
{
    return connections_.size();
}

bool manager::is_websocket_id(uint32_t id)
{
    return id & websocket_id_mask;
}

void manager::add_connection(mg_connection_ptr connection)
{
    connections_.insert(connection);
}

void manager::remove_connection(mg_connection_ptr connection)
{
    auto it = connections_.find(connection);
    if (it != connections_.end())
    {
        closesocket(connection->sock);
        connections_.erase(it);
    }

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    query_work_map_lock_.lock();

    for(auto it = query_work_map_.begin(); it != query_work_map_.end();)
    {
        if (it->second.connection == connection)
            it = query_work_map_.erase(it);
        else
            ++it;
    }

    query_work_map_lock_.unlock();
    ///////////////////////////////////////////////////////////////////////

}

void manager::remove_connections()
{
    for (auto connection : connections_)
        closesocket(connection->sock);

    connections_.clear();

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    query_work_map_lock_.lock();
    query_work_map_.clear();
    query_work_map_lock_.unlock();
    ///////////////////////////////////////////////////////////////////////
}

// Note that this is called with the connection_lock_ held across the
// mg_mgr_poll method.
void manager::notify_query_work(mg_connection_ptr connection,
    const std::string& command, const std::string& arguments)
{
    const auto handler = translator_.find(command);
    if (handler == translator_.end())
    {
        static const std::string error = "Unrecognized query command";
        send_impl<false>(connection, error);
        return;
    }

    // Wrap sequence number if needed.
    if (++sequence_ == max_uint32)
        sequence_ = websocket_id_mask;

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    query_work_map_lock_.lock();

    query_work_map_.emplace(std::piecewise_construct, std::forward_as_tuple(
        sequence_), std::forward_as_tuple(connection, command, arguments));

    query_work_map_lock_.unlock();
    ///////////////////////////////////////////////////////////////////////

    // Encode request based on query work and send to query_websocket.
    zmq::message request;
    handler->second.encode(request, handler->second.command, arguments,
        sequence_);

    // ZMQ pair sockets block when at the exceptional state of high
    // water mark.  To avoid backlogging futher, we drop the
    // connection lock so that the websocket handling can continue
    // during that time.
    connection_lock_.unlock();
    const auto ec = query_sender_->send(request);
    connection_lock_.lock();

    if (ec)
    {
        LOG_WARNING(LOG_SERVER)
            << "Query send failure: " << ec.message();

        static const std::string error = "Failed to send query to service";
        send_impl<false>(connection, error);
    }
}

void manager::send(mg_connection_ptr connection, const std::string& data)
{
    send_impl<true>(connection, data);
}

void manager::broadcast(const std::string& data)
{
    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    std::lock_guard<boost::detail::spinlock> guard(connection_lock_);
    for (auto connection : connections_)
        if (connection->user_data != nullptr)
            send_impl<false>(connection, data);
    ///////////////////////////////////////////////////////////////////////
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

    bc::chain::transaction tx;
    tx.from_data(transaction_data, true, true);

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

    // Format and send transaction to websocket subscribers.
    broadcast(to_json(block, height));

    LOG_VERBOSE(LOG_SERVER)
        << "Sent " << security_ << " block [" << height << "]";
    return true;
}

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
            << "Failure handling query response: invalid data";

        // Don't let a failure here prevent future notifications.
        return true;
    }

    std::string command;
    uint32_t id;
    data_chunk data;
    response.dequeue(command);
    response.dequeue<uint32_t>(id);
    response.dequeue(data);

    ///////////////////////////////////////////////////////////////////////
    // Critical Section
    query_work_map_lock_.lock();

    auto it = query_work_map_.find(id);
    if (it == query_work_map_.end())
    {
        query_work_map_lock_.unlock();

        LOG_WARNING(LOG_SERVER)
            << "Unmatched websocket query work item id: " << id;

        // Don't let a failure here prevent future notifications.
        return true;
    }

    const auto work = it->second;
    query_work_map_.erase(it);

    query_work_map_lock_.unlock();
    ///////////////////////////////////////////////////////////////////////

    data_source istream(data);
    istream_reader source(istream);

    ec = source.read_error_code();

    if (ec)
    {
        send(work.connection, to_json(ec));

        // Don't let a failure here prevent future notifications.
        return true;
    }

    const auto handler = translator_.find(work.command);
    if (handler == translator_.end())
    {
        static constexpr auto error = bc::error::not_implemented;
        send(work.connection, to_json(error));

        // Don't let a failure here prevent future notifications.
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
