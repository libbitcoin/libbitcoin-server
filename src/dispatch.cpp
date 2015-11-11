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
#include <future>
#include <iostream>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/config/parser.hpp>
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
    "Please wait while server is starting."
#define BS_SERVER_START_FAIL \
    "Server failed to start with error, %1%."
#define BS_SERVER_STARTED \
    "Server started, press CTRL-C to stop."
#define BS_SERVER_STOPPING \
    "Please wait while server is stopping (code: %1%)..."
#define BS_SERVER_UNMAPPING \
    "Please wait while files are unmapped..."
#define BS_SERVER_STOP_FAIL \
    "Server stopped with error, %1%."
#define BS_PUBLISHER_START_FAIL \
    "Publisher service failed to start: %1%"
#define BS_PUBLISHER_STOP_FAIL \
    "Publisher service failed to stop."
#define BS_WORKER_START_FAIL \
    "Query service failed to start."
#define BS_WORKER_STOP_FAIL \
    "Query service failed to stop."
#define BS_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BS_INVALID_PARAMETER \
    "Error: %1%"
#define BS_VERSION_MESSAGE \
    "\nVersion Information:\n\n" \
    "libbitcoin-server:     %1%\n" \
    "libbitcoin-node:       %2%\n" \
    "libbitcoin-blockchain: %3%\n" \
    "libbitcoin:            %4%"

namespace libbitcoin {
namespace server {

using std::placeholders::_1;
using std::placeholders::_2;
using boost::format;
using namespace bc::blockchain;
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

static void show_help(parser& metadata, std::ostream& stream)
{
    printer help(metadata.load_options(), metadata.load_arguments(),
        BS_APPLICATION_NAME, BS_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(stream);
}

static void show_settings(parser& metadata, std::ostream& stream)
{
    printer print(metadata.load_settings(), BS_APPLICATION_NAME,
        BS_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(stream);
}

static void show_version(std::ostream& stream)
{
    stream << format(BS_VERSION_MESSAGE) % LIBBITCOIN_SERVER_VERSION %
        LIBBITCOIN_NODE_VERSION % LIBBITCOIN_BLOCKCHAIN_VERSION %
        LIBBITCOIN_VERSION << std::endl;
}

static console_result init_chain(const path& directory, bool testnet,
    std::ostream& output, std::ostream& error)
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

    const auto prefix = directory.string();
    auto genesis = testnet ? testnet_genesis_block() :
        mainnet_genesis_block();

    return database::initialize(prefix, genesis) ?
        console_result::okay : console_result::failure;
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
static void interrupt_handler(int code)
{
    bc::cout << format(BS_SERVER_STOPPING) % code << std::endl;
    stopped = true;
}

// Attach client-server API.
static void attach_api(request_worker& worker, server_node& node,
    subscribe_manager& subscriber)
{
    typedef std::function<void(server_node&, const incoming_message&,
        queue_send_callback)> basic_command_handler;

    auto attach = [&worker, &node](const std::string& command,
        basic_command_handler handler)
    {
        worker.attach(command, std::bind(handler, std::ref(node), _1, _2));
    };

    // Subscriptions.
    worker.attach("address.subscribe", 
        std::bind(&subscribe_manager::subscribe,
            &subscriber, _1, _2));

    worker.attach("address.renew", 
        std::bind(&subscribe_manager::renew,
            &subscriber, _1, _2));

    // Non-subscription API.
    attach("address.fetch_history2", server_node::fullnode_fetch_history);
    attach("blockchain.fetch_history", blockchain_fetch_history);
    attach("blockchain.fetch_transaction", blockchain_fetch_transaction);
    attach("blockchain.fetch_last_height", blockchain_fetch_last_height);
    attach("blockchain.fetch_block_header", blockchain_fetch_block_header);
    ////attach("blockchain.fetch_block_transaction_hashes", blockchain_fetch_block_transaction_hashes);
    attach("blockchain.fetch_transaction_index", blockchain_fetch_transaction_index);
    attach("blockchain.fetch_spend", blockchain_fetch_spend);
    attach("blockchain.fetch_block_height", blockchain_fetch_block_height);
    attach("blockchain.fetch_stealth", blockchain_fetch_stealth);
    attach("protocol.broadcast_transaction", protocol_broadcast_transaction);
    attach("protocol.total_connections", protocol_total_connections);
    attach("transaction_pool.validate", transaction_pool_validate);
    attach("transaction_pool.fetch_transaction", transaction_pool_fetch_transaction);

    // Deprecated command, for backward compatibility.
    attach("address.fetch_history", COMPAT_fetch_history);
}

// Run the server.
static console_result run(const configuration& config, std::ostream& output,
    std::ostream& error)
{
    // Ensure the blockchain directory is initialized (at least exists).
    const auto result = verify_chain(config.chain.database_path, error);
    if (result != console_result::okay)
        return result;

    output << BS_SERVER_STARTING << std::endl;

    server_node server(config);
    std::promise<code> start_promise;
    const auto handle_start = [&start_promise](const code& ec)
    {
        start_promise.set_value(ec);
    };

    // Logging initialized here.
    server.start(handle_start);
    auto ec = start_promise.get_future().get();

    if (ec)
    {
        error << format(BS_SERVER_START_FAIL) % ec.message() << std::endl;
        return console_result::not_started;
    }

    publisher publish(server, config.server);
    if (config.server.publisher_enabled)
    {
        if (!publish.start())
        {
            error << format(BS_PUBLISHER_START_FAIL) %
                zmq_strerror(zmq_errno()) << std::endl;
            return console_result::not_started;
        }
    }

    request_worker worker(config.server);
    subscribe_manager subscriber(server, config.server);
    if (config.server.queries_enabled)
    {
        if (!worker.start())
        {
            error << BS_WORKER_START_FAIL << std::endl;
            return console_result::not_started;
        }

        attach_api(worker, server, subscriber);
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
    if (config.server.queries_enabled)
        if (!worker.stop())
            error << BS_WORKER_STOP_FAIL << std::endl;

    if (config.server.publisher_enabled)
        if (!publish.stop())
            error << BS_PUBLISHER_STOP_FAIL << std::endl;

    std::promise<code> stop_promise;
    const auto handle_stop = [&stop_promise](const code& ec)
    {
        stop_promise.set_value(ec);
    };

    server.stop(handle_stop);
    ec = stop_promise.get_future().get();

    if (ec)
        error << format(BS_SERVER_STOP_FAIL) % ec.message() << std::endl;

    output << BS_SERVER_UNMAPPING << std::endl;
    return ec ? console_result::failure : console_result::okay;
}

// Load argument, environment and config and then run the server.
console_result dispatch(int argc, const char* argv[], std::istream&, 
    std::ostream& output, std::ostream& error)
{
    parser parsed;
    std::string message;

    if (!parser::parse(parsed, message, argc, argv))
    {
        display_invalid_parameter(error, message);
        return console_result::failure;
    }

    const auto settings = parsed.settings;
    if (!settings.file.empty())
        output << format(BS_USING_CONFIG_FILE) % settings.file << std::endl;

    if (settings.help)
        show_help(parsed, output);
    else if (settings.settings)
        show_settings(parsed, output);
    else if (settings.version)
        show_version(output);
    else if (settings.mainnet)
        return init_chain(settings.chain.database_path, false, output, error);
    else if (settings.testnet)
        return init_chain(settings.chain.database_path, true, output, error);
    else
        return run(settings, output, error);

    return console_result::okay;
}

} // namespace server
} // namespace libbitcoin
