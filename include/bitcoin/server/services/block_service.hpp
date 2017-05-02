/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_BLOCK_SERVICE_HPP
#define LIBBITCOIN_SERVER_BLOCK_SERVICE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class server_node;

// This class is thread safe.
// Subscribe to block acceptances into the long chain.
class BCS_API block_service
  : public bc::protocol::zmq::worker
{
public:
    typedef std::shared_ptr<block_service> ptr;

    /// Construct a block service.
    block_service(bc::protocol::zmq::authenticator& authenticator,
        server_node& node, bool secure);

    /// Start the service.
    bool start() override;

protected:
    typedef bc::protocol::zmq::socket socket;

    virtual bool bind(socket& xpub, socket& xsub);
    virtual bool unbind(socket& xpub, socket& xsub);

    // Implement the service.
    virtual void work() override;

private:
    bool handle_reorganization(const code& ec, size_t fork_height,
        block_const_ptr_list_const_ptr incoming,
        block_const_ptr_list_const_ptr outgoing);

    void publish_blocks(uint32_t fork_height,
        block_const_ptr_list_const_ptr blocks);
    void publish_block(socket& publisher, size_t height,
        block_const_ptr block);

    // These are thread safe.
    const bool secure_;
    const std::string security_;
    const bc::server::settings& settings_;
    const bc::protocol::settings& external_;
    const bc::protocol::settings internal_;
    const config::endpoint service_;
    const config::endpoint worker_;
    bc::protocol::zmq::authenticator& authenticator_;
    server_node& node_;

    // This is protected by reorganization non-concurrency.
    uint16_t sequence_;
};

} // namespace server
} // namespace libbitcoin

#endif
