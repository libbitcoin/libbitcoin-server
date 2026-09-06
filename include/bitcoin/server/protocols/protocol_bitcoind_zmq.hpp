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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_ZMQ_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_ZMQ_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

/// bitcoind zmq notifications over native zmtp. Subscriptions are topic
/// prefixes recorded from the subscriber (bounded by configuration), and
/// notifications are published for chaser events to the matching topics with
/// per-topic sequences (bitcoind semantics). The socket frames each
/// notification as [topic][body][32-bit little-endian sequence].
class BCS_API protocol_bitcoind_zmq
  : public protocol_rpc<channel_bitcoind_zmq>,
    protected network::tracker<protocol_bitcoind_zmq>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_zmq> ptr;
    using rpc_interface = interface::bitcoind_zmq;

    /// Topics (bitcoind doc/zmq.md).
    static constexpr std::string_view hash_block{ "hashblock" };
    static constexpr std::string_view raw_block{ "rawblock" };
    static constexpr std::string_view hash_tx{ "hashtx" };
    static constexpr std::string_view raw_tx{ "rawtx" };
    static constexpr std::string_view sequence{ "sequence" };

    /// All topics (getzmqnotifications enumerates these per binding).
    static constexpr std::array<std::string_view, 5> topics
    {
        hash_block, raw_block, hash_tx, raw_tx, sequence
    };

    /// Sequence topic labels, following the reversed 32 byte hash.
    enum class label : uint8_t
    {
        block_connected = 'C',
        block_disconnected = 'D',
        transaction_accepted = 'A',
        transaction_removed = 'R'
    };

    inline protocol_bitcoind_zmq(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<channel_bitcoind_zmq>(session, channel, options),
        options_(options),
        network::tracker<protocol_bitcoind_zmq>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// The topic matches a subscription prefix (empty matches every topic).
    static bool subscribed(const system::data_stack& subscriptions,
        std::string_view topic) NOEXCEPT;

    /// The sequence topic body: reversed hash then label.
    static system::data_chunk sequence_body(const system::hash_digest& hash,
        label label) NOEXCEPT;

    /// Handlers (the method is native, its first parameter a shared_ptr).
    bool handle_subscribe(const code& ec, const system::chunk_cptr& prefix,
        bool stop) NOEXCEPT;

    /// Event handlers.
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;

    /// Notifications (stranded).
    void do_organized(node::header_t link) NOEXCEPT;
    void publish(std::string_view topic, system::data_chunk&& body) NOEXCEPT;

private:
    static size_t index(std::string_view topic) NOEXCEPT;

    // This is thread safe.
    const options_t& options_;

    // These are protected by the channel strand.
    system::data_stack subscriptions_{};
    std::array<uint32_t, topics.size()> sequences_{};
};

} // namespace server
} // namespace libbitcoin

#endif
