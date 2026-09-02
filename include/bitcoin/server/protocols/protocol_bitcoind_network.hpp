/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_NETWORK_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_NETWORK_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_network>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_network>;

class BCS_API protocol_bitcoind_network
  : public protocol_bitcoind_dispatch<interface::bitcoind_network>,
    protected network::tracker<protocol_bitcoind_network>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_network> ptr;
    using rpc_interface = interface::bitcoind_network;

    inline protocol_bitcoind_network(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_network>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_get_network_info(const code& ec,
        rpc_interface::get_network_info) NOEXCEPT;
    bool handle_clear_banned(const code& ec,
        rpc_interface::clear_banned) NOEXCEPT;
    bool handle_list_banned(const code& ec,
        rpc_interface::list_banned) NOEXCEPT;
    bool handle_set_ban(const code& ec,
        rpc_interface::set_ban) NOEXCEPT;
    bool handle_add_node(const code& ec,
        rpc_interface::add_node, const std::string& node,
        const std::string& command, bool v2transport) NOEXCEPT;
    bool handle_disconnect_node(const code& ec,
        rpc_interface::disconnect_node) NOEXCEPT;
    bool handle_export_asmap(const code& ec,
        rpc_interface::export_asmap, const std::string&) NOEXCEPT;
    bool handle_get_added_node_info(const code& ec,
        rpc_interface::get_added_node_info) NOEXCEPT;
    bool handle_get_addrman_info(const code& ec,
        rpc_interface::get_addrman_info) NOEXCEPT;
    bool handle_get_node_addresses(const code& ec,
        rpc_interface::get_node_addresses, double count,
        const std::string& network) NOEXCEPT;
    bool handle_ping(const code& ec, rpc_interface::ping) NOEXCEPT;
    bool handle_set_network_active(const code& ec,
        rpc_interface::set_network_active, bool state) NOEXCEPT;
    bool handle_get_connection_count(const code& ec,
        rpc_interface::get_connection_count) NOEXCEPT;
    bool handle_get_net_totals(const code& ec,
        rpc_interface::get_net_totals) NOEXCEPT;
    bool handle_get_peer_info(const code& ec,
        rpc_interface::get_peer_info) NOEXCEPT;

private:
    /// Address dump completion (bounced to the channel strand).
    void handle_dump_nodes(const code& ec,
        const network::address_cptr& message) NOEXCEPT;
    void do_send_nodes(const code& ec,
        const network::address_cptr& message) NOEXCEPT;

    // These are protected by strand.
    size_t node_count_{};
    std::string node_network_{};
};

} // namespace server
} // namespace libbitcoin

#endif
