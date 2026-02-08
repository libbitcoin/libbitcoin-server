/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_INTERFACES_ELECTRUM_HPP
#define LIBBITCOIN_SERVER_INTERFACES_ELECTRUM_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct electrum_methods
{
    /// Electrum protocol version 1.4.2
    static constexpr std::tuple methods
    {
        /// Blockchain methods.
        method<"blockchain.block.header", number_t, number_t>{ "height", "cp_height" },
        method<"blockchain.block.headers", number_t, number_t, number_t>{ "start_height", "count", "cp_height" },
        method<"blockchain.headers.subscribe">{},
        method<"blockchain.estimatefee", number_t, optional<""_t>>{ "number", "mode" },
        method<"blockchain.relayfee">{},
        method<"blockchain.scripthash.get_balance", string_t>{ "scripthash" },
        method<"blockchain.scripthash.get_history", string_t>{ "scripthash" },
        method<"blockchain.scripthash.get_mempool", string_t>{ "scripthash" },
        method<"blockchain.scripthash.listunspent", string_t>{ "scripthash" },
        method<"blockchain.scripthash.subscribe", string_t>{ "scripthash" },
        method<"blockchain.scripthash.unsubscribe", string_t>{ "scripthash" },
        method<"blockchain.transaction.broadcast", string_t>{ "raw_tx" },
        method<"blockchain.transaction.get", string_t, boolean_t>{ "tx_hash", "verbose" },
        method<"blockchain.transaction.get_merkle", string_t, number_t>{ "tx_hash", "height" },
        method<"blockchain.transaction.id_from_pos", number_t, number_t, boolean_t>{ "height", "tx_pos", "merkle" },

        /// Server methods.
        method<"server.add_peer", object_t>{ "features" },
        method<"server.banner">{},
        method<"server.donation_address">{},
        method<"server.features">{},
        method<"server.peers.subscribe">{},
        method<"server.ping">{},
        method<"server.version", string_t, optional<empty::value>>{ "client_name", "protocol_version" },

        /// Mempool methods.
        method<"mempool.get_fee_histogram">{}
    };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.
    using blockchain_block_header = at<0>;
    using blockchain_block_headers = at<1>;
    using blockchain_headers_subscribe = at<2>;
    using blockchain_estimate_fee = at<3>;
    using blockchain_relay_fee = at<4>;
    using blockchain_scripthash_get_balance = at<5>;
    using blockchain_scripthash_get_history = at<6>;
    using blockchain_scripthash_get_mempool = at<7>;
    using blockchain_scripthash_listunspent = at<8>;
    using blockchain_scripthash_subscribe = at<9>;
    using blockchain_scripthash_unsubscribe = at<10>;
    using blockchain_transaction_broadcast = at<11>;
    using blockchain_transaction_get = at<12>;
    using blockchain_transaction_get_merkle = at<13>;
    using blockchain_transaction_id_from_pos = at<14>;
    using server_add_peer = at<15>;
    using server_banner = at<16>;
    using server_donation_address = at<17>;
    using server_features = at<18>;
    using server_peers_subscribe = at<19>;
    using server_ping = at<20>;
    using server_version = at<21>;
    using mempool_get_fee_histogram = at<22>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
