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

#include <unordered_map>
#include <bitcoin/server.hpp>

namespace libbitcoin {
namespace server {

using namespace node;

const std::unordered_map<uint8_t, std::string> executor::fired_
{
    { events::header_archived,     "header_archived....." },
    { events::header_organized,    "header_organized...." },
    { events::header_reorganized,  "header_reorganized.." },

    { events::block_archived,      "block_archived......" },
    { events::block_buffered,      "block_buffered......" },
    { events::block_validated,     "block_validated....." },
    { events::block_confirmed,     "block_confirmed....." },
    { events::block_unconfirmable, "block_unconfirmable." },
    { events::validate_bypassed,   "validate_bypassed..." },
    { events::confirm_bypassed,    "confirm_bypassed...." },

    { events::tx_archived,         "tx_archived........." },
    { events::tx_validated,        "tx_validated........" },
    { events::tx_invalidated,      "tx_invalidated......" },

    { events::block_organized,     "block_organized....." },
    { events::block_reorganized,   "block_reorganized..." },

    { events::template_issued,     "template_issued....." },

    { events::snapshot_secs,       "snapshot_secs......." },
    { events::prune_msecs,         "prune_msecs........." },
    { events::reload_msecs,        "reload_msecs........" },
    { events::block_msecs,         "block_msecs........." },
    { events::ancestry_msecs,      "ancestry_msecs......" },
    { events::filter_msecs,        "filter_msecs........" },
    { events::filterhashes_msecs,  "filterhashes_msecs.." },
    { events::filterchecks_msecs,  "filterchecks_msecs.." }
};

// Events.
// ----------------------------------------------------------------------------

// TODO: throws, handle failure.
system::ofstream executor::create_event_sink() const
{
    // Standard file name, within the [node].path directory.
    return { metadata_.configured.log.events_file() };
}

void executor::subscribe_events(std::ostream& sink)
{
    using namespace network;
    using namespace std::chrono;

    log_.subscribe_events
    (
        [&sink, start = logger::now()](const code& ec, uint8_t event_,
            uint64_t value, const logger::time& point)
        {
            if (ec)
                return false;

            const auto time = duration_cast<seconds>(point - start).count();
            sink << fired_.at(event_) << " " << value << " " << time << std::endl;
            return true;
        }
    );
}

} // namespace server
} // namespace libbitcoin
