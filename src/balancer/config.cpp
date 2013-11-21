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
    std::cout << "Using config file: " << filename << std::endl;
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
    get_value<std::string>(root, config, "frontend", "tcp://*:9091");
    get_value<std::string>(root, config, "backend", "tcp://*:9092");
}

} // namespace obelisk

