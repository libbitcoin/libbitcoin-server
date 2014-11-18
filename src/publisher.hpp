#ifndef QUERY_PUBLISHER_HPP
#define QUERY_PUBLISHER_HPP

#include <czmq++/czmqpp.hpp>
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
    bool setup_socket(
        const std::string& connection, czmqpp::socket& socket);

    void send_blk(uint32_t height, const bc::block_type& blk);
    void send_tx(const bc::transaction_type& tx);

    node_impl& node_;
    czmqpp::context context_;
    czmqpp::socket socket_block_, socket_tx_;
};

} // namespace obelisk

#endif

