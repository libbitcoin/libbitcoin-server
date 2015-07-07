/**
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
#include <bitcoin/server/dispatch.hpp>

#include <csignal>
#include <iostream>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/publisher.hpp>
#include <bitcoin/server/subscribe_manager.hpp>
#include <bitcoin/server/version.hpp>
#include <bitcoin/server/worker.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/service/blockchain.hpp>
#include <bitcoin/server/service/compat.hpp>
#include <bitcoin/server/service/protocol.hpp>
#include <bitcoin/server/service/transaction_pool.hpp>

#define BS_APPLICATION_NAME "bs"

// Localizable messages.
#define BS_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BS_INFORMATION_MESSAGE \
    "Runs a full bitcoin node in the global peer-to-peer network."
#define BS_UNINITIALIZED_CHAIN \
    "The %1% directory is not initialized."
#define BS_INITIALIZING_CHAIN \
    "Please wait while initializing %1% directory..."
#define BS_INITCHAIN_DIR_NEW \
    "Failed to create directory %1% with error, '%2%'."
#define BS_INITCHAIN_DIR_EXISTS \
    "Failed because the directory %1% already exists."
#define BS_INITCHAIN_DIR_TEST \
    "Failed to test directory %1% with error, '%2%'."
#define BS_SERVER_STARTING \
    "Press CTRL-C to stop server."
#define BS_SERVER_STARTED \
    "Server started."
#define BS_SERVER_STOPPING \
    "Please wait while server is stopping..."
#define BS_SERVER_STOPPED \
    "Server stopped cleanly."
#define BS_SERVER_UNMAPPING \
    "Please wait while files are unmapped..."
#define BS_NODE_START_FAIL \
    "Node failed to start."
#define BS_NODE_STOP_FAIL \
    "Node failed to stop."
#define BS_PUBLISHER_START_FAIL \
    "Publisher failed to start: %1%"
#define BS_PUBLISHER_STOP_FAIL \
    "Publisher failed to stop."
#define BS_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BS_INVALID_PARAMETER \
    "Error: %1%"
#define BS_VERSION_MESSAGE \
    "\nVersion Information:\n\n" \
    "libbitcoin-server:     %1%\n" \
    "libbitcoin-node:       %2%\n" \
    "libbitcoin-blockchain: %3%\n" \
    "libbitcoin [%5%]:  %4%"

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using boost::format;
using namespace bc::config;
using namespace boost::system;
using namespace boost::filesystem;

static void display_invalid_parameter(std::ostream& stream,
    const std::string& message)
{
    // English-only hack to patch missing arg name in boost exception message.
    std::string clean_message(message);
    boost::replace_all(clean_message, "for option is invalid", "is invalid");
    stream << format(BS_INVALID_PARAMETER) % clean_message << std::endl;
}

static void show_help(config_type& metadata, std::ostream& stream)
{
    printer help(metadata.load_options(), metadata.load_arguments(),
        BS_APPLICATION_NAME, BS_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(stream);
}

static void show_settings(config_type& metadata, std::ostream& stream)
{
    printer settings(metadata.load_settings(), BS_APPLICATION_NAME,
        BS_SETTINGS_MESSAGE);
    settings.initialize();
    settings.settings(stream);
}

static void show_version(std::ostream& stream)
{
// The testnet switch is deprecated, so don't worry about loc.
#ifdef ENABLE_TESTNET
    const auto coinnet = "testnet";
#else
    const auto coinnet = "mainnet";
#endif

    stream << format(BS_VERSION_MESSAGE) % LIBBITCOIN_SERVER_VERSION %
        LIBBITCOIN_NODE_VERSION % LIBBITCOIN_BLOCKCHAIN_VERSION %
        LIBBITCOIN_VERSION % coinnet << std::endl;
}

static console_result init_chain(const path& directory, std::ostream& output,
    std::ostream& error)
{
    // TODO: see github.com/bitcoin/bitcoin/issues/432
    // Create the directory as a convenience for the user, and then use it
    // as sentinel to guard against inadvertent re-initialization.

    error_code code;
    if (!create_directories(directory, code))
    {
        if (code.value() == 0)
            error << format(BS_INITCHAIN_DIR_EXISTS) % directory << std::endl;
        else
            error << format(BS_INITCHAIN_DIR_NEW) % directory % code.message()
                << std::endl;

        return console_result::failure;
    }

    output << format(BS_INITIALIZING_CHAIN) % directory << std::endl;

    using namespace bc::chain;
    const auto& prefix = directory.string();
    initialize_blockchain(prefix);

    // Add genesis block.
    db_paths file_paths(prefix);
    db_interface interface(file_paths, { 0 });
    interface.start();

    // This is affected by the ENABLE_TESTNET switch.
    interface.push(genesis_block());

    return console_result::okay;
}

static console_result verify_chain(const path& directory, std::ostream& error)
{
    // Use missing directory as a sentinel indicating lack of initialization.

    error_code code;
    if (!exists(directory, code))
    {
        if (code.value() == 2)
            error << format(BS_UNINITIALIZED_CHAIN) % directory << std::endl;
        else
            error << format(BS_INITCHAIN_DIR_TEST) % directory %
                code.message() << std::endl;

        return console_result::failure;
    }

    return console_result::okay;
}

// Static handler for catching termination signals.
static bool stopped = false;
static void interrupt_handler(int)
{
    bc::cout << BS_SERVER_STOPPING << std::endl;
    stopped = true;
}

// Attach client-server API.
static void attach_api(request_worker& worker, server_node& node,
    subscribe_manager& subscriber)
{
    typedef std::function<void(server_node&, const incoming_message&,
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
    attach("address.fetch_history2", server_node::fullnode_fetch_history);
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
    auto result = verify_chain(config.blockchain_path, error);
    if (result != console_result::okay)
        return result;

    output << BS_SERVER_STARTING << std::endl;

    request_worker worker;
    worker.start(config);
    server_node full_node(config);
    publisher publish(full_node);

    if (config.publisher_enabled)
        if (!publish.start(config))
        {
            error << format(BS_PUBLISHER_START_FAIL) %
                zmq_strerror(zmq_errno()) << std::endl;

            // This is a bit wacky, since the node hasn't been started yet, but
            // there is threadpool cleanup code in here.
            full_node.stop();

            return console_result::not_started;
        }

    subscribe_manager subscriber(full_node);
    attach_api(worker, full_node, subscriber);

    // Start the node last so subscriptions to new blocks don't miss anything.
    if (!full_node.start(config))
    {
        error << BS_NODE_START_FAIL << std::endl;

        // This is a bit wacky, since the node hasn't been started yet, but
        // there is threadpool cleanup code in here.
        full_node.stop();

        return console_result::not_started;
    }

    output << BS_SERVER_STARTED << std::endl;

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
            error << BS_PUBLISHER_STOP_FAIL << std::endl;

    if (!full_node.stop())
    {
        error << BS_NODE_STOP_FAIL << std::endl;
        return console_result::failure;
    }

    output << BS_SERVER_STOPPED << std::endl;
    output << BS_SERVER_UNMAPPING << std::endl;
    return console_result::okay;
}

// Load argument, environment and config and then run the server.
console_result dispatch(int argc, const char* argv[], std::istream&, 
    std::ostream& output, std::ostream& error)
{
    std::string message;
    config_type configuration;
    if (!load_config(configuration, message, argc, argv))
    {
        display_invalid_parameter(error, message);
        return console_result::failure;
    }

    if (!configuration.settings.configuration.empty())
        output << format(BS_USING_CONFIG_FILE) % 
            configuration.settings.configuration << std::endl;

    auto settings = configuration.settings;
    if (settings.help)
        show_help(configuration, output);
    else if (settings.settings)
        show_settings(configuration, output);
    else if (settings.version)
        show_version(output);
    else if (settings.initchain)
        return init_chain(settings.blockchain_path, output, error);
    else
        return run(settings, output, error);

    return console_result::okay;
}

} // namespace server
} // namespace libbitcoin
