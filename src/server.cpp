/*
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "server.hpp"

#include <csignal>
#include <iostream>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/bitcoin.hpp>
#include "echo.hpp"
#include "message.hpp"
#include "node_impl.hpp"
#include "publisher.hpp"
#include "service/blockchain.hpp"
#include "service/fullnode.hpp"
#include "service/protocol.hpp"
#include "service/transaction_pool.hpp"
#include "service/compat.hpp"
#include "subscribe_manager.hpp"
#include "config.hpp"
#include "worker.hpp"

// Localizable messages.
#define BS_SERVER_STARTING      "Press CTRL-C to stop server.\n"
#define BS_SERVER_STARTED       "Server started.\n"
#define BS_SERVER_STOPPING      "Server stopping... Please wait.\n"
#define BS_SERVER_STOPPED       "Server stopped cleanly.\n"
#define BS_NODE_START_FAIL      "Node failed to start.\n"
#define BS_NODE_STOP_FAIL       "Node failed to stop.\n"
#define BS_PUBLISHER_START_FAIL "Publisher failed to start: %1%\n"
#define BS_PUBLISHER_STOP_FAIL  "Publisher failed to stop.\n"
#define BS_USING_CONFIG_FILE    "Using config file: %1%\n"
#define BS_INVALID_PARAMETER    "Error: %1%\n"

namespace libbitcoin {
namespace server {

using boost::format;
using boost::filesystem::path;
using std::placeholders::_1;
using std::placeholders::_2;

// Static handler for catching termination signals.
static bool stopped = false;
static void interrupt_handler(int)
{
    echo() << BS_SERVER_STOPPING;
    stopped = true;
}

// Attach client-server API.
static void attach_api(request_worker& worker, node_impl& node,
    subscribe_manager& subscriber)
{
    typedef std::function<void(node_impl&, const incoming_message&,
        queue_send_callback)> basic_command_handler;
    auto attach = [&worker, &node](
        const std::string& command, basic_command_handler handler)
    {
        worker.attach(command, std::bind(handler, std::ref(node), _1, _2));
    };

    // Subscriptions.
    worker.attach("address.subscribe", 
        std::bind(&subscribe_manager::subscribe, &subscriber, _1, _2));
    worker.attach("address.renew", 
        std::bind(&subscribe_manager::renew, &subscriber, _1, _2));

    // Non-subscription API.
    attach("address.fetch_history2", fullnode_fetch_history);
    attach("blockchain.fetch_history", blockchain_fetch_history);
    attach("blockchain.fetch_transaction", blockchain_fetch_transaction);
    attach("blockchain.fetch_last_height", blockchain_fetch_last_height);
    attach("blockchain.fetch_block_header", blockchain_fetch_block_header);
    attach("blockchain.fetch_block_transaction_hashes", 
        blockchain_fetch_block_transaction_hashes);
    attach("blockchain.fetch_transaction_index",
        blockchain_fetch_transaction_index);
    attach("blockchain.fetch_spend", blockchain_fetch_spend);
    attach("blockchain.fetch_block_height", blockchain_fetch_block_height);
    attach("blockchain.fetch_stealth", blockchain_fetch_stealth);
    attach("protocol.broadcast_transaction", protocol_broadcast_transaction);
    attach("protocol.total_connections", protocol_total_connections);
    attach("transaction_pool.validate", transaction_pool_validate);
    attach("transaction_pool.fetch_transaction", 
        transaction_pool_fetch_transaction);

    // Deprecated command, for backward compatibility.
    attach("address.fetch_history", COMPAT_fetch_history);
}

// Run the server.
static console_result run(config_type config, std::ostream& output,
    std::ostream& error)
{
    output << BS_SERVER_STARTING;

    request_worker worker;
    worker.start(config);
    node_impl full_node(config);
    publisher publish(full_node);

    if (config.publisher_enabled)
        if (!publish.start(config))
        {
            error << format(BS_PUBLISHER_START_FAIL) %
                zmq_strerror(zmq_errno());
            return console_result::not_started;
        }

    subscribe_manager subscriber(full_node);
    attach_api(worker, full_node, subscriber);

    // Start the node last so subscriptions to new blocks don't miss anything.
    if (!full_node.start(config))
    {
        error << BS_NODE_START_FAIL;
        return console_result::not_started;
    }

    output << BS_SERVER_STARTED;

    // Catch C signals for stopping the program.
    signal(SIGABRT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    signal(SIGINT, interrupt_handler);

    // Main loop.
    while (!stopped)
        worker.update();

    // Stop the worker, publisher and node.

    worker.stop();

    if (config.publisher_enabled)
        if (!publish.stop())
            error << BS_PUBLISHER_STOP_FAIL;

    if (!full_node.stop())
    {
        output << BS_NODE_STOP_FAIL;
        return console_result::failure;
    }

    output << BS_SERVER_STOPPED;
    return console_result::okay;
}

void display_invalid_parameter(std::ostream& stream,
    const std::string& message)
{
    // English-only hack to patch missing arg name in boost exception message.
    std::string clean_message(message);
    boost::replace_all(clean_message, "for option is invalid", "is invalid");

    stream << format(BS_INVALID_PARAMETER) % clean_message;
}

// Load argument, environment and config and then run the server.
console_result run(int argc, const char* argv[], std::istream&, 
    std::ostream& output, std::ostream& error)
{
    config_type config;
    std::string message;
    auto config_path = path(bc::config::system_config_directory())
        / "libbitcoin" / "server.cfg";

    if (!load_config(config, message, config_path, argc, argv))
    {
        display_invalid_parameter(error, message);
        return console_result::failure;
    }

    if (!config_path.empty())
        output << format(BS_USING_CONFIG_FILE) % config_path;

    return run(config, output, error);
}

} // namespace server
} // namespace libbitcoin

