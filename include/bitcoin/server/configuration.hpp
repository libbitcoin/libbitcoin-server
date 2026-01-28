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
#ifndef LIBBITCOIN_SERVER_CONFIGURATION_HPP
#define LIBBITCOIN_SERVER_CONFIGURATION_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Server node configuration, thread safe.
class BCS_API configuration
  : public node::configuration
{
public:
    configuration(system::chain::selection context,
        const server::settings::embedded_pages& native,
        const server::settings::embedded_pages& web) NOEXCEPT;

    /// Environment.
    std::filesystem::path file{};

    /// Information.
    bool help{};
    bool hardware{};
    bool settings{};
    bool version{};

    /// Actions.
    bool newstore{};
    bool backup{};
    bool restore{};

    /// Chain scans.
    bool flags{};
    bool information{};
    bool slabs{};
    bool buckets{};
    bool collisions{};

    /// Ad-hoc Testing.
    system::config::hash256 test{};
    system::config::hash256 write{};

    /// Settings.
    log::settings log;
    server::settings server;
};

} // namespace server
} // namespace libbitcoin

#endif
