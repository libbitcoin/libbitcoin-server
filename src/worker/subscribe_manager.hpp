#ifndef OBELISK_WORKER_SUBSCRIBE_MANAGER_HPP
#define OBELISK_WORKER_SUBSCRIBE_MANAGER_HPP

#include <unordered_map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <bitcoin/bitcoin.hpp>
#include <obelisk/message.hpp>
#include "node_impl.hpp"

namespace obelisk {

class subscribe_manager
{
public:
    subscribe_manager(node_impl& node);
    void subscribe(const incoming_message& request, zmq_socket_ptr socket);
    void renew(const incoming_message& request, zmq_socket_ptr socket);

    void submit(size_t height, const bc::hash_digest& block_hash,
        const bc::transaction_type& tx);

private:
    struct subscription
    {
        boost::posix_time::ptime expiry_time;
        const bc::data_chunk client_origin;
        const zmq_socket_ptr socket;
    };

    typedef std::unordered_multimap<bc::payment_address, subscription>
        subscription_map;

    void do_subscribe(const incoming_message& request, zmq_socket_ptr socket);
    void do_renew(const incoming_message& request, zmq_socket_ptr socket);

    void do_submit(
        size_t height, const bc::hash_digest& block_hash,
        const bc::transaction_type& tx);
    void post_updates(const bc::payment_address& address,
        size_t height, const bc::hash_digest& block_hash,
        const bc::transaction_type& tx);

    void sweep_expired();

    bc::async_strand strand_;
    subscription_map subs_;
};

} // namespace obelisk

#endif

