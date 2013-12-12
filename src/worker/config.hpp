#ifndef OBELISK_WORKER_CONFIG_HPP
#define OBELISK_WORKER_CONFIG_HPP

#include <map>
#include <string>

namespace obelisk {

struct config_type
{
    std::string output_file = "debug.log";
    std::string error_file = "error.log";
    std::string blockchain_path = "blockchain/";
    std::string service = "tcp://localhost:9092";
    bool publisher_enabled = false;
    std::string block_publish;
    std::string tx_publish;
    std::string name;
    unsigned int outgoing_connections = 8;
    bool listener_enabled = true;
};

void load_config(config_type& config, const std::string& config_path);

} // namespace obelisk

#endif

