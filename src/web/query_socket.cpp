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
#include <bitcoin/server/web/json_string.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::config;
using namespace bc::protocol;
using role = zmq::socket::role;

static const auto domain = "query";
static constexpr auto poll_interval_milliseconds = 100u;

query_socket::query_socket(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : socket(authenticator, node, secure, domain)
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
        send(connection, web::to_json(height, id));
    };

    const auto decode_transaction = [this](const data_chunk& data,
        const uint32_t id, connection_ptr connection)
    {
        chain::transaction transaction;
        transaction.from_data(data, true, true);
        send(connection, web::to_json(transaction, id));
    };

    const auto decode_block_header = [this](const data_chunk& data,
        const uint32_t id,connection_ptr connection)
    {
        chain::header header;
        header.from_data(data, true);
        send(connection, web::to_json(header, id));
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

// Called by this thread's zmq work() method.
// Returns true to continue future notifications.
//
// Correlation lock usage is required because it protects the shared
// correlation map of query ids, which is also used by the websocket
// thread event handler (e.g. remove_connection, notify_query_work).
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
        send(work.connection, web::to_json(ec, id));
        return true;
    }

    const auto handler = handlers_.find(work.command);
    if (handler == handlers_.end())
    {
        static constexpr auto error = bc::error::not_implemented;
        send(work.connection, web::to_json(error, id));
        return true;
    }

    // Decode response and send query output to websocket client.
    // Note that this send is a web socket send from the zmq thread.
    // It does not need to be protected because the send is buffered
    // and the actual socket write happens on the web socket thread
    // via socket::poll that is eventually called from the
    // socket::handle_websockets loop.
    const auto payload = source.read_bytes();
    if (work.connection->user_data != nullptr)
        handler->second.decode(payload, id, work.connection);
    return true;
}

const endpoint& query_socket::retrieve_zeromq_endpoint() const
{
    return server_settings_.zeromq_query_endpoint(secure_);
}

const endpoint& query_socket::retrieve_websocket_endpoint() const
{
    return server_settings_.websockets_query_endpoint(secure_);
}

const endpoint& query_socket::retrieve_query_endpoint() const
{
    static const endpoint secure_query("inproc://secure_query_websockets");
    static const endpoint public_query("inproc://public_query_websockets");
    return secure_ ? secure_query : public_query;
}

const std::shared_ptr<zmq::socket> query_socket::get_service() const
{
    return service_;
}

// This method is run by the web socket thread.
void query_socket::handle_websockets()
{
    // A zmq socket must remain on its single thread.
    service_ = std::make_shared<zmq::socket>(authenticator_, role::pair,
        protocol_settings_);

    // Hold a reference to this service_ socket member by this thread
    // method so that we can properly shutdown even if the
    // query_socket object is destroyed.
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

    // socket::handle_websockets does web socket initialization and
    // runs the web event loop.  Inside that loop, socket::poll
    // eventually calls into the static handle_event callback, which
    // calls socket::notify_query_work, which uses this service_ zmq
    // socket for sending incoming requests and reading the json web
    // responses.
    socket::handle_websockets();

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
