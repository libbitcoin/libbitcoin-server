#ifndef OBELISK_CLIENT_INTERFACE
#define OBELISK_CLIENT_INTERFACE

#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/backend.hpp>

class blockchain_interface
{
public:
    blockchain_interface(backend_cluster& backend);
    void fetch_history(const bc::payment_address& address,
        bc::blockchain::fetch_handler_history handle_fetch);
private:
    backend_cluster& backend_;
};

class fullnode_interface
{
public:
    fullnode_interface();
    void update();
    blockchain_interface blockchain;
private:
    backend_cluster backend_;
};

#endif

