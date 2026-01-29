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
#include <chrono>
#include <csignal>
#include <future>
#include <thread>

namespace libbitcoin {
namespace server {

using boost::format;
using namespace system;
using namespace std::placeholders;

// static initializers.
std::thread executor::stop_poller_{};
std::promise<bool> executor::stopping_{};
std::atomic<bool> executor::initialized_{};
std::atomic<int> executor::signal_{ unsignalled };

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
    BC_ASSERT(!initialized_);
    initialized_ = true;

    initialize_stop();

#if defined(HAVE_MSC)
    create_hidden_window();
#endif
}

executor::~executor()
{
    initialized_ = false;

#if defined(HAVE_MSC)
    destroy_hidden_window();
#endif
}

// Stop signal.
// ----------------------------------------------------------------------------
// static

#if defined(HAVE_MSC)
BOOL WINAPI executor::control_handler(DWORD signal)
{
    switch (signal)
    {
        // Keyboard events. These prevent exit altogether when TRUE returned.
        // handle_stop(signal) therefore shuts down gracefully/completely.
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:

        // A signal that the system sends to all processes attached to a
        // console when the user closes the console (by clicking Close on the
        // console window's window menu). Returning TRUE here does not
        // materially delay exit, so aside from capture this is a noop.
        case CTRL_CLOSE_EVENT:
            executor::handle_stop(possible_narrow_sign_cast<int>(signal));
            return TRUE;

        ////// Only services receive this (*any* user is logging off).
        ////case CTRL_LOGOFF_EVENT:
        ////// Only services receive this (all users already logged off).
        ////case CTRL_SHUTDOWN_EVENT:
        default:
            return FALSE;
    }
}
#endif

void executor::initialize_stop()
{
    poll_for_stopping();

#if defined(HAVE_MSC)
    ::SetConsoleCtrlHandler(&executor::control_handler, TRUE);
#else
    // Restart interrupted system calls.
    struct sigaction action
    {
        .sa_handler = handle_stop,
        .sa_flags = SA_RESTART
    };

    // Set masking.
    sigemptyset(&action.sa_mask);

    // Block during handling to prevent reentrancy.
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaddset(&action.sa_mask, SIGHUP);
    sigaddset(&action.sa_mask, SIGUSR2);

    sigaction(SIGINT,  &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGHUP,  &action, nullptr);
    sigaction(SIGUSR2, &action, nullptr);

    #if defined(HAVE_LINUX)
    sigaction(SIGPWR,  &action, nullptr);
    #endif
#endif
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
    using namespace std::this_thread;

    stop_poller_ = std::thread([]()
    {
        while (!canceled())
            sleep_for(std::chrono::milliseconds(10));

        stopping_.set_value(true);
    });
}

// Blocks until stopping is signalled by poller.
void executor::wait_for_stopping()
{
    stopping_.get_future().wait();
    if (stop_poller_.joinable())
        stop_poller_.join();
}

// Suspend verbose logging and log the stop signal.
void executor::log_stopping()
{
    const auto signal = signal_.load();
    if (signal == signal_none)
        return;

    // A high level of consolve logging can obscure and delay stop.
    toggle_.at(network::levels::protocol) = false;
    toggle_.at(network::levels::verbose) = false;
    toggle_.at(network::levels::proxy) = false;

    logger(format(BS_NODE_INTERRUPTED) % signal);
    logger(BS_NETWORK_STOPPING);
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
