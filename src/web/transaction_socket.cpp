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
#include <bitcoin/server/web/transaction_socket.hpp>

#include <functional>
#include <memory>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/web/http/json_string.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::chain;
using namespace bc::message;
using namespace bc::protocol;
using role = zmq::socket::role;

static const auto domain = "transaction";
static constexpr auto poll_interval_milliseconds = 100u;

transaction_socket::transaction_socket(zmq::authenticator& authenticator,
    server_node& node, bool secure)
  : socket(authenticator, node, secure, domain)
{
}

void transaction_socket::work()
{
    zmq::socket sub(authenticator_, role::subscriber, protocol_settings_);

    const auto endpoint = zeromq_endpoint().to_local();
    const auto ec = sub.connect(endpoint);

    if (ec)
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to connect to transaction service " << endpoint << ": "
            << ec.message();
        return;
    }

    if (!started(start_websocket_handler()))
    {
        LOG_ERROR(LOG_SERVER)
            << "Failed to start " << security_ << " " << domain
            << " websocket handler";
        return;
    }

    // Hold a shared reference to the websocket thread_ so that we can
    // properly call stop_websocket_handler on cleanup.
    const auto thread_ref = thread_;

    zmq::poller poller;
    poller.add(sub);

    while (!poller.terminated() && !stopped())
    {
        if (poller.wait(poll_interval_milliseconds).contains(sub.id()) &&
            !handle_transaction(sub))
            break;
    }

    const auto sub_stop = sub.stop();
    const auto websocket_stop = stop_websocket_handler();

    if (!sub_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to disconnect " << security_
            << " transaction websocket service.";

    if (!websocket_stop)
        LOG_ERROR(LOG_SERVER)
            << "Failed to stop " << security_
            << " transaction websocket handler";

    finished(sub_stop && websocket_stop);
}

// Called by this thread's work() method.
// Returns true to continue future notifications.
bool transaction_socket::handle_transaction(zmq::socket& subscriber)
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

    uint16_t sequence{};
    data_chunk transaction_data;
    response.dequeue<uint16_t>(sequence);
    response.dequeue(transaction_data);

    chain::transaction tx;
    tx.from_data(transaction_data, true, true);

    broadcast(web::to_json(tx, sequence));

    LOG_VERBOSE(LOG_SERVER)
        << "Broadcasted " << security_ << " socket tx ["
        << encode_hash(tx.hash()) << "]";
    return true;
}

const config::endpoint& transaction_socket::zeromq_endpoint() const
{
    // The Websocket to zeromq backend internally always uses the
    // local public zeromq endpoint since it does not affect the
    // external security of the websocket endpoint and impacts
    // configuration and performance for no additional gain.
    return server_settings_.zeromq_transaction_endpoint(false /* secure_ */);
}

const config::endpoint& transaction_socket::websocket_endpoint() const
{
    return server_settings_.websockets_transaction_endpoint(secure_);
}

} // namespace server
} // namespace libbitcoin
