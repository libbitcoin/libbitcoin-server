#ifndef OBELISK_WORKER_CONFIG_HPP
#define OBELISK_WORKER_CONFIG_HPP

#include <map>
#include <string>
#include <vector>

namespace obelisk {

struct node_config_object
{
    std::string hostname;
    uint16_t port;
};

struct config_type
{
    typedef std::vector<node_config_object> nodes_list;

    std::string output_file = "debug.log";
    std::string error_file = "error.log";
    std::string blockchain_path = "blockchain/";
    std::string hosts_file = "hosts";
    std::string service = "tcp://localhost:9092";
    bool publisher_enabled = false;
    std::string block_publish;
    std::string tx_publish;
    std::string name;
    unsigned int outgoing_connections = 8;
    bool listener_enabled = true;
    nodes_list nodes;
};

void load_config(config_type& config, const std::string& config_path);

} // namespace obelisk

#endif

