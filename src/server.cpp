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
#include "settings.hpp"
#include "worker.hpp"

// Localizable messages.
#define BS_SETTINGS_MESSAGE \
    "These configuration settings are currently in effect."
#define BS_INFORMATION_MESSAGE \
    "Runs a full bitcoin node in the global peer-to-peer network."
#define BS_INITIALIZING_CHAIN \
    "Please wait while the %1% directory is initialized.\n"
#define BS_INITCHAIN_DIR_FAIL \
    "Failed to create directory %1% with error, '%2%'.\n"
#define BS_INITCHAIN_DIR_EXISTS \
    "Failed because the directory %1% already exists.\n"
#define BS_INITCHAIN_DIR_TEST \
    "Failed to test directory %1% with error, '%2%'.\n"
#define BS_SERVER_STARTING \
    "Press CTRL-C to stop server.\n"
#define BS_SERVER_STARTED \
    "Server started.\n"
#define BS_SERVER_STOPPING \
    "Server stopping... Please wait.\n"
#define BS_SERVER_STOPPED \
    "Server stopped cleanly.\n"
#define BS_NODE_START_FAIL \
    "Node failed to start.\n"
#define BS_NODE_STOP_FAIL \
    "Node failed to stop.\n"
#define BS_PUBLISHER_START_FAIL \
    "Publisher failed to start: %1%\n"
#define BS_PUBLISHER_STOP_FAIL \
    "Publisher failed to stop.\n"
#define BS_USING_CONFIG_FILE \
    "Using config file: %1%\n"
#define BS_INVALID_PARAMETER \
    "Error: %1%\n"

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using boost::format;
using namespace boost::system;
using namespace boost::filesystem;

static void display_invalid_parameter(std::ostream& stream,
    const std::string& message)
{
    // English-only hack to patch missing arg name in boost exception message.
    std::string clean_message(message);
    boost::replace_all(clean_message, "for option is invalid", "is invalid");
    stream << format(BS_INVALID_PARAMETER) % clean_message;
}

static void show_help(config_type& metadata, std::ostream& output)
{
    // TODO: create a more general construction.
    bc::config::printer help("bitcoin_server", "", "", BS_INFORMATION_MESSAGE,
        metadata.load_arguments(), metadata.load_options());
    help.initialize();
    help.print(output);
}

static void show_settings(config_type& metadata, std::ostream& output)
{
    // TODO: create a more general construction and settings formatted output.
    bc::config::printer help("bitcoin_server", "", "", BS_SETTINGS_MESSAGE,
        metadata.load_arguments(), metadata.load_settings());
    help.initialize();
    help.print(output);
}

static console_result init_chain(path& directory, std::ostream& output,
    std::ostream& error)
{
    // Create the directory as a convenience for the user, and then use it
    // as sentinel to guard against inadvertent re-initialization.

    error_code code;
    if (!create_directories(directory, code))
    {
        if (code.value() == 0)
            error << format(BS_INITCHAIN_DIR_EXISTS) % directory;
        else
            error << format(BS_INITCHAIN_DIR_FAIL) % directory % code.message();

        return console_result::failure;
    }

    output << format(BS_INITIALIZING_CHAIN) % directory;

    using namespace bc::chain;
    const auto& prefix = directory.generic_string();
    initialize_blockchain(prefix);

    // Add genesis block.
    db_paths file_paths(prefix);
    db_interface interface(file_paths, { 0 });
    interface.start();

    // This is affected by the ENABLE_TESTNET switch.
    interface.push(genesis_block());

    return console_result::okay;
}

static console_result verify_chain(path& directory, std::ostream& output,
    std::ostream& error)
{
    // Use missing directory as a sentinel indicating lack of initialization.

    error_code code;
    if (!exists(directory, code))
    {
        if (code.value() == 2)
            return init_chain(directory, output, error);

        error << format(BS_INITCHAIN_DIR_TEST) % directory % code.message();
        return console_result::failure;
    }

    return console_result::okay;
}

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
static console_result run(settings_type& config, std::ostream& output,
    std::ostream& error)
{
    // Ensure the blockchain directory is initialized (at least exists).
    auto result = verify_chain(config.blockchain_path, output, error);
    if (result != console_result::okay)
        return result;

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
        error << BS_NODE_STOP_FAIL;
        return console_result::failure;
    }

    output << BS_SERVER_STOPPED;
    return console_result::okay;
}

// Load argument, environment and config and then run the server.
console_result dispatch(int argc, const char* argv[], std::istream&, 
    std::ostream& output, std::ostream& error)
{
    std::string message;
    config_type metadata;
    if (!load_config(metadata, message, argc, argv))
    {
        display_invalid_parameter(error, message);
        return console_result::failure;
    }

    if (!metadata.settings.config.empty())
        output << format(BS_USING_CONFIG_FILE) % metadata.settings.config;

    auto settings = metadata.settings;
    if (settings.help)
        show_help(metadata, output);
    else if (settings.settings)
        show_settings(metadata, output);
    else if (settings.initchain)
        return init_chain(settings.blockchain_path, output, error);
    else
        return run(settings, output, error);

    return console_result::okay;
}

} // namespace server
} // namespace libbitcoin
