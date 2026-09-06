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
#ifndef LIBBITCOIN_SERVER_INTERFACES_BITCOIND_ZMQ_HPP
#define LIBBITCOIN_SERVER_INTERFACES_BITCOIND_ZMQ_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

/// The bitcoind zmq notification interface: a set of published topics, not
/// json-rpc methods. Each notification is a three frame message of topic,
/// body and a per-topic 32-bit little-endian sequence (bitcoind doc/zmq.md).
struct bitcoind_zmq_topics
{
    /// Topic names (the subscription prefixes).
    static constexpr std::string_view hash_block{ "hashblock" };
    static constexpr std::string_view raw_block{ "rawblock" };
    static constexpr std::string_view hash_tx{ "hashtx" };
    static constexpr std::string_view raw_tx{ "rawtx" };
    static constexpr std::string_view sequence{ "sequence" };

    /// All topics (getzmqnotifications enumerates these per binding).
    static constexpr std::array<std::string_view, 5> names
    {
        hash_block, raw_block, hash_tx, raw_tx, sequence
    };

    /// Sequence topic labels, following the reversed 32 byte hash.
    static constexpr uint8_t block_connected{ 'C' };
    static constexpr uint8_t block_disconnected{ 'D' };
    static constexpr uint8_t transaction_accepted{ 'A' };
    static constexpr uint8_t transaction_removed{ 'R' };
};

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
