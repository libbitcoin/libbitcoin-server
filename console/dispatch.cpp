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
#include "dispatch.hpp"

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

namespace libbitcoin {
namespace server {
    
using boost::format;
using std::placeholders::_1;
using std::placeholders::_2;
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
    stream << format(BS_VERSION_MESSAGE) %
        LIBBITCOIN_SERVER_VERSION %
        LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION %
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

// Run the server.
static console_result run(const configuration& configuration,
    std::ostream& output, std::ostream& error)
{
    // This must be verified before node/blockchain construct.
    // Ensure the blockchain directory is initialized (at least exists).
    const auto result = verify_chain(configuration.chain.database_path, error);
    if (result != console_result::okay)
        return result;

    // TODO: make these members of new dispatch class.
    // Keep server and services in scope until stop, but start after node start.
    server_node server(configuration);
    publisher publish(server);
    receiver receive(server);
    notifier notify(server);

    // TODO: initialize on construct of new dispatch class.
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
    log::info(LOG_SERVICE) << BS_SERVER_STARTING;

    // BUGBUG: race between this sequence and <CTRL-C>.
    // The stop handlers are registered in start.
    server.start(
        std::bind(handle_started,
            _1, std::ref(server), std::ref(publish), std::ref(receive),
                std::ref(notify)));

    // Block until the node is stopped.
    return wait_for_stop(server);
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
    else if (settings.main_network)
        return init_chain(settings.chain.database_path, false, output, error);
    else if (settings.test_network)
        return init_chain(settings.chain.database_path, true, output, error);
    else
        return run(settings, output, error);

    return console_result::okay;
}

// Static handler for catching termination signals.
static auto stopped = false;
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

// TODO: use server, publish, receive, notify as members.
// This is called at the end of seeding.
void handle_started(const code& ec, server_node& server, publisher& publish,
    receiver& receive, notifier& notify)
{
    if (ec)
    {
        log::info(LOG_NODE) << format(BS_SERVER_START_FAIL) % ec.message();
        stopped = true;
        return;
    }

    // Start running the node (header and block sync for now).
    server.run(
        std::bind(handle_running,
            _1, std::ref(server), std::ref(publish), std::ref(receive),
                std::ref(notify)));
}

// TODO: use notifier and receive as members.
// Class and method names must match protocol expectations (do not change).
static void attach_subscription_api(receiver& receive, notifier& notifier)
{
    ATTACH(receive, address, renew, notifier);
    ATTACH(receive, address, subscribe, notifier);
}

// TODO: user server and receive as members.
// Class and method names must match protocol expectations (do not change).
static void attach_query_api(receiver& receive, server_node& server)
{
    ATTACH(receive, address, fetch_history2, server);
    ATTACH(receive, blockchain, fetch_history, server);
    ATTACH(receive, blockchain, fetch_transaction, server);
    ATTACH(receive, blockchain, fetch_last_height, server);
    ATTACH(receive, blockchain, fetch_block_header, server);
    ATTACH(receive, blockchain, fetch_block_transaction_hashes, server);
    ATTACH(receive, blockchain, fetch_transaction_index, server);
    ATTACH(receive, blockchain, fetch_spend, server);
    ATTACH(receive, blockchain, fetch_block_height, server);
    ATTACH(receive, blockchain, fetch_stealth, server);
    ATTACH(receive, protocol, broadcast_transaction, server);
    ATTACH(receive, protocol, total_connections, server);
    ATTACH(receive, transaction_pool, validate, server);
    ATTACH(receive, transaction_pool, fetch_transaction, server);
}

// TODO: use server as member.
// This is called at the end of block sync, though execution continues after.
void handle_running(const code& ec, server_node& server, publisher& publish,
    receiver& receive, notifier& notify)
{
    if (ec)
    {
        log::info(LOG_NODE) << format(BS_SERVER_START_FAIL) % ec.message();
        stopped = true;
        return;
    }

    // The node is running now, waiting on stopped to be set to true.

    // Start server services on top of the node, these log internally.
    if (!publish.start() || !receive.start() || !notify.start())
    {
        stopped = true;
        return;
    }

    if (server.configuration_settings().queries_enabled)
        attach_query_api(receive, server);

    if (server.configuration_settings().subscription_limit > 0)
        attach_subscription_api(receive, notify);
}

// TODO: use server as member.
console_result wait_for_stop(server_node& server)
{
    // Set up the stop handler.
    std::promise<code> promise;
    const auto stop_handler = [&promise](code ec) { promise.set_value(ec); };

    // Monitor stopped for completion.
    monitor_for_stop(server, stop_handler);

    // Block until the stop handler is invoked.
    const auto ec = promise.get_future().get();

    if (ec)
    {
        log::info(LOG_NODE) << format(BS_SERVER_STOP_FAIL) % ec.message();
        return console_result::failure;
    }

    log::info(LOG_NODE) << BS_SERVER_STOPPED;
    return console_result::okay;
}

// TODO: use server as member.
void monitor_for_stop(server_node& server, p2p::result_handler handler)
{
    using namespace std::chrono;
    using namespace std::this_thread;
    std::string command;

    interrupt_handler(0);
    log::info(LOG_NODE) << BS_SERVER_STARTED;

    while (!stopped)
        sleep_for(milliseconds(10));

    log::info(LOG_NODE) << BS_SERVER_UNMAPPING;
    server.stop(handler);
}

} // namespace server
} // namespace libbitcoin
