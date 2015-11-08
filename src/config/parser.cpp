/**
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
#include <bitcoin/server/config/parser.hpp>

#include <string>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/throw_exception.hpp>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace server {

using boost::format;
using namespace boost::filesystem;
using namespace boost::program_options;
using namespace boost::system;

static path get_config_option(variables_map& variables)
{
    // read config from the map so we don't require an early notify
    const auto& config = variables[BS_CONFIG_VARIABLE];

    // prevent exception in the case where the config variable is not set
    if (config.empty())
        return path();

    return config.as<path>();
}

static bool get_option(variables_map& variables, const std::string& name)
{
    // Read settings from the map so we don't require an early notify call.
    const auto& variable = variables[name];

    // prevent exception in the case where the settings variable is not set.
    if (variable.empty())
        return false;

    return variable.as<bool>();
}

static void load_command_variables(variables_map& variables, parser& metadata,
    int argc, const char* argv[])
{
    const auto options = metadata.load_options();
    const auto arguments = metadata.load_arguments();
    auto command_parser = command_line_parser(argc, argv).options(options)
        .positional(arguments);
    store(command_parser.run(), variables);
}

static bool load_configuration_variables(variables_map& variables,
    parser& metadata)
{
    const auto config_settings = metadata.load_settings();
    const auto config_path = get_config_option(variables);

    // If the existence test errors out we pretend there's no file :/.
    error_code code;
    if (!config_path.empty() && exists(config_path, code))
    {
        bc::ifstream file(config_path.string());
        if (!file.good())
        {
            BOOST_THROW_EXCEPTION(reading_file(config_path.string().c_str()));
        }

        const auto config = parse_config_file(file, config_settings);
        store(config, variables);
        return true;
    }

    // Loading from an empty stream causes the defaults to populate.
    std::stringstream stream;
    const auto config = parse_config_file(stream, config_settings);
    store(config, variables);
    return false;
}

static void load_environment_variables(variables_map& variables,
    parser& metadata)
{
    const auto& environment_variables = metadata.load_environment();
    const auto environment = parse_environment(environment_variables,
        BS_ENVIRONMENT_VARIABLE_PREFIX);
    store(environment, variables);
}

bool parser::parse(parser& metadata, std::string& message, int argc,
    const char* argv[])
{
    try
    {
        auto file = false;
        variables_map variables;
        load_command_variables(variables, metadata, argc, argv);
        load_environment_variables(variables, metadata);

        // Don't load the rest if any of these options are specified.
        if (!get_option(variables, BS_VERSION_VARIABLE) && 
            !get_option(variables, BS_SETTINGS_VARIABLE) &&
            !get_option(variables, BS_HELP_VARIABLE))
        {
            // Returns true if the settings were loaded from a file.
            file = load_configuration_variables(variables, metadata);
        }

        // Update bound variables in metadata.settings.
        notify(variables);

        // Clear the config file path if it wasn't used.
        if (!file)
            metadata.settings.file.clear();
    }
    catch (const boost::program_options::error& e)
    {
        // This is obtained from boost, which circumvents our localization.
        message = e.what();
        return false;
    }

    return true;
}

} // namespace server
} // namespace libbitcoin
