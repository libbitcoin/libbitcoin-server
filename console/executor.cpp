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
using namespace std::placeholders;

std::atomic_bool executor::cancel_{};
std::thread executor::stop_poller_{};
std::promise<bool> executor::stopping_{};

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

#if defined(HAVE_MSC)
BOOL WINAPI executor::win32_handler(DWORD signal)
{
    ////if (auto* log = fopen("shutdown.log", "a"))
    ////{
    ////    fprintf(log, "Signal %lu at %llu\n", signal, GetTickCount64());
    ////    fflush(log);
    ////    fclose(log);
    ////}

    switch (signal)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            executor::handle_stop({});
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

// Call only once.
void executor::initialize_stop()
{
    poll_for_stopping();

#if defined(HAVE_MSC)
    // TODO: use RegisterServiceCtrlHandlerEx for service registration.
    ::SetConsoleCtrlHandler(&executor::win32_handler, TRUE);
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

void executor::poll_for_stopping()
{
    stop_poller_ = std::thread([]()
    {
        while (!cancel_.load(std::memory_order_acquire))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        stopping_.set_value(true);
    });
}

void executor::wait_for_stopping()
{
    stopping_.get_future().wait();
    if (stop_poller_.joinable())
        stop_poller_.join();
}

// Implementation is limited to signal safe code.
void executor::handle_stop(int)
{
    stop();
}

// Manage the race between console stop and server stop.
void executor::stop()
{
    cancel_.store(true, std::memory_order_release);
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
