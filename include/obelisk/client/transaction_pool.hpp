#ifndef OBELISK_CLIENT_TRANSACTION_POOL_HPP
#define OBELISK_CLIENT_TRANSACTION_POOL_HPP

#include <obelisk/client/backend.hpp>
#include <obelisk/define.hpp>

namespace obelisk {

class transaction_pool_interface
{
public:
    BCS_API transaction_pool_interface(backend_cluster& backend);
    BCS_API void validate(const bc::transaction_type& tx,
        bc::transaction_pool::validate_handler handle_validate);
    BCS_API void fetch_transaction(const bc::hash_digest& tx_hash,
        bc::transaction_pool::fetch_handler handle_fetch);
private:
    backend_cluster& backend_;
};

} // namespace obelisk

#endif

