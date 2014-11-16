#ifndef OBELISK_WORKER_CONFIG_HPP
#define OBELISK_WORKER_CONFIG_HPP

#include <map>
#include <stdint.h>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>

namespace obelisk {

struct node_config_object
{
    std::string hostname;
    uint16_t port;
};

struct config_type
{
    typedef std::vector<node_config_object> nodes_list;
    typedef std::vector<std::string> ipaddress_list;

    std::string output_file = "debug.log";
    std::string error_file = "error.log";
    std::string blockchain_path = "blockchain/";
    std::string hosts_file = "hosts";
    std::string service = "tcp://*:9091";
    std::string heartbeat = "tcp://*:9092";
    bool publisher_enabled = false;
    std::string block_publish;
    std::string tx_publish;
    std::string certificate = "";
    ipaddress_list whitelist;
    std::string client_allowed_certs = "ALLOW_ALL_CERTS";
    std::string name;
    unsigned int outgoing_connections = 8;
    bool listener_enabled = true;
    nodes_list nodes;
    bool log_requests = false;
};

typedef std::map<std::string, std::string> config_map_type;
void load_config(config_type& config, 
    boost::filesystem::path& config_path);
std::string system_config_directory();

} // namespace obelisk

#endif
