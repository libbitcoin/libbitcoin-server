#ifndef QUERY_PUBLISHER_HPP
#define QUERY_PUBLISHER_HPP

#include <zmq.hpp>
#include <bitcoin/bitcoin.hpp>

#include "config.hpp"
#include "node_impl.hpp"

namespace obelisk {

class publisher
{
public:
    publisher(node_impl& node);
    bool start(config_type& config);
    bool stop();

private:
    typedef std::unique_ptr<zmq::socket_t> zmq_socket_uniqptr;

    bool setup_socket(const std::string& connection,
        zmq_socket_uniqptr& socket);

    bool send_blk(uint32_t height, const bc::block_type& blk);
    bool send_tx(const bc::transaction_type& tx);

    zmq::context_t context_;
    node_impl& node_;
    zmq_socket_uniqptr socket_block_, socket_tx_;
};

} // namespace obelisk

#endif

