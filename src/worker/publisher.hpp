#ifndef QUERY_PUBLISHER_HPP
#define QUERY_PUBLISHER_HPP

#include <czmq++/czmq.hpp>
#include <bitcoin/bitcoin.hpp>
#include <obelisk/define.hpp>
#include "config.hpp"
#include "node_impl.hpp"

namespace obelisk {

class publisher
{
public:
    BCS_API publisher(node_impl& node);
    BCS_API bool start(config_type& config);
    BCS_API bool stop();

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

