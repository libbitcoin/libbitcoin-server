#ifndef OBELISK_CLIENT_INTERFACE
#define OBELISK_CLIENT_INTERFACE

#include <obelisk/client/blockchain.hpp>

class fullnode_interface
{
public:
    fullnode_interface(const std::string& connection);
    void update();
    void stop(const std::string& secret);
    blockchain_interface blockchain;
private:
    backend_cluster backend_;
};

#endif

