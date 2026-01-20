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

namespace libbitcoin {
namespace server {

using boost::format;
using namespace network;

// Store functions.
// ----------------------------------------------------------------------------

bool executor::check_store_path(bool create) const
{
    const auto& configuration = metadata_.configured.file;
    if (configuration.empty())
    {
        logger(BS_USING_DEFAULT_CONFIG);
    }
    else
    {
        logger(format(BS_USING_CONFIG_FILE) % configuration);
    }

    const auto& store = metadata_.configured.database.path;
    if (create)
    {
        logger(format(BS_INITIALIZING_CHAIN) % store);
        if (const auto ec = database::file::create_directory_ex(store))
        {
            logger(format(BS_INITCHAIN_DIRECTORY_ERROR) % store % ec.message());
            return false;
        }
    }
    else
    {
        if (!database::file::is_directory(store))
        {
            logger(format(BS_UNINITIALIZED_DATABASE) % store);
            return false;
        }
    }

    return true;
}

bool executor::create_store(bool details)
{
    logger(BS_INITCHAIN_CREATING);
    const auto start = logger::now();
    if (const auto ec = store_.create([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_CREATE) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        logger(format(BS_INITCHAIN_DATABASE_CREATE_FAILURE) % ec.message());
        return false;
    }

    // Create and confirm genesis block (store invalid without it).
    logger(BS_INITCHAIN_DATABASE_INITIALIZE);
    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        logger(BS_INITCHAIN_DATABASE_INITIALIZE_FAILURE);
        close_store(details);
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BS_INITCHAIN_CREATED) % span.count());
    return true;
}

bool executor::open_store(bool details)
{
    return !open_store_coded(details);
}

// not timed or announced (generally fast)
code executor::open_store_coded(bool details)
{
    ////logger(BS_DATABASE_STARTING);
    if (const auto ec = store_.open([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_OPEN) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        logger(format(BS_DATABASE_START_FAIL) % ec.message());
        return ec;
    }

    logger(store_.is_dirty() ? BS_DATABASE_STARTED_DIRTY : BS_DATABASE_STARTED);
    return error::success;
}

bool executor::close_store(bool details)
{
    logger(BS_DATABASE_STOPPING);
    const auto start = logger::now();
    if (const auto ec = store_.close([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_CLOSE) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        logger(format(BS_DATABASE_STOP_FAIL) % ec.message());
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BS_DATABASE_TIMED_STOP) % span.count());
    return true;
}

bool executor::reload_store(bool details)
{
    if (!node_)
    {
        logger(BS_NODE_UNAVAILABLE);
        return false;
    }

    if (const auto ec = store_.get_fault())
    {
        logger(format(BS_RELOAD_INVALID) % ec.message());
        return false;
    }

    logger(BS_NODE_RELOAD_STARTED);
    const auto start = logger::now();
    if (const auto ec = node_->reload([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_RELOAD) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        logger(format(BS_NODE_RELOAD_FAIL) % ec.message());
        return false;
    };

    node_->resume();
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BS_NODE_RELOAD_COMPLETE) % span.count());
    return true;
}

bool executor::restore_store(bool details)
{
    logger(BS_RESTORING_CHAIN);
    const auto start = logger::now();
    if (const auto ec = store_.restore([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_RESTORE) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        if (ec == database::error::flush_lock)
            logger(BS_RESTORE_MISSING_FLUSH_LOCK);
        else
            logger(format(BS_RESTORE_FAILURE) % ec.message());

        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BS_RESTORE_COMPLETE) % span.count());
    return true;
}

bool executor::hot_backup_store(bool details)
{
    if (!node_)
    {
        logger(BS_NODE_UNAVAILABLE);
        return false;
    }

    if (const auto ec = store_.get_fault())
    {
        logger(format(BS_SNAPSHOT_INVALID) % ec.message());
        return false;
    }

    logger(BS_NODE_BACKUP_STARTED);
    const auto start = logger::now();
    if (const auto ec = node_->snapshot([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_BACKUP) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        // system::error::not_a_stream when disk is full.
        logger(format(BS_NODE_BACKUP_FAIL) % ec.message());
        return false;
    }

    node_->resume();
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BS_NODE_BACKUP_COMPLETE) % span.count());
    return true;
}

bool executor::cold_backup_store(bool details)
{
    logger(BS_NODE_BACKUP_STARTED);
    const auto start = logger::now();
    if (const auto ec = store_.snapshot([&](auto event_, auto table)
    {
        if (details)
            logger(format(BS_BACKUP) %
                server_node::store::events.at(event_) %
                server_node::store::tables.at(table));
    }))
    {
        // system::error::not_a_stream when disk is full.
        logger(format(BS_NODE_BACKUP_FAIL) % ec.message());
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(format(BS_NODE_BACKUP_COMPLETE) % span.count());
    return true;
}

} // namespace server
} // namespace libbitcoin
