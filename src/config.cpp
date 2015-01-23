/*
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.hpp"

#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/bitcoin.hpp>
#include "metadata.hpp"

// Not localizable.
#define BS_HELP_VARIABLE "help"
#define BS_CONFIG_VARIABLE "config"
#define BS_ENVIRONMENT_VARIABLE_PREFIX "BS"

namespace libbitcoin {
namespace server {

using boost::format;
using boost::filesystem::path;
using namespace boost::program_options;

path get_config_option(variables_map& variables)
{
    // Read config from the map so we don't require an early notify call.
    const auto& config = variables[BS_CONFIG_VARIABLE];

    // prevent exception in the case where the config variable is not set.
    if (config.empty())
        return path();

    return config.as<path>();
}

bool get_help_option(variables_map& variables)
{
    // Read help from the map so we don't require an early notify call.
    const auto& help = variables[BS_HELP_VARIABLE];

    // Prevent exception in the case where the help variable is not set.
    if (help.empty())
        return false;

    return help.as<bool>();
}

void load_command_variables(variables_map& variables, metadata_type& metadata,
    int argc, const char* argv[]) throw()
{
    // load metadata
    const auto& options = metadata.load_options();
    const auto& arguments = metadata.load_arguments();

    // parse inputs
    auto command_parser = command_line_parser(argc, argv).options(options)
        .positional(arguments);

    // map parsed inputs into variables map
    store(command_parser.run(), variables);
}

// Not unit testable (without creating actual config files).
void load_configuration_variables(path& config_path, variables_map& variables, 
    metadata_type& metadata) throw(reading_file)
{
    // load metadata
    const auto& config_settings = metadata.load_settings();

    // override the default config path if specified on the command line
    const auto config_path_arg = get_config_option(variables);
    if (!config_path_arg.empty())
        config_path = config_path_arg;

    if (config_path.empty())
    {
        // loading from an empty stream causes the defaults to populate
        std::stringstream stream;

        // parse inputs
        const auto configuration = parse_config_file(stream, config_settings);

        // map parsed inputs into variables map
        store(configuration, variables);
    }
    else
    {
        // return an empty path if the file does not load
        const auto& path = config_path.generic_string();
        std::ifstream file(path);
        if (!file.good())
        {
            config_path.clear();
            return;
        }

        // parse inputs
        const auto configuration = parse_config_file(file, config_settings);

        // map parsed inputs into variables map
        store(configuration, variables);
    }
}

void load_environment_variables(variables_map& variables, 
    metadata_type& metadata) throw()
{
    // load metadata
    const auto& environment_variables = metadata.load_environment();

    // parse inputs
    const auto environment = parse_environment(environment_variables,
        BS_ENVIRONMENT_VARIABLE_PREFIX);

    // map parsed inputs into variables map
    store(environment, variables);
}

bool load_config(config_type& config, std::string& message, path& config_path,
    int argc, const char* argv[])
{
    try
    {
        metadata_type metadata;
        variables_map variables;
        load_command_variables(variables, metadata, argc, argv);

        // Don't load rest if help is specified.
        if (!get_help_option(variables))
        {
            // Must store before configuration in order to specify the path.
            load_environment_variables(variables, metadata);

            // Is lowest priority, which will cause confusion if there is
            // composition between them, which therefore should be avoided.
            // Returns config_path or empty if not found/used.
            load_configuration_variables(config_path, variables, metadata);

            // Send notifications and update bound variables.
            notify(variables);
        }
    }
    catch (const boost::program_options::error& e)
    {
        // BUGBUG: the error message is obtained directly form boost, which
        // circumvents our localization model.
        message = e.what();
        return false;
    }
    catch (...)
    {
        message = "...";
        return false;
    }

    return true;
}

} // namespace server
} // namespace libbitcoin

