/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
    /// Electrum protocol versions 1.0-1.6
    static constexpr std::tuple methods
    {
        /// Blockchain methods.
        method<"blockchain.numblocks.subscribe">{},
        method<"blockchain.block.get_chunk", number_t>{ "index" },
        method<"blockchain.block.get_header", number_t>{ "height" },
        method<"blockchain.block.header", number_t, optional<0.0>>{ "height", "cp_height" },
        method<"blockchain.block.headers", number_t, number_t, optional<0.0>>{ "start_height", "count", "cp_height" },
        method<"blockchain.headers.subscribe", optional<false>>{ "raw" },

        method<"blockchain.estimatefee", number_t, optional<""_t>>{ "number", "mode" },
        method<"blockchain.relayfee">{},

        method<"blockchain.utxo.get_address", string_t, number_t>{ "tx_hash", "index" },
        method<"blockchain.outpoint.get_status", string_t, number_t, optional<""_t>>{ "tx_hash", "txout_idx", "spk_hint" },
        method<"blockchain.outpoint.subscribe", string_t, number_t, optional<""_t>>{ "tx_hash", "txout_idx", "spk_hint" },
        method<"blockchain.outpoint.unsubscribe", string_t, number_t>{ "tx_hash", "txout_idx" },

        method<"blockchain.address.get_balance", string_t>{ "address" },
        method<"blockchain.address.get_history", string_t>{ "address" },
        method<"blockchain.address.get_mempool", string_t>{ "address" },
        method<"blockchain.address.listunspent", string_t>{ "address" },
        method<"blockchain.address.subscribe", string_t>{ "address" },

        method<"blockchain.scripthash.get_balance", string_t>{ "scripthash" },
        method<"blockchain.scripthash.get_history", string_t>{ "scripthash" },
        method<"blockchain.scripthash.get_mempool", string_t>{ "scripthash" },
        method<"blockchain.scripthash.listunspent", string_t>{ "scripthash" },
        method<"blockchain.scripthash.subscribe", string_t>{ "scripthash" },
        method<"blockchain.scripthash.unsubscribe", string_t>{ "scripthash" },

        method<"blockchain.scriptpubkey.get_balance", string_t>{ "scriptpubkey" },
        method<"blockchain.scriptpubkey.get_history", string_t>{ "scriptpubkey" },
        method<"blockchain.scriptpubkey.get_mempool", string_t>{ "scriptpubkey" },
        method<"blockchain.scriptpubkey.listunspent", string_t>{ "scriptpubkey" },
        method<"blockchain.scriptpubkey.subscribe", string_t>{ "scriptpubkey" },
        method<"blockchain.scriptpubkey.unsubscribe", string_t>{ "scriptpubkey" },

        method<"blockchain.transaction.broadcast", string_t>{ "raw_tx" },
        method<"blockchain.transaction.broadcast_package", value_t, optional<false>>{ "raw_txs", "verbose" },
        method<"blockchain.transaction.get", string_t, boolean_t>{ "tx_hash", "verbose" },
        method<"blockchain.transaction.get_merkle", string_t, number_t>{ "tx_hash", "height" },
        method<"blockchain.transaction.id_from_pos", number_t, number_t, optional<false>>{ "height", "tx_pos", "merkle" },

        /// Server methods.
        method<"server.add_peer", object_t>{ "features" },
        method<"server.banner">{},
        method<"server.donation_address">{},
        method<"server.features">{},
        method<"server.peers.subscribe">{},
        method<"server.ping", optional<0.0>, optional<""_t>>{ "pong_len", "data" },
        method<"server.version", string_t, optional<empty::value>>{ "client_name", "protocol_version" },

        /// Mempool methods.
        method<"mempool.get_fee_histogram">{},
        method<"mempool.get_info">{}
    };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    using blockchain_number_of_blocks_subscribe = at<0>;
    using blockchain_block_get_chunk = at<1>;
    using blockchain_block_get_header = at<2>;
    using blockchain_block_header = at<3>;
    using blockchain_block_headers = at<4>;
    using blockchain_headers_subscribe = at<5>;

    using blockchain_estimate_fee = at<6>;
    using blockchain_relay_fee = at<7>;

    using blockchain_utxo_get_address = at<8>;
    using blockchain_outpoint_get_status = at<9>;
    using blockchain_outpoint_subscribe = at<10>;
    using blockchain_outpoint_unsubscribe = at<11>;

    using blockchain_address_get_balance = at<12>;
    using blockchain_address_get_history = at<13>;
    using blockchain_address_get_mempool = at<14>;
    using blockchain_address_list_unspent = at<15>;
    using blockchain_address_subscribe = at<16>;

    using blockchain_scripthash_get_balance = at<17>;
    using blockchain_scripthash_get_history = at<18>;
    using blockchain_scripthash_get_mempool = at<19>;
    using blockchain_scripthash_list_unspent = at<20>;
    using blockchain_scripthash_subscribe = at<21>;
    using blockchain_scripthash_unsubscribe = at<22>;

    using blockchain_scriptpubkey_get_balance = at<23>;
    using blockchain_scriptpubkey_get_history = at<24>;
    using blockchain_scriptpubkey_get_mempool = at<25>;
    using blockchain_scriptpubkey_list_unspent = at<26>;
    using blockchain_scriptpubkey_subscribe = at<27>;
    using blockchain_scriptpubkey_unsubscribe = at<28>;

    using blockchain_transaction_broadcast = at<29>;
    using blockchain_transaction_broadcast_package = at<30>;
    using blockchain_transaction_get = at<31>;
    using blockchain_transaction_get_merkle = at<32>;
    using blockchain_transaction_id_from_position = at<33>;

    using server_add_peer = at<34>;
    using server_banner = at<35>;
    using server_donation_address = at<36>;
    using server_features = at<37>;
    using server_peers_subscribe = at<38>;
    using server_ping = at<39>;
    using server_version = at<40>;

    using mempool_get_fee_histogram = at<41>;
    using mempool_get_info = at<42>;
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
