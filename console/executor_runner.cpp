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

#include <bitcoin/server.hpp>

namespace libbitcoin {
namespace server {

using namespace network;
using namespace std::placeholders;

// Runner.
// ----------------------------------------------------------------------------

void executor::stopper(const std::string& message)
{
    // Stop capturing console evens and join its thread.
    capture_.stop();

    // Stop log, causing final message to be buffered by handler.
    log_.stop(message,levels::application);

    // Suspend process termination until final message is buffered.
    log_suspended_.get_future().wait();
}

void executor::subscribe_connect()
{
    node_->subscribe_connect
    (
        [&](const code&, const channel::ptr&)
        {
            log_.write(levels::verbose) <<
                "{in:" << node_->inbound_channel_count() << "}"
                "{ch:" << node_->channel_count() << "}"
                "{rv:" << node_->reserved_count() << "}"
                "{nc:" << node_->nonces_count() << "}"
                "{ad:" << node_->address_count() << "}"
                "{ss:" << node_->stop_subscriber_count() << "}"
                "{cs:" << node_->connect_subscriber_count() << "}."
                << std::endl;

            return true;
        },
        [&](const code&, uintptr_t)
        {
            // By not handling it is possible stop could fire before complete.
            // But the handler is not required for termination, so this is ok.
            // The error code in the handler can be used to differentiate.
        }
    );
}

void executor::subscribe_close()
{
    node_->subscribe_close
    (
        [&](const code&)
        {
            log_.write(levels::verbose) <<
                "{in:" << node_->inbound_channel_count() << "}"
                "{ch:" << node_->channel_count() << "}"
                "{rv:" << node_->reserved_count() << "}"
                "{nc:" << node_->nonces_count() << "}"
                "{ad:" << node_->address_count() << "}"
                "{ss:" << node_->stop_subscriber_count() << "}"
                "{cs:" << node_->connect_subscriber_count() << "}."
                << std::endl;

            return false;
        },
        [&](const code&, size_t)
        {
            // By not handling it is possible stop could fire before complete.
            // But the handler is not required for termination, so this is ok.
            // The error code in the handler can be used to differentiate.
        }
    );
}

bool executor::do_run()
{
    if (!metadata_.configured.log.path.empty())
        database::file::create_directory(metadata_.configured.log.path);

    // Hold sinks in scope for the length of the run.
    auto log = create_log_sink();
    auto events = create_event_sink();
    if (!log || !events)
    {
        logger(BS_LOG_INITIALIZE_FAILURE);
        return false;
    }

    subscribe_log(log);
    subscribe_events(events);
    subscribe_capture();
    logger(BS_LOG_HEADER);

    if (check_store_path())
    {
        auto ec = open_store_coded(true);
        if (ec == database::error::flush_lock)
        {
            ec = error::success;
            if (!restore_store(true))
                ec = database::error::integrity;
        }

        if (ec)
        {
            stopper(BS_NODE_STOPPED);
            return false;
        }
    }
    else if (!check_store_path(true) || !create_store(true))
    {
        stopper(BS_NODE_STOPPED);
        return false;
    }

    dump_body_sizes();
    dump_records();
    dump_buckets();
    ////logger(BS_MEASURE_PROGRESS_START);
    ////dump_progress();

    // Stopped by stopper.
    capture_.start();
    dump_version();
    dump_hardware();
    dump_options();
    logger(BS_NODE_INTERRUPT);

    // Create node.
    metadata_.configured.network.manual.initialize();
    node_ = std::make_shared<server_node>(query_, metadata_.configured, log_);

    // Subscribe node.
    subscribe_connect();
    subscribe_close();

    // Start network.
    logger(BS_NETWORK_STARTING);
    node_->start(std::bind(&executor::handle_started, this, _1));

    // Wait on signal to stop node (<ctrl-c>, etc).
    wait_for_stopping();

    // Stop network (if not already stopped by self).
    // Blocks on join of server/node/network threadpool.
    log_stopping();
    node_->close();

    // Sizes and records change, buckets don't.
    dump_body_sizes();
    dump_records();
    ////logger(BS_MEASURE_PROGRESS_START);
    ////dump_progress();

    if (!close_store(true))
    {
        stopper(BS_NODE_STOPPED);
        return false;
    }

    // Stop console capture and issue terminating log message.
    stopper(BS_NODE_STOPPED);
    return true; 
}

} // namespace server
} // namespace libbitcoin
