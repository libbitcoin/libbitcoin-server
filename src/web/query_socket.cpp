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
#include <bitcoin/server/web/query_socket.hpp>

#include <bitcoin/protocol.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::config;
using namespace bc::protocol;
using role = zmq::socket::role;

static const auto domain = "query";
static constexpr auto poll_interval_milliseconds = 100u;

query_socket::query_socket(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : manager(authenticator, node, secure, domain)
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
    const auto decode_height = [this](const data_chunk& data, const uint32_t id,
        connection_ptr connection)
    {
        data_source istream(data);
        istream_reader source(istream);
        const auto height = source.read_4_bytes_little_endian();
        send(connection, to_json(height, id));
    };

    const auto decode_transaction = [this](const data_chunk& data,
        const uint32_t id, connection_ptr connection)
    {
        chain::transaction transaction;
        transaction.from_data(data, true, true);
        send(connection, to_json(transaction, id));
    };

    const auto decode_block_header = [this](const data_chunk& data,
        const uint32_t id,connection_ptr connection)
    {
        chain::header header;
        header.from_data(data, true);
        send(connection, to_json(header, id));
    };

    handlers_["query fetch-height"] = handlers
    {
        "blockchain.fetch_last_height",
        encode_empty,
        decode_height
    };

    handlers_["query fetch-tx"] = handlers
    {
        "transaction_pool.fetch_transaction2",
        encode_hash,
        decode_transaction
    };

    handlers_["query fetch-header"] = handlers
    {
        "blockchain.fetch_block_header",
        encode_hash,
        decode_block_header
    };
}

void query_socket::work()
{
    zmq::socket dealer(authenticator_, role::dealer, protocol_settings_);
    zmq::socket query_receiver(authenticator_, role::pair, protocol_settings_);

    auto ec = query_receiver.bind(retrieve_query_endpoint());

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to bind " << security_ << " query internal socket: "
            << ec.message();
        return;
    }

    const auto endpoint = retrieve_zeromq_connect_endpoint();
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

    if (!query_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to unbind " << security_ << " websocket query service.";

    if (!dealer_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query connection.";

    if (!stop_websocket_handler())
        LOG_ERROR(LOG_SERVER)
            << "Failed to stop " << security_
            << " query websocket handler";

    finished(query_stop && dealer_stop);
}

const config::endpoint& query_socket::retrieve_zeromq_endpoint() const
{
    return server_settings_.zeromq_query_endpoint(secure_);
}

const config::endpoint& query_socket::retrieve_websocket_endpoint() const
{
    return server_settings_.websockets_query_endpoint(secure_);
}

const config::endpoint& query_socket::retrieve_query_endpoint() const
{
    static const config::endpoint secure_query("inproc://secure_query_websockets");
    static const config::endpoint public_query("inproc://public_query_websockets");
    return secure_ ? secure_query : public_query;
}

void query_socket::handle_websockets()
{
    // A zmq socket must remain on its single thread.
    service_ = std::make_shared<socket>(authenticator_, role::pair, protocol_settings_);
    // Hold a shared reference to the service_ socket_ so that we can
    // properly call stop on cleanup.
    const auto service_ref = service_;
    const auto ec = service_ref->connect(retrieve_query_endpoint());

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect " << security_ << " query sender socket: "
            << ec.message();
        thread_status_.set_value(false);
        return;
    }

    // manager::poll eventually calls into the static (manager)
    // handle_event callback, which calls manager::notify_query_work,
    // which uses this service_ socket for sending.
    manager::handle_websockets();

    if (!service_ref->stop())
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_ << " query sender.";
    }
}

bool query_socket::start_websocket_handler()
{
    std::future<bool> status = thread_status_.get_future();
    thread_ = std::make_shared<asio::thread>(&query_socket::handle_websockets, this);
    status.wait();
    return status.get();
}

} // namespace server
} // namespace libbitcoin
