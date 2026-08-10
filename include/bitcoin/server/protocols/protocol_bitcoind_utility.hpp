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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_UTILITY_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_UTILITY_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_utility>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_utility>;

class BCS_API protocol_bitcoind_utility
  : public protocol_bitcoind_dispatch<interface::bitcoind_utility>,
    protected network::tracker<protocol_bitcoind_utility>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_utility> ptr;
    using rpc_interface = interface::bitcoind_utility;

    inline protocol_bitcoind_utility(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_utility>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_decode_script(const code& ec,
        rpc_interface::decode_script, const std::string& hex) NOEXCEPT;
    bool handle_validate_address(const code& ec,
        rpc_interface::validate_address,
        const std::string& address) NOEXCEPT;
    bool handle_create_multisig(const code& ec,
        rpc_interface::create_multisig, double nrequired,
        const network::rpc::array_t& keys,
        const std::string& address_type) NOEXCEPT;
    bool handle_derive_addresses(const code& ec,
        rpc_interface::derive_addresses) NOEXCEPT;
    bool handle_get_descriptor_info(const code& ec,
        rpc_interface::get_descriptor_info) NOEXCEPT;
    bool handle_verify_message(const code& ec,
        rpc_interface::verify_message, const std::string& address,
        const std::string& signature, const std::string& message) NOEXCEPT;
    bool handle_get_index_info(const code& ec,
        rpc_interface::get_index_info, const std::string& index_name) NOEXCEPT;
    bool handle_estimate_smart_fee(const code& ec,
        rpc_interface::estimate_smart_fee) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
