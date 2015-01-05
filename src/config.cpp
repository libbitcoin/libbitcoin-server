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
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <libconfig.h++>
#include "config.hpp"
#include "echo.hpp"

namespace server {

void load_nodes(const libconfig::Setting& root, config_type& config)
{
    try
    {
        const libconfig::Setting& setting = root["nodes"];
        for (int i = 0; i < setting.getLength(); ++i)
        {
            const libconfig::Setting& node_setting = setting[i];
            node_config_object node;
            node.hostname = (const char*)node_setting[0];
            node.port = (unsigned int)node_setting[1];
            config.nodes.push_back(node);
        }
    }
    catch (const libconfig::SettingTypeException)
    {
        std::cerr << "Incorrectly formed nodes setting in config." << std::endl;
    }
    catch (const libconfig::SettingNotFoundException&) {}
}

void load_whitelist(const libconfig::Setting& root, config_type& config)
{
    try
    {
        const libconfig::Setting& setting = root["whitelist"];
        for (int i = 0; i < setting.getLength(); ++i)
        {
            std::string address = (const char*)setting[i];
            config.whitelist.push_back(address);
        }
    }
    catch (const libconfig::SettingTypeException)
    {
        std::cerr << "Incorrectly formed whitelist setting in config."
            << std::endl;
    }
    catch (const libconfig::SettingNotFoundException&) {}
}

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
std::string system_config_directory()
{
    // Use explicitly wide char functions and compile for unicode.
    char app_data_path[MAX_PATH];
    auto result = SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL,
        SHGFP_TYPE_CURRENT, app_data_path);
    if (SUCCEEDED(result))
        return std::string(app_data_path);

    return "";
}
#else
std::string system_config_directory()
{
    return std::string(SYSCONFDIR);
}
#endif

void set_config_path(libconfig::Config& configuration, 
    const boost::filesystem::path& config_path)
{
    // Ignore error if unable to read config file.
    try
    {
        // libconfig is ANSI/MBCS on Windows - no Unicode support.
        // This translates the path from Unicode to a "generic" path in
        // ANSI/MBCS, which can result in failures.
        configuration.readFile(config_path.generic_string().c_str());
    }
    catch (const libconfig::FileIOException&) {}
    catch (const libconfig::ParseException&) {}
}

void load_config(config_type& config, boost::filesystem::path& config_path)
{
    // Load values from config file.
    echo() << "Using config file: " << config_path;

    libconfig::Config configuration;
    set_config_path(configuration, config_path);

    // Read off values.
    // libconfig is ANSI/MBCS on Windows - no Unicode support.
    // This reads ANSI/MBCS values from XML. If they are UTF-8 (and above the
    // ASCII band) the values will be misinterpreted upon use.
    const libconfig::Setting& root = configuration.getRoot();
    root.lookupValue("output-file", config.output_file);
    root.lookupValue("error-file", config.error_file);
    root.lookupValue("blockchain-path", config.blockchain_path);
    root.lookupValue("hosts-file", config.hosts_file);
    root.lookupValue("service", config.service);
    root.lookupValue("heartbeat", config.heartbeat);
    root.lookupValue("publisher_enabled", config.publisher_enabled);
    root.lookupValue("block-publish", config.block_publish);
    root.lookupValue("tx-publish", config.tx_publish);
    root.lookupValue("certificate", config.certificate);
    root.lookupValue("client-allowed-certs", config.client_allowed_certs);
    load_whitelist(root, config);
    root.lookupValue("name", config.name);
    root.lookupValue("outgoing-connections", config.outgoing_connections);
    root.lookupValue("listener_enabled", config.listener_enabled);
    load_nodes(root, config);
    root.lookupValue("log_requests", config.log_requests);
    root.lookupValue("history_db_active_height",
        config.history_db_active_height);
}

} // namespace server

