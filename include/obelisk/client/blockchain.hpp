#ifndef OBELISK_CLIENT_BLOCKCHAIN
#define OBELISK_CLIENT_BLOCKCHAIN

#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/backend.hpp>

class blockchain_interface
{
public:
    blockchain_interface(backend_cluster& backend);
    void fetch_history(const bc::payment_address& address,
        bc::blockchain::fetch_handler_history handle_fetch);
    void fetch_transaction(const bc::hash_digest& tx_hash,
        bc::blockchain::fetch_handler_transaction handle_fetch);
    void fetch_last_height(
        bc::blockchain::fetch_handler_last_height handle_fetch);
private:
    backend_cluster& backend_;
};

#endif

