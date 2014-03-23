#include "config.hpp"

#include <boost/lexical_cast.hpp>
#include <libconfig.h++>
#include "echo.hpp"

namespace obelisk {

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

void load_config(config_type& config, const std::string& filename)
{
    // Load values from config file.
    echo() << "Using config file: " << filename;
    libconfig::Config cfg;
    // Ignore error if unable to read config file.
    try
    {
        cfg.readFile(filename.c_str());
    }
    catch (const libconfig::FileIOException&) {}
    catch (const libconfig::ParseException&) {}
    // Read off values
    const libconfig::Setting& root = cfg.getRoot();
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
}

} // namespace obelisk

