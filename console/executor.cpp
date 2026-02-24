/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <chrono>
#include <future>
#include <optional>

namespace libbitcoin {
namespace server {

using boost::format;
using namespace system;
using namespace std::placeholders;

// Construction.
// ----------------------------------------------------------------------------

// static initializers.
std::promise<bool> executor::stopped_{};
std::promise<bool> executor::stopping_{};
std::atomic<int> executor::signal_{ unsignalled };
std::optional<std::thread> executor::poller_thread_{};

// class factory (singleton)
executor& executor::factory(parser& metadata, std::istream& input,
    std::ostream& output, std::ostream& error)
{
    static executor instance(metadata, input, output, error);
    return instance;
}

// private constructor (singleton)
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

// 
executor::~executor()
{
    uninitialize_stop();
}

// Stop signal.
// ----------------------------------------------------------------------------
// static

void executor::initialize_stop()
{
    poll_for_stopping();
    create_hidden_window();
    set_signal_handlers();
}

void executor::uninitialize_stop()
{
    stop();
    if (poller_thread_.has_value() && poller_thread_.value().joinable())
    {
        poller_thread_.value().join();
        poller_thread_.reset();
    }

    destroy_hidden_window();
}

// Handle the stop signal and invoke stop method (requries signal safe code).
void executor::handle_stop(int signal)
{
    stop(signal);
}

// Manage race between console stop and server stop.
void executor::stop(int signal)
{
    ////if (auto* log = fopen("shutdown.log", "a"))
    ////{
    ////    fprintf(log, "stop %lu at %llu\n", signal, GetTickCount64());
    ////    fflush(log);
    ////    fclose(log);
    ////}

    // Implementation is limited to signal safe code.
    static_assert(std::atomic<int>::is_always_lock_free);

    // Capture first handled signal value.
    auto unset = unsignalled;
    signal_.compare_exchange_strong(unset, signal, std::memory_order_acq_rel);
}

// Any thread can monitor this for stopping.
bool executor::canceled()
{
    return signal_.load(std::memory_order_acquire) != unsignalled;
}

// Spinning must be used in signal handler, cannot wait on a promise.
void executor::poll_for_stopping()
{
    poller_thread_.emplace(std::thread([]()
    {
        while (!canceled())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        stopping_.set_value(true);
    }));
}

// Blocks until stopping is signalled by poller.
void executor::wait_for_stopping()
{
    stopping_.get_future().wait();
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

        stop();
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
        stop();
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec)
{
    if (ec)
    {
        logger(format(BS_NODE_START_FAIL) % ec.message());
        stop();
        return;
    }

    logger(BS_NODE_RUNNING);
}

bool executor::handle_stopped(const code& ec)
{
    if (ec && ec != network::error::service_stopped)
        logger(format(BS_NODE_STOP_CODE) % ec.message());

    // Signal stop (simulates <ctrl-c>).
    stop();
    return false;
}

} // namespace server
} // namespace libbitcoin
