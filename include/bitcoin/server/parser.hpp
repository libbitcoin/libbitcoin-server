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
#ifndef LIBBITCOIN_SERVER_PARSER_HPP
#define LIBBITCOIN_SERVER_PARSER_HPP

#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

// Not localizable.
#define BS_HELP_VARIABLE "help"
#define BS_HARDWARE_VARIABLE "hardware"
#define BS_SETTINGS_VARIABLE "settings"
#define BS_VERSION_VARIABLE "version"
#define BS_NEWSTORE_VARIABLE "newstore"
#define BS_BACKUP_VARIABLE "backup"
#define BS_RESTORE_VARIABLE "restore"

#define BS_FLAGS_VARIABLE "flags"
#define BS_SLABS_VARIABLE "slabs"
#define BS_BUCKETS_VARIABLE "buckets"
#define BS_COLLISIONS_VARIABLE "collisions"
#define BS_INFORMATION_VARIABLE "information"

#define BS_READ_VARIABLE "test"
#define BS_WRITE_VARIABLE "write"

// This must be lower case but the env var part can be any case.
#define BS_CONFIG_VARIABLE "config"

// This must match the case of the env var.
#define BS_ENVIRONMENT_VARIABLE_PREFIX "BS_"

namespace libbitcoin {
namespace server {

/// Parse configurable values from environment variables, settings file, and
/// command line positional and non-positional options.
class BCS_API parser
  : public system::config::parser
{
public:
    parser(system::chain::selection context,
        const server::settings::embedded_pages& explore,
        const server::settings::embedded_pages& web) NOEXCEPT;

    /// Load command line options (named).
    virtual options_metadata load_options() THROWS;

    /// Load command line arguments (positional).
    virtual arguments_metadata load_arguments() THROWS;

    /// Load environment variable settings.
    virtual options_metadata load_environment() THROWS;

    /// Load configuration file settings.
    virtual options_metadata load_settings() THROWS;

    /// Parse all configuration into member settings.
    virtual bool parse(int argc, const char* argv[],
        std::ostream& error) THROWS;

    /// The populated configuration settings values.
    configuration configured;
};

} // namespace server
} // namespace libbitcoin

#endif
