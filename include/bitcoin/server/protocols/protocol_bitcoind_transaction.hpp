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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_TRANSACTION_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_TRANSACTION_HPP

#include <memory>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_transaction>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_transaction>;

class BCS_API protocol_bitcoind_transaction
  : public protocol_bitcoind_dispatch<interface::bitcoind_transaction>,
    protected network::tracker<protocol_bitcoind_transaction>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_transaction> ptr;
    using rpc_interface = interface::bitcoind_transaction;

    inline protocol_bitcoind_transaction(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_transaction>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_create_raw_transaction(const code& ec,
        rpc_interface::create_raw_transaction,
        const network::rpc::array_t& inputs,
        const network::rpc::value_t& outputs, double locktime,
        bool replaceable, double version) NOEXCEPT;
    bool handle_decode_raw_transaction(const code& ec,
        rpc_interface::decode_raw_transaction, const std::string& hexstring,
        const std::optional<bool>& iswitness) NOEXCEPT;
    bool handle_get_raw_transaction(const code& ec,
        rpc_interface::get_raw_transaction, const std::string& txid,
        double verbose, const std::string& blockhash) NOEXCEPT;
    bool handle_send_raw_transaction(const code& ec,
        rpc_interface::send_raw_transaction, const std::string& hexstring,
        double maxfeerate, double maxburnamount) NOEXCEPT;
    bool handle_test_mempool_accept(const code& ec,
        rpc_interface::test_mempool_accept,
        const network::rpc::array_t& rawtxs, double maxfeerate) NOEXCEPT;
    bool handle_analyze_psbt(const code& ec,
        rpc_interface::analyze_psbt, const std::string& psbt) NOEXCEPT;
    bool handle_combine_psbt(const code& ec,
        rpc_interface::combine_psbt,
        const network::rpc::array_t& txs) NOEXCEPT;
    bool handle_convert_to_psbt(const code& ec,
        rpc_interface::convert_to_psbt, const std::string& hexstring,
        bool permitsigdata, const std::optional<bool>& iswitness,
        double psbt_version) NOEXCEPT;
    bool handle_create_psbt(const code& ec,
        rpc_interface::create_psbt, const network::rpc::array_t& inputs,
        const network::rpc::value_t& outputs, double locktime,
        bool replaceable, double version, double psbt_version) NOEXCEPT;
    bool handle_decode_psbt(const code& ec,
        rpc_interface::decode_psbt, const std::string& psbt) NOEXCEPT;
    bool handle_finalize_psbt(const code& ec,
        rpc_interface::finalize_psbt, const std::string& psbt,
        bool extract) NOEXCEPT;
    bool handle_join_psbts(const code& ec,
        rpc_interface::join_psbts,
        const network::rpc::array_t& txs) NOEXCEPT;
    bool handle_descriptor_process_psbt(const code& ec,
        rpc_interface::descriptor_process_psbt) NOEXCEPT;
    bool handle_utxo_update_psbt(const code& ec,
        rpc_interface::utxo_update_psbt, const std::string& psbt,
        const network::rpc::array_t& descriptors) NOEXCEPT;

    bool handle_abort_private_broadcast(const code& ec,
        rpc_interface::abort_private_broadcast) NOEXCEPT;
    bool handle_get_private_broadcast_info(const code& ec,
        rpc_interface::get_private_broadcast_info) NOEXCEPT;
    bool handle_submit_package(const code& ec,
        rpc_interface::submit_package) NOEXCEPT;
    bool handle_combine_raw_transaction(const code& ec,
        rpc_interface::combine_raw_transaction,
        const network::rpc::array_t& txs) NOEXCEPT;
    bool handle_sign_raw_transaction_with_key(const code& ec,
        rpc_interface::sign_raw_transaction_with_key) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
