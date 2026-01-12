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

#include <boost/format.hpp>
#include <bitcoin/server.hpp>

namespace libbitcoin {
namespace server {

using system::config::printer;

const std::string executor::name_{ "bs" };

// Command line options.
// ----------------------------------------------------------------------------

// --[h]elp
bool executor::do_help()
{
    log_.stop();
    printer help(metadata_.load_options(), name_, BS_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
    return true;
}

// --[d]hardware
bool executor::do_hardware()
{
    log_.stop();
    dump_hardware();
    return true;
}

// --[s]ettings
bool executor::do_settings()
{
    log_.stop();
    printer print(metadata_.load_settings(), name_, BS_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
    return true;
}

// --[v]ersion
bool executor::do_version()
{
    log_.stop();
    dump_version();
    return true;
}

// --[n]ewstore
bool executor::do_new_store()
{
    log_.stop();
    if (!check_store_path(true) ||
        !create_store(true))
        return false;

    // Records and sizes reflect genesis block only.
    dump_body_sizes();
    dump_records();
    dump_buckets();

    if (!close_store(true))
        return false;

    logger(BS_INITCHAIN_COMPLETE);
    return true;
}

// --[b]ackup
bool executor::do_backup()
{
    log_.stop();
    return check_store_path()
        && open_store()
        && cold_backup_store(true)
        && close_store();
}

// --[r]estore
bool executor::do_restore()
{
    log_.stop();
    return check_store_path()
        && restore_store(true)
        && close_store();
}

// --[f]lags
bool executor::do_flags()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_flags();
    return close_store();
}

// --[i]nformation
bool executor::do_information()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    dump_sizes();
    return close_store();
}

// --[a]slabs
bool executor::do_slabs()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_slabs();
    return close_store();
}

// --[k]buckets
bool executor::do_buckets()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_buckets();
    return close_store();
}

// --[l]collisions
bool executor::do_collisions()
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    scan_collisions();
    return close_store();
}

// --[t]read
bool executor::do_read(const system::hash_digest& hash)
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    read_test(hash);
    return close_store();
}

// --[w]rite
bool executor::do_write(const system::hash_digest& hash)
{
    log_.stop();
    if (!check_store_path() ||
        !open_store())
        return false;

    write_test(hash);
    return close_store();
}

// Command line dispatch.
// ----------------------------------------------------------------------------

bool executor::dispatch()
{
    const auto& config = metadata_.configured;

    if (config.help)
        return do_help();

    // Order below matches help output (alphabetical), so that first option is
    // executed in the case where multiple options are parsed.

    if (config.slabs)
        return do_slabs();

    if (config.backup)
        return do_backup();

    if (config.restore)
        return do_restore();

    if (config.hardware)
        return do_hardware();

    if (config.flags)
        return do_flags();

    if (config.newstore)
        return do_new_store();

    if (config.buckets)
        return do_buckets();

    if (config.collisions)
        return do_collisions();

    if (config.information)
        return do_information();

    if (config.settings)
        return do_settings();

    if (config.version)
        return do_version();

    if (config.test != system::null_hash)
        return do_read(config.test);

    if (config.write != system::null_hash)
        return do_write(config.write);

    return do_run();
}

} // namespace server
} // namespace libbitcoin
