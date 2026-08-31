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
#ifndef LIBBITCOIN_SERVER_PARSER_HPP
#define LIBBITCOIN_SERVER_PARSER_HPP

#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

/// Parse configurable values from environment variables, settings file, and
/// command line positional and non-positional options.
class BCS_API parser
  : public system::config::parser
{
public:
    /// Environment variable prefix, case must match the env var.
    static constexpr auto environment_prefix = "BS_";

    /// Command line and environment variable names.
    static constexpr auto help_variable = "help";
    static constexpr auto hardware_variable = "hardware";
    static constexpr auto settings_variable = "settings";
    static constexpr auto version_variable = "version";
    static constexpr auto newstore_variable = "newstore";
    static constexpr auto backup_variable = "backup";
    static constexpr auto restore_variable = "restore";
    static constexpr auto daemon_variable = "daemon";
    static constexpr auto user_variable = "user";
    static constexpr auto flags_variable = "flags";
    static constexpr auto buckets_variable = "buckets";
    static constexpr auto collisions_variable = "collisions";
    static constexpr auto information_variable = "information";
    static constexpr auto get_variable = "get";
    static constexpr auto put_variable = "put";
    static constexpr auto config_variable = "config";

    parser(system::chain::selection context,
        const server::settings::embedded_pages& native,
        const server::settings::embedded_pages& admin) NOEXCEPT;

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
