/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
        logger(std::format(BS_USING_CONFIG_FILE, system::from_path(configuration)));
    }

    using selection = system::config::selection;
    const auto& selected = metadata_.configured.bitcoin.network;
    if (metadata_.is_configured(settings::bitcoin_network))
    {
        const auto& variables = metadata_.variables();
        const auto& value = variables.at(settings::bitcoin_network);
        const auto& configured = value.as<selection>();
        if (configured != selected)
            logger(std::format(BS_NETWORK_OVERRIDE,
                selection::to_word(selected), selection::to_word(configured)));
    }

    const auto& store = metadata_.configured.database.path;
    if (create)
    {
        logger(std::format(BS_INITIALIZING_CHAIN, system::from_path(store)));
        if (const auto ec = database::file::create_directory_ex(store))
        {
            logger(std::format(BS_INITCHAIN_DIRECTORY_ERROR, system::from_path(store),
                ec.message()));
            return false;
        }
    }
    else
    {
        if (!database::file::is_directory(store))
        {
            logger(std::format(BS_UNINITIALIZED_DATABASE, system::from_path(store)));
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
            logger(std::format(BS_CREATE,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        logger(std::format(BS_INITCHAIN_DATABASE_CREATE_FAILURE, ec.message()));
        return false;
    }

    if (!initialize_store(details))
        return false;

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_INITCHAIN_CREATED, span.count()));
    return true;
}

// Create and confirm genesis block (store invalid without it).
bool executor::initialize_store(bool details)
{
    logger(BS_INITCHAIN_DATABASE_INITIALIZE);
    if (!query_.initialize(metadata_.configured.bitcoin.genesis_block))
    {
        logger(BS_INITCHAIN_DATABASE_INITIALIZE_FAILURE);
        close_store(details);
        return false;
    }

    return true;
}

bool executor::open_store(bool details)
{
    return !open_store_coded(details);
}

// not timed or announced (generally fast)
code executor::open_store_coded(bool details)
{
    if (metadata_.stored_code == database::error::schema_version)
    {
        logger(std::format(BS_DATABASE_START_FAIL,
            metadata_.stored_code.message()));
        logger(std::format(BS_DATABASE_SCHEMA_MISMATCH,
            metadata_.stored.schema.to_string(),
            database::envelope::compiled.to_string()));
        return metadata_.stored_code;
    }

    auto migrated = false;
    if (!metadata_.stored_code &&
        metadata_.stored.schema != database::envelope::compiled)
    {
        if (!prompt_migrate_store())
            return database::error::schema_version;

        if (const auto ec = migrate_store(migrated, details))
            return ec;
    }

    // The pool compaction is the open's only pruning, and its last step.
    std::optional<logger::time> compacting{};
    ////logger(BS_DATABASE_STARTING);
    if (const auto ec = store_.open([&](auto event_, auto table)
    {
        if (!compacting && event_ == database::event_t::prune_table)
            compacting = logger::now();

        if (details)
            logger(std::format(BS_OPEN,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        if (ec == database::error::fault_lock)
            logger(BS_DATABASE_FAULT_LOCK);
        else
            logger(std::format(BS_DATABASE_START_FAIL, ec.message()));

        return ec;
    }

    if (compacting)
    {
        const auto span = duration_cast<milliseconds>(logger::now() -
            compacting.value());
        logger(std::format(BS_POOL_COMPACTED, query_.pool_records(),
            span.count()));
    }

    apply_envelope();
    logger(BS_DATABASE_STARTED);

    if (migrated)
        /* bool */ cold_backup_store(details);

    return error::success;
}

// Migrate a closed store to the compiled schema.
code executor::migrate_store(bool& migrated, bool details)
{
    const auto& compiled = database::envelope::compiled;
    logger(std::format(BS_DATABASE_MIGRATING,
        metadata_.stored.schema.to_string(), compiled.to_string()));

    const auto start = logger::now();
    system::config::version original{};
    if (const auto ec = store_.migrate(original, metadata_.stored,
        [&](auto event_, auto table)
    {
        if (details)
            logger(std::format(BS_MIGRATE,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        logger(std::format(BS_DATABASE_MIGRATE_FAIL, original.to_string(),
            ec.message()));
        return ec;
    }

    migrated = (original != compiled);
    metadata_.stored.schema = compiled;
    if (!migrated)
    {
        logger(std::format(BS_DATABASE_CURRENT, compiled.to_string()));
        return error::success;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_DATABASE_MIGRATED, original.to_string(),
        compiled.to_string(), span.count()));
    return error::success;
}

bool executor::prompt_migrate_store() const
{
    if (metadata_.configured.accept)
        return true;

    if (!interactive())
    {
        logger(BS_MIGRATE_HALTED);
        return false;
    }

    logger(BS_PROMPT_SETOFF);
    logger(std::format(BS_MIGRATE_PROMPT1,
        metadata_.stored.schema.to_string(),
        database::envelope::compiled.to_string()));
    logger(BS_MIGRATE_PROMPT2);
    logger(BS_MIGRATE_CHOICE1);
    logger(BS_MIGRATE_CHOICE2);
    logger(BS_PROMPT_SETOFF);

    std::string line{};
    while (std::getline(input_, line) && !canceled())
    {
        system::trim(line);
        if (line.empty())
            return true;

        if (line == "c")
        {
            logger(BS_MIGRATE_HALTED);
            return false;
        }

        logger(BS_MIGRATE_CHOICE1);
        logger(BS_MIGRATE_CHOICE2);
    }

    return false;
}

bool executor::close_store(bool details)
{
    logger(BS_DATABASE_STOPPING);
    const auto start = logger::now();
    if (const auto ec = store_.close([&](auto event_, auto table)
    {
        if (details)
            logger(std::format(BS_CLOSE,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        logger(std::format(BS_DATABASE_STOP_FAIL, ec.message()));
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_DATABASE_TIMED_STOP, span.count()));
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
        logger(std::format(BS_RELOAD_INVALID, ec.message()));
        return false;
    }

    logger(BS_NODE_RELOAD_STARTED);
    const auto start = logger::now();
    if (const auto ec = node_->reload([&](auto event_, auto table)
    {
        if (details)
            logger(std::format(BS_RELOAD,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        logger(std::format(BS_NODE_RELOAD_FAIL, ec.message()));
        return false;
    };

    node_->resume();
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_NODE_RELOAD_COMPLETE, span.count()));
    return true;
}

bool executor::restore_store(bool details)
{
    logger(BS_RESTORING_CHAIN);
    const auto start = logger::now();
    if (const auto ec = store_.restore([&](auto event_, auto table)
    {
        if (details)
            logger(std::format(BS_RESTORE,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        if (ec == database::error::flush_lock)
            logger(BS_RESTORE_MISSING_FLUSH_LOCK);
        else if (ec == database::error::fault_lock)
            logger(BS_DATABASE_FAULT_LOCK);
        else
            logger(std::format(BS_RESTORE_FAILURE, ec.message()));

        return false;
    }

    // Recovery without a snapshot recreates an empty store.
    if (!query_.is_initialized() && !initialize_store(details))
        return false;

    apply_envelope();
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_RESTORE_COMPLETE, span.count()));
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
        logger(std::format(BS_SNAPSHOT_INVALID, ec.message()));
        return false;
    }

    logger(BS_NODE_BACKUP_STARTED);
    const auto start = logger::now();
    if (const auto ec = node_->snapshot([&](auto event_, auto table)
    {
        if (details)
            logger(std::format(BS_BACKUP,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        // system::error::not_a_stream when disk is full.
        logger(std::format(BS_NODE_BACKUP_FAIL, ec.message()));
        return false;
    }

    node_->resume();
    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_NODE_BACKUP_COMPLETE, span.count()));
    return true;
}

bool executor::cold_backup_store(bool details)
{
    logger(BS_NODE_BACKUP_STARTED);
    const auto start = logger::now();
    if (const auto ec = store_.snapshot([&](auto event_, auto table)
    {
        if (details)
            logger(std::format(BS_BACKUP,
                server_node::store::events.at(event_),
                server_node::store::tables.at(table)));
    }))
    {
        // system::error::not_a_stream when disk is full.
        logger(std::format(BS_NODE_BACKUP_FAIL, ec.message()));
        return false;
    }

    const auto span = duration_cast<seconds>(logger::now() - start);
    logger(std::format(BS_NODE_BACKUP_COMPLETE, span.count()));
    return true;
}

bool executor::prompt_milestone_store() const
{
    const auto& milestone = metadata_.configured.bitcoin.milestone;
    if (metadata_.is_configured(settings::milestone) ||
        metadata_.configured.accept)
    {
        logger(std::format(BS_BITCOIN_MILESTONE, milestone.to_string()));
        return true;
    }

    const auto genesis_default = is_zero(milestone.height());
    if (genesis_default || !interactive() || store_.is_dirty())
        return true;

    logger(BS_PROMPT_SETOFF);
    logger(std::format(BS_MILESTONE_PROMPT1));
    logger(std::format(BS_MILESTONE_PROMPT2));
    logger(std::format(BS_MILESTONE_PROMPT3));
    logger(std::format(BS_BITCOIN_MILESTONE, milestone.to_string()));
    logger(BS_MILESTONE_CHOICE1);
    logger(BS_MILESTONE_CHOICE2);
    logger(BS_PROMPT_SETOFF);

    std::string line{};
    while (std::getline(input_, line) && !canceled())
    {
        system::trim(line);
        if (line.empty())
            return true;

        if (line == "c")
        {
            logger(BS_MILESTONE_HALTED1);
            logger(BS_MILESTONE_HALTED2);
            return false;
        }

        logger(BS_MILESTONE_CHOICE1);
        logger(BS_MILESTONE_CHOICE2);
    }

    return false;
}

void executor::apply_envelope()
{
    auto& configured = metadata_.configured;
    const auto applied = store_.get_envelope().apply(
        configured.bitcoin,
        configured.database,
        configured.node.limited_blocks,
        configured.node.require_witness,
        configured.node.silent_start_height);

    for (const auto& name: metadata_.overrides)
        logger(std::format(BS_DATABASE_ENVELOPE_OVERRIDE, name));

    for (const auto& name: applied)
        logger(std::format(BS_DATABASE_ENVELOPE_OVERRIDE, name));

    metadata_.overrides.clear();
}

} // namespace server
} // namespace libbitcoin
