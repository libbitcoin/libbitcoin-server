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

#include <atomic>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

/// bitcoind zmq notifications over native zmtp.
/// The socket frames each as [topic][body][sequence].
class BCS_API protocol_bitcoind_zmq
  : public server::protocol,
    public network::protocol_rpc<channel_bitcoind_zmq>,
    protected network::tracker<protocol_bitcoind_zmq>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_zmq> ptr;
    using channel_t = channel_bitcoind_zmq;
    using options_t = channel_t::options_t;
    using rpc_interface = interface::bitcoind_zmq;

    /// Sequence topic labels.
    enum class label : uint8_t
    {
        block_connected = 'C',
        block_disconnected = 'D',
        transaction_accepted = 'A',
        transaction_removed = 'R'
    };

    struct topic
    {
        static constexpr std::string_view hash_block{ "hashblock" };
        static constexpr std::string_view raw_block{ "rawblock" };
        static constexpr std::string_view hash_tx{ "hashtx" };
        static constexpr std::string_view raw_tx{ "rawtx" };
        static constexpr std::string_view sequence{ "sequence" };
    };

    inline protocol_bitcoind_zmq(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol(session, channel),
        network::protocol_rpc<channel_bitcoind_zmq>(session, channel, options),
        options_(options),
        network::tracker<protocol_bitcoind_zmq>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    /// Event handlers.
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;

    /// Handlers (the method is native, its first parameter a shared_ptr).
    bool handle_subscribe(const code& ec, const system::chunk_cptr& prefix,
        bool cancel) NOEXCEPT;

    /// Notifications (stranded).
    void do_organized(node::header_t link) NOEXCEPT;
    void do_reorganized(node::header_t link) NOEXCEPT;
    void do_transaction(node::transaction_t link) NOEXCEPT;

    /// Senders.
    void publish(node::transaction_t link, bool sequenced) NOEXCEPT;
    void publish(const std::string_view& topic, uint32_t sequence,
        system::data_chunk&& body) NOEXCEPT;

private:
    bool blocks() const NOEXCEPT;
    bool transactions() const NOEXCEPT;
    bool sequences() const NOEXCEPT;

    static void set_subscription(std::atomic_bool& subscribed,
        const std::string_view& topic, const std::string& prefix,
        bool cancel) NOEXCEPT;
    static system::data_chunk sequence_body(label value,
        system::hash_digest&& hash, uint64_t pool=zero) NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    std::atomic_bool subscribed_hash_block_{};
    std::atomic_bool subscribed_raw_block_{};
    std::atomic_bool subscribed_hash_tx_{};
    std::atomic_bool subscribed_raw_tx_{};
    std::atomic_bool subscribed_sequence_{};

    // These are protected by the strand.
    uint32_t subscriptions_{};
    uint32_t sequences_{};
    uint32_t hash_blocks_{};
    uint32_t raw_blocks_{};
    uint32_t hash_txs_{};
    uint32_t raw_txs_{};
    uint64_t pool_{};
};

} // namespace server
} // namespace libbitcoin

#endif
