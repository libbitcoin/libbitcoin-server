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
#include "executor.hpp"

#include <algorithm>
#include <csignal>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/common.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <bitcoin/server.hpp>

namespace libbitcoin {
namespace server {

using boost::format;
using namespace std::placeholders;
using namespace boost::system;
using namespace bc::config;
using namespace bc::database;
using namespace bc::network;

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;

typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;

static constexpr int initialize_stop = 0;
static constexpr int directory_exists = 0;
static constexpr int directory_not_found = 2;
static const auto application_name = "bs";

std::promise<code> executor::stopping_;

executor::executor(parser& metadata, std::istream& input,
    std::ostream& output, std::ostream& error)
  : metadata_(metadata), output_stream_(output), error_stream_(error),
    debug_file_(metadata_.configured.network.debug_file.string(),
        std::ofstream::out | std::ofstream::app),
    error_file_(metadata_.configured.network.error_file.string(),
        std::ofstream::out | std::ofstream::app)
{
    boost::shared_ptr<std::ostream> console_out(&output_stream_,
        boost::null_deleter());
    boost::shared_ptr<std::ostream> console_err(&error_stream_,
        boost::null_deleter());
    boost::shared_ptr<bc::ofstream> debug_log(&debug_file_,
        boost::null_deleter());
    boost::shared_ptr<bc::ofstream> error_log(&error_file_,
        boost::null_deleter());

    initialize_logging(debug_log, error_log, console_out, console_err);
    handle_stop(initialize_stop);
}

// Command line options.
// ----------------------------------------------------------------------------
// Emit directly to standard output (not the log).

void executor::do_help()
{
    const auto options = metadata_.load_options();
    printer help(options, application_name, BS_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_stream_);
}

void executor::do_settings()
{
    const auto settings = metadata_.load_settings();
    printer print(settings, application_name, BS_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_stream_);
}

void executor::do_version()
{
    output_stream_ << format(BS_VERSION_MESSAGE) %
        LIBBITCOIN_SERVER_VERSION %
        LIBBITCOIN_PROTOCOL_VERSION %
        LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION %
        LIBBITCOIN_VERSION << std::endl;
}

// Emit to the log.
bool executor::do_initchain()
{
    initialize_output();

    error_code ec;
    const auto& directory = metadata_.configured.database.directory;

    if (create_directories(directory, ec))
    {
        LOG_INFO(LOG_SERVER) << format(BS_INITIALIZING_CHAIN) % directory;

        // Unfortunately we are still limited to a choice of hardcoded chains.
        const auto genesis = metadata_.configured.chain.use_testnet_rules ?
            chain::block::genesis_testnet() : chain::block::genesis_mainnet();

        const auto result = data_base::initialize(directory, genesis);

        LOG_INFO(LOG_SERVER) << BS_INITCHAIN_COMPLETE;
        return result;
    }

    if (ec.value() == directory_exists)
    {
        LOG_ERROR(LOG_SERVER) << format(BS_INITCHAIN_EXISTS) % directory;
        return false;
    }

    LOG_ERROR(LOG_SERVER) << format(BS_INITCHAIN_NEW) % directory % ec.message();
    return false;
}

// Menu selection.
// ----------------------------------------------------------------------------

bool executor::menu()
{
    const auto& config = metadata_.configured;

    if (config.help)
    {
        do_help();
        return true;
    }

    if (config.settings)
    {
        do_settings();
        return true;
    }

    if (config.version)
    {
        do_version();
        return true;
    }

    if (config.initchain)
    {
        return do_initchain();
    }

    // There are no command line arguments, just run the server.
    return run();
}

// Run.
// ----------------------------------------------------------------------------

bool executor::run()
{
    initialize_output();

    LOG_INFO(LOG_SERVER) << BS_NODE_STARTING;

    if (!verify_directory())
        return false;

    // Ensure all configured services can function.
    set_minimum_threadpool_size();

    // Now that the directory is verified we can create the node for it.
    node_ = std::make_shared<server_node>(metadata_.configured);

    // The callback may be returned on the same thread.
    node_->start(
        std::bind(&executor::handle_started,
            this, _1));

    // Wait for stop.
    stopping_.get_future().wait();

    LOG_INFO(LOG_SERVER) << BS_NODE_STOPPING;

    // Close must be called from main thread.
    if (node_->close())
        LOG_INFO(LOG_NODE) << BS_NODE_STOPPED;
    else
        LOG_INFO(LOG_NODE) << BS_NODE_STOP_FAIL;

    return true;
}

// Handle the completion of the start sequence and begin the run sequence.
void executor::handle_started(const code& ec)
{
    if (ec)
    {
        LOG_ERROR(LOG_SERVER) << format(BS_NODE_START_FAIL) % ec.message();
        stop(ec);
        return;
    }

    LOG_INFO(LOG_SERVER) << BS_NODE_SEEDED;

    // This is the beginning of the stop sequence.
    node_->subscribe_stop(
        std::bind(&executor::handle_stopped,
            this, _1));

    // This is the beginning of the run sequence.
    node_->run(
        std::bind(&executor::handle_running,
            this, _1));
}

// This is the end of the run sequence.
void executor::handle_running(const code& ec)
{
    if (ec)
    {
        LOG_INFO(LOG_SERVER) << format(BS_NODE_START_FAIL) % ec.message();
        stop(ec);
        return;
    }

    LOG_INFO(LOG_SERVER) << BS_NODE_STARTED;
}

// This is the end of the stop sequence.
void executor::handle_stopped(const code& ec)
{
    stop(ec);
}

// Stop signal.
// ----------------------------------------------------------------------------

void executor::handle_stop(int code)
{
    // Reinitialize after each capture to prevent hard shutdown.
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);
    std::signal(SIGABRT, handle_stop);

    if (code == initialize_stop)
        return;

    LOG_INFO(LOG_SERVER) << format(BS_NODE_SIGNALED) % code;
    stop(error::success);
}

void executor::stop(const code& ec)
{
    static std::once_flag stop_mutex;
    std::call_once(stop_mutex, [&](){ stopping_.set_value(ec); });
}

// Utilities.
// ----------------------------------------------------------------------------

// Set up logging.
void executor::initialize_output()
{
    LOG_DEBUG(LOG_SERVER) << BS_LOG_HEADER;
    LOG_INFO(LOG_SERVER) << BS_LOG_HEADER;
    LOG_WARNING(LOG_SERVER) << BS_LOG_HEADER;
    LOG_ERROR(LOG_SERVER) << BS_LOG_HEADER;
    LOG_FATAL(LOG_SERVER) << BS_LOG_HEADER;

    const auto& file = metadata_.configured.file;

    if (file.empty())
        LOG_INFO(LOG_SERVER) << BS_USING_DEFAULT_CONFIG;
    else
        LOG_INFO(LOG_SERVER) << format(BS_USING_CONFIG_FILE) % file;
}

// Use missing directory as a sentinel indicating lack of initialization.
bool executor::verify_directory()
{
    error_code ec;
    const auto& directory = metadata_.configured.database.directory;

    if (exists(directory, ec))
        return true;

    if (ec.value() == directory_not_found)
    {
        LOG_ERROR(LOG_SERVER) << format(BS_UNINITIALIZED_CHAIN) % directory;
        return false;
    }

    const auto message = ec.message();
    LOG_ERROR(LOG_SERVER) << format(BS_INITCHAIN_TRY) % directory % message;
    return false;
}

// Increase the configured minomum as required to operate the service.
void executor::set_minimum_threadpool_size()
{
    metadata_.configured.network.threads =
        std::max(metadata_.configured.network.threads,
            server_node::threads_required(metadata_.configured));
}

template<typename Stream>
void executor::add_text_sink(boost::shared_ptr<Stream>& stream)
{
    // Construct the sink
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    // Add a stream to write log to
    sink->locked_backend()->add_stream(stream);

    sink->set_formatter(expr::stream << "["
        << expr::format_date_time<boost::posix_time::ptime, char>(
            log::attributes::timestamp.get_name(), "%Y-%m-%d %H:%M:%S")
        << "][" << log::attributes::channel
        << "][" << log::attributes::severity
        << "]: " << expr::smessage);

    // Register the sink in the logging core
    logging::core::get()->add_sink(sink);
}

template<typename Stream, typename FunT>
void executor::add_text_sink(boost::shared_ptr<Stream>& stream,
    FunT const& filter)
{
    // Construct the sink
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    // Add a stream to write log to
    sink->locked_backend()->add_stream(stream);

    sink->set_filter(filter);

    sink->set_formatter(expr::stream << "["
        << expr::format_date_time<boost::posix_time::ptime, char>(
            log::attributes::timestamp.get_name(), "%Y-%m-%d %H:%M:%S")
        << "][" << log::attributes::channel
        << "][" << log::attributes::severity
        << "]: " << expr::smessage);

    // Register the sink in the logging core
    logging::core::get()->add_sink(sink);
}

void executor::initialize_logging(boost::shared_ptr<bc::ofstream>& debug,
    boost::shared_ptr<bc::ofstream>& error,
    boost::shared_ptr<std::ostream>& output_stream,
    boost::shared_ptr<std::ostream>& error_stream)
{
    auto error_filter = (log::attributes::severity == log::severity::warning)
        || (log::attributes::severity == log::severity::error)
        || (log::attributes::severity == log::severity::fatal);

    auto info_filter = (log::attributes::severity == log::severity::info);

    add_text_sink(debug);
    add_text_sink(error, error_filter);
    add_text_sink(output_stream, info_filter);
    add_text_sink(output_stream, error_filter);
}

} // namespace server
} // namespace libbitcoin
