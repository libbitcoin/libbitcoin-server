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
#include <bitcoin/server/message/incoming.hpp>
#include <bitcoin/server/message/notifier.hpp>
#include <bitcoin/server/message/outgoing.hpp>
#include <bitcoin/server/message/publisher.hpp>
#include <bitcoin/server/message/receiver.hpp>
#include <bitcoin/server/parser.hpp>
#include <bitcoin/server/interface/address.hpp>
#include <bitcoin/server/interface/blockchain.hpp>
#include <bitcoin/server/interface/protocol.hpp>
#include <bitcoin/server/interface/transaction_pool.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/version.hpp>

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
using namespace boost::system;
using namespace boost::filesystem;
using namespace bc::blockchain;
using namespace bc::config;
using namespace bc::network;
using namespace bc::node;

#define ATTACH(worker, class_name, method_name, instance) \
    worker.attach(#class_name "." #method_name, \
        std::bind(&class_name::method_name, \
            std::ref(instance), _1, _2));

constexpr auto append = std::ofstream::out | std::ofstream::app;

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
    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    signal(SIGABRT, interrupt_handler);

    if (code != 0 && !stopped)
    {
        bc::cout << format(BS_SERVER_STOPPING) % code << std::endl;
        stopped = true;
    }
}

// Class and method names must match protocol expectations (do not change).
static void attach_subscription_api(receiver& worker, notifier& notifier)
{
    ATTACH(worker, address, renew, notifier);
    ATTACH(worker, address, subscribe, notifier);
}

// Class and method names must match protocol expectations (do not change).
static void attach_query_api(receiver& worker, server_node& server)
{
    ATTACH(worker, address, fetch_history2, server);
    ATTACH(worker, blockchain, fetch_history, server);
    ATTACH(worker, blockchain, fetch_transaction, server);
    ATTACH(worker, blockchain, fetch_last_height, server);
    ATTACH(worker, blockchain, fetch_block_header, server);
    ATTACH(worker, blockchain, fetch_block_transaction_hashes, server);
    ATTACH(worker, blockchain, fetch_transaction_index, server);
    ATTACH(worker, blockchain, fetch_spend, server);
    ATTACH(worker, blockchain, fetch_block_height, server);
    ATTACH(worker, blockchain, fetch_stealth, server);
    ATTACH(worker, protocol, broadcast_transaction, server);
    ATTACH(worker, protocol, total_connections, server);
    ATTACH(worker, transaction_pool, validate, server);
    ATTACH(worker, transaction_pool, fetch_transaction, server);
}

// Run the server.
static console_result run(const configuration& configuration,
    std::ostream& output, std::ostream& error)
{
    // Ensure the blockchain directory is initialized (at least exists).
    const auto result = verify_chain(configuration.chain.database_path, error);
    if (result != console_result::okay)
        return result;

    // These must be libbitcoin streams.
    bc::ofstream debug_file(configuration.network.debug_file.string(), append);
    bc::ofstream error_file(configuration.network.error_file.string(), append);
    initialize_logging(debug_file, error_file, bc::cout, bc::cerr);

    static const auto startup = "================= startup ==================";
    log::debug(LOG_NODE) << startup;
    log::info(LOG_NODE) << startup;
    log::warning(LOG_NODE) << startup;
    log::error(LOG_NODE) << startup;
    log::fatal(LOG_NODE) << startup;

    log::info(LOG_SERVICE)
        << BS_SERVER_STARTING;

    server_node server(configuration);
    std::promise<code> start_promise;
    const auto handle_start = [&start_promise](const code& ec)
    {
        start_promise.set_value(ec);
    };

    server.start(handle_start);
    auto ec = start_promise.get_future().get();

    if (ec)
    {
        log::error(LOG_SERVICE)
            << format(BS_SERVER_START_FAIL) % ec.message();
        return console_result::not_started;
    }

    std::promise<code>run_promise;
    const auto handle_run = [&run_promise](const code ec)
    {
        run_promise.set_value(ec);
    };

    // Start the long-running sessions.
    server.run(handle_run);
    ec = run_promise.get_future().get();

    publisher publish(server, configuration.server);
    if (configuration.server.publisher_enabled && !publish.start())
    {
        log::error(LOG_SERVICE)
            << format(BS_PUBLISHER_START_FAIL) % zmq_strerror(zmq_errno());
        return console_result::not_started;
    }

    receiver worker(configuration.server);
    const auto subscribe = configuration.server.subscription_limit > 0;
    if ((configuration.server.queries_enabled || subscribe) && !worker.start())
    {
        log::error(LOG_SERVICE) << BS_WORKER_START_FAIL;
        return console_result::not_started;
    }

    if (configuration.server.queries_enabled)
        attach_query_api(worker, server);

    notifier notifier(server, configuration.server);
    if (subscribe)
        attach_subscription_api(worker, notifier);

    log::info(LOG_SERVICE) << BS_SERVER_STARTED;

    // Catch C signals for stopping the program.
    interrupt_handler(0);

    // Main loop.
    while (!stopped)
        worker.poll();

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
    parser metadata;
    std::string error_message;

    if (!metadata.parse(error_message, argc, argv))
    {
        display_invalid_parameter(error, error_message);
        return console_result::failure;
    }

    const auto settings = metadata.settings;
    if (!settings.file.empty())
        output << format(BS_USING_CONFIG_FILE) % settings.file << std::endl;

    if (settings.help)
        show_help(metadata, output);
    else if (settings.settings)
        show_settings(metadata, output);
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
