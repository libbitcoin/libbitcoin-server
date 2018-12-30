/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/web/query_socket.hpp>

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::protocol;
using namespace bc::system;
using namespace bc::system::config;
using namespace bc::system::machine;
using role = zmq::socket::role;
using connection_ptr = http::connection_ptr;

static constexpr auto poll_interval_milliseconds = 100u;

query_socket::query_socket(zmq::context& context, server_node& node,
    bool secure)
  : http::socket(context, node.protocol_settings(), secure),
    settings_(node.server_settings()),
    protocol_settings_(node.protocol_settings())
{
    // JSON to ZMQ request encoders.
    //-------------------------------------------------------------------------

    const auto encode_empty = [](zmq::message& request,
        const std::string& command, const std::string& /* arguments */,
        uint32_t id)
    {
        request.enqueue(command);
        request.enqueue_little_endian(id);
        request.enqueue(data_chunk{});
    };

    const auto encode_height = [](zmq::message& request,
        const std::string& command, const std::string& arguments, uint32_t id)
    {
        request.enqueue(command);
        request.enqueue_little_endian(id);
        const uint32_t height = std::strtoul(arguments.c_str(), nullptr, 0);
        request.enqueue_little_endian(height);
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

    const auto encode_hash_or_height = [&](zmq::message& request,
        const std::string& command, const std::string& arguments, uint32_t id)
    {
        data_chunk decoded;
        if (decode_base16(decoded, arguments) &&
            decoded.size() == system::hash_size)
            encode_hash(request, command, arguments, id);
        else
            encode_height(request, command, arguments, id);
    };

    // A local clone of the task_sender send logic, for the decoders
    // below that don't need to use that mechanism since they're
    // already guaranteed to be run on the web thread.
    auto decode_send = [](connection_ptr connection, const std::string& json)
    {
        if (!connection || connection->closed())
            return false;

        if (json.size() > std::numeric_limits<int32_t>::max())
        {
            LOG_ERROR(LOG_SERVER_HTTP)
                << "Skipping JSON-RPC response of size " << json.size();
            return false;
        }

        const int32_t json_size = static_cast<int32_t>(json.size());

        if (!connection->json_rpc())
            return connection->write(json) == json_size;

        http::http_reply reply;
        const auto response = reply.generate(http::protocol_status::ok, {},
            json_size, false) + json;

        LOG_VERBOSE(LOG_SERVER_HTTP)
            << "Writing JSON-RPC response: " << response;

        return connection->write(response) == json_size;
    };

    // JSON to ZMQ response decoders.
    // -------------------------------------------------------------------------
    // These all run on the websocket thread, so can write on the
    // connection directly.
    const auto decode_height_raw = [decode_send](const data_chunk& data,
        const uint32_t id, connection_ptr connection, bool rpc)
    {
        data_source istream(data);
        istream_reader source(istream);
        const auto height = source.read_4_bytes_little_endian();
        decode_send(connection, http::to_json(height, id,
            connection->json_rpc()));
    };

    const auto decode_transaction_raw = [&node, decode_send](
        const data_chunk& data, const uint32_t id, connection_ptr connection,
        bool rpc)
    {
        const auto witness = chain::script::is_enabled(
            node.blockchain_settings().enabled_forks(), rule_fork::bip141_rule);
        const auto transaction = chain::transaction::factory(data, true,
            witness);
        decode_send(connection, http::to_json(transaction, id,
            connection->json_rpc()));
    };

    const auto decode_block_raw = [&node, decode_send](const data_chunk& data,
        const uint32_t id, connection_ptr connection, bool rpc)
    {
        const auto witness = chain::script::is_enabled(
            node.blockchain_settings().enabled_forks(), rule_fork::bip141_rule);
        const auto block = chain::block::factory(data, witness);
        decode_send(connection, http::to_json(block, id,
            connection->json_rpc()));
    };

    const auto decode_block_header_raw = [decode_send](const data_chunk& data,
        const uint32_t id, connection_ptr connection, bool rpc)
    {
        const auto header = chain::header::factory(data, true);
        decode_send(connection, http::to_json(header, id,
            connection->json_rpc()));
    };

    const auto decode_block_hash_from_header_raw = [decode_send](
        const data_chunk& data, uint32_t id, connection_ptr connection,
        bool rpc)
    {
        const auto header = chain::header::factory(data, true);
        if (connection->json_rpc())
            decode_send(connection, http::to_json(header.hash(), id,
                connection->json_rpc()));
        else
            decode_send(connection, http::to_json(header, id,
                connection->json_rpc()));
    };

#define REGISTER_HANDLER(native, core, encoder, decoder)    \
    handlers_[core] = handlers{ native, encoder, decoder }; \
    handlers_[native] = handlers{ native, encoder, decoder }

    REGISTER_HANDLER("blockchain.fetch_last_height", "getblockcount",
        encode_empty, decode_height);
    REGISTER_HANDLER("transaction_pool.fetch_transaction", "getrawtransaction",
        encode_hash, decode_transaction);
    REGISTER_HANDLER("blockchain.fetch_block", "getblock",
        encode_hash_or_height, decode_block);
    REGISTER_HANDLER("blockchain.fetch_block_header", "getblockheader",
        encode_hash_or_height, decode_block_header);
    REGISTER_HANDLER("blockchain.fetch_block_header", "getblockhash",
        encode_height, decode_block_hash_from_header);
    REGISTER_HANDLER("blockchain.fetch_block_height", "getblockheight",
        encode_hash, decode_height);

#undef REGISTER_HANDLER

}

void query_socket::work()
{
    zmq::socket dealer(context_, role::dealer, protocol_settings_);
    zmq::socket query_receiver(context_, role::pair, protocol_settings_);

    auto ec = query_receiver.bind(query_endpoint());

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " query internal socket: "
            << ec.message();
        return;
    }

    const auto endpoint = zeromq_endpoint().to_local();
    ec = dealer.connect(endpoint);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect to " << security_ << " query socket "
            << endpoint << ": " << ec.message();
        return;
    }

    if (!started(start_websocket_handler()))
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to start " << security_ << " websocket handler: "
            << ec.message();
        return;
    }

    LOG_INFO(LOG_SERVER)
        << "Bound " << security_ << " websocket query service to "
        << websocket_endpoint();

    // TODO: this should be hidden in socket base.
    // Hold a shared reference to the websocket thread_ so that we can
    // properly call stop_websocket_handler on cleanup.
    const auto thread_ref = thread_;

    zmq::poller poller;
    poller.add(dealer);
    poller.add(query_receiver);

    while (!poller.terminated() && !stopped())
    {
        const auto identifiers = poller.wait(poll_interval_milliseconds);

        if (identifiers.contains(query_receiver.id()) &&
            !forward(query_receiver, dealer))
            break;

        if (identifiers.contains(dealer.id()) && !handle_query(dealer))
            break;
    }

    const auto query_stop = query_receiver.stop();
    const auto dealer_stop = dealer.stop();
    const auto websocket_stop = stop_websocket_handler();

    if (!query_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " websocket query service.";

    if (!dealer_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query connection.";

    if (!websocket_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to stop " << security_ << " query websocket handler.";

    finished(query_stop && dealer_stop && websocket_stop);
}

// Called by this thread's zmq work() method.  Returns true to
// continue future notifications.
bool query_socket::handle_query(zmq::socket& dealer)
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

    if (!response.dequeue(command) || !response.dequeue<uint32_t>(sequence) ||
        !response.dequeue(data))
    {
        LOG_WARNING(LOG_SERVER)
            << "Failure handling query response: invalid data parts.";
        return true;
    }

    socket::queue_response(sequence, data, command);
    return true;
}

const endpoint& query_socket::zeromq_endpoint() const
{
    // The Websocket to zeromq backend internally always uses the
    // local public zeromq endpoint since it does not affect the
    // external security of the websocket endpoint and impacts
    // configuration and performance for no additional gain.
    return settings_.zeromq_query_endpoint(false /* secure_ */);
}

const endpoint& query_socket::websocket_endpoint() const
{
    return settings_.websockets_query_endpoint(secure_);
}

const endpoint& query_socket::query_endpoint() const
{
    static const endpoint secure_query("inproc://secure_query_websockets");
    static const endpoint public_query("inproc://public_query_websockets");
    return secure_ ? secure_query : public_query;
}

const std::shared_ptr<zmq::socket> query_socket::service() const
{
    return service_;
}

// This method is run by the web socket thread.
void query_socket::handle_websockets()
{
    // A zmq socket must remain on its single thread.
    service_ = std::make_shared<zmq::socket>(context_, role::pair,
        protocol_settings_);

    // Hold a reference to this service_ socket member by this thread
    // method so that we can properly shutdown even if the query_socket
    // object is destroyed.
    const auto service_ref = service_;
    const auto ec = service_ref->connect(query_endpoint());

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect " << security_ << " query sender socket: "
            << ec.message();
        socket_started_.set_value(false);
        return;
    }

    // socket::handle_websockets does web socket initialization and runs the
    // web event loop. Inside that loop, socket::poll eventually calls into
    // the static handle_event callback, which calls socket::notify_query_work,
    // which uses this 'service_' zmq socket for sending incoming requests and
    // reading the json web responses.
    socket::handle_websockets();

    if (!service_ref->stop())
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query sender.";
    }
}

bool query_socket::start_websocket_handler()
{
    auto started = socket_started_.get_future();
    thread_ = std::make_shared<asio::thread>(&query_socket::handle_websockets,
        this);
    return started.get();
}

} // namespace server
} // namespace libbitcoin
