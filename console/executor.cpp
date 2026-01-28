/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include "executor.hpp"
#include "localize.hpp"

#include <atomic>
#include <csignal>
#include <future>
#include <mutex>

namespace libbitcoin {
namespace server {

using boost::format;
using namespace std::placeholders;

// non-const member static (global for non-blocking interrupt handling).
std::atomic_bool executor::cancel_{};

// non-const member static (global for blocking interrupt handling).
std::promise<code> executor::stopping_{};

executor::executor(parser& metadata, std::istream& input, std::ostream& output,
    std::ostream&)
  : metadata_(metadata),
    store_(metadata.configured.database),
    query_(store_),
    input_(input),
    output_(output),
    toggle_
    {
        metadata.configured.log.application,
        metadata.configured.log.news,
        metadata.configured.log.session,
        metadata.configured.log.protocol,
        metadata.configured.log.proxy,
        metadata.configured.log.remote,
        metadata.configured.log.fault,
        metadata.configured.log.quitting,
        metadata.configured.log.objects,
        metadata.configured.log.verbose
    }
{
    initialize_stop();
}

// Stop signal.
// ----------------------------------------------------------------------------

// Capture <ctrl-c>.
void executor::initialize_stop()
{
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);
}

void executor::handle_stop(int)
{
    initialize_stop();
    stop(error::success);
}

// Manage the race between console stop and server stop.
void executor::stop(const code& ec)
{
    static std::once_flag stop_mutex{};
    std::call_once(stop_mutex, [&]()
    {
        // Only for tool cancelation (scan/read/writer).
        cancel_.store(true);

        // Unblock monitor thread (do_run).
        stopping_.set_value(ec);
    });
}

// Event handlers.
// ----------------------------------------------------------------------------

void executor::handle_started(const code& ec)
{
    if (ec)
    {
        if (ec == node::error::store_uninitialized)
            logger(format(BS_UNINITIALIZED_CHAIN) %
                metadata_.configured.database.path);
        else
            logger(format(BS_NODE_START_FAIL) % ec.message());

        stop(ec);
        return;
    }

    logger(BS_NODE_STARTED);

    // Invokes handle_stopped(ec), which invokes stop(ec), unblocking monitor.
    node_->subscribe_close(
        std::bind(&executor::handle_stopped, this, _1),
        std::bind(&executor::handle_subscribed, this, _1, _2));
}

void executor::handle_subscribed(const code& ec, size_t)
{
    if (ec)
    {
        logger(format(BS_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec)
{
    if (ec)
    {
        logger(format(BS_NODE_START_FAIL) % ec.message());
        stop(ec);
        return;
    }

    logger(BS_NODE_RUNNING);
}

bool executor::handle_stopped(const code& ec)
{
    if (ec && ec != network::error::service_stopped)
        logger(format(BS_NODE_STOP_CODE) % ec.message());

    // Signal stop (simulates <ctrl-c>).
    stop(ec);
    return false;
}

} // namespace server
} // namespace libbitcoin
