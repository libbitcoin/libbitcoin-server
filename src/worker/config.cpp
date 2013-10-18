#include "config.hpp"

#include <boost/lexical_cast.hpp>
#include <libconfig.h++>

namespace obelisk {

template <typename T>
void get_value(const libconfig::Setting& root, config_map_type& config,
    const std::string& key_name, const T& fallback_value)
{
    T value;
    if (root.lookupValue(key_name, value))
        config[key_name] = boost::lexical_cast<std::string>(value);
    else
        config[key_name] = boost::lexical_cast<std::string>(fallback_value);
}

void load_config(config_map_type& config, const std::string& filename)
{
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
    get_value<std::string>(root, config, "output-file", "debug.log");
    get_value<std::string>(root, config, "error-file", "error.log");
    get_value<std::string>(root, config, "blockchain-path", "blockchain");
    get_value<std::string>(root, config, "service", "tcp://localhost:9092");
    get_value<std::string>(root, config, "publisher", "disabled");
    get_value<std::string>(root, config, "block-publish", "tcp://*:9093");
    get_value<std::string>(root, config, "tx-publish", "tcp://*:9094");
    get_value<std::string>(root, config, "name", "");
}

} // namespace obelisk

