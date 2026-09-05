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
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

/// bitcoind zmq notifications over native zmtp. Subscriptions are recorded
/// from the subscriber (3.1 commands and the 3.0 message form), bounded by
/// configuration, and notifications are published for chaser events to
/// matching subscriptions with per-topic sequences (bitcoind semantics).
class BCS_API protocol_bitcoind_zmq
  : public server::protocol,
    public network::protocol,
    protected network::tracker<protocol_bitcoind_zmq>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_zmq> ptr;
    using channel_t = channel_bitcoind_zmq;
    using options_t = channel_t::options_t;
    using frame_t = channel_t::frame_t;
    using topics = interface::bitcoind_zmq;

    inline protocol_bitcoind_zmq(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol(session, channel),
        network::protocol(session, channel),
        options_(options),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        network::tracker<protocol_bitcoind_zmq>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

    /// Codec (static, exposed for test).
    /// -----------------------------------------------------------------------

    /// Parse a delivered frame as a subscription: a 3.1 SUBSCRIBE/CANCEL
    /// command or the 3.0 message form (0x01/0x00 prefix). False otherwise.
    static bool subscription(bool& add, system::data_chunk& topic,
        const frame_t& frame) NOEXCEPT;

    /// The topic matches a subscription prefix (empty matches every topic).
    static bool subscribed(const system::data_stack& subscriptions,
        std::string_view topic) NOEXCEPT;

    /// Frame a notification: [topic][body][32-bit little-endian sequence].
    static system::data_chunk notification(std::string_view topic,
        const system::data_chunk& body, uint32_t sequence) NOEXCEPT;

    /// The sequence topic body: reversed hash then label.
    static system::data_chunk sequence_body(const system::hash_digest& hash,
        uint8_t label) NOEXCEPT;

protected:
    /// Chaser events (not stranded).
    bool handle_chase(const code& ec, node::chase event_,
        node::event_value value) NOEXCEPT;

    /// Subscriptions (stranded).
    void handle_frame(const code& ec, size_t size) NOEXCEPT;

    /// Notifications (stranded).
    void do_organized(node::header_t link) NOEXCEPT;
    void publish(std::string_view topic, system::data_chunk&& body) NOEXCEPT;
    void handle_publish(const code& ec, size_t size) NOEXCEPT;

private:
    static size_t index(std::string_view topic) NOEXCEPT;

    // These are thread safe.
    const options_t& options_;
    const channel_t::ptr channel_;

    // These are protected by the channel strand.
    frame_t frame_{};
    system::data_stack subscriptions_{};
    std::array<uint32_t, topics::names.size()> sequences_{};
};

} // namespace server
} // namespace libbitcoin

#endif
