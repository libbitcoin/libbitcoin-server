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
#include <bitcoin/server/protocols/protocol_bitcoind_zmq.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_zmq

using namespace system;
using namespace network;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Start/stop.
// ----------------------------------------------------------------------------

void protocol_bitcoind_zmq::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Chaser subscription is asynchronous, events may be missed.
    subscribe_chase(BIND(handle_chase, _1, _2, _3));

    // One read stays armed: the proxy absorbs keepalive and delivers
    // subscriptions, each of which is recorded before the read is re-armed.
    channel_->read_frame(frame_, BIND(handle_frame, _1, _2));
    network::protocol::start();
}

// Events unsubscription is asynchronous, race is ok.
void protocol_bitcoind_zmq::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    unsubscribe_chase();
    network::protocol::stopping(ec);
}

// Codec (static).
// ----------------------------------------------------------------------------

bool protocol_bitcoind_zmq::subscription(bool& add, data_chunk& topic,
    const frame_t& frame) NOEXCEPT
{
    using stream = zmtp::stream;

    if (frame.command())
    {
        std::string name{};
        std::span<const uint8_t> content{};
        const std::span<const uint8_t> body{ frame.body };
        if (!stream::command_name(name, content, body))
            return false;

        if (name == "SUBSCRIBE")
            add = true;
        else if (name == "CANCEL")
            add = false;
        else
            return false;

        topic.assign(content.begin(), content.end());
        return true;
    }

    // The 3.0 dialect: a single frame message prefixed 0x01 (subscribe) or
    // 0x00 (cancel), accepted from any peer (as libzmq).
    if (frame.more() || frame.body.empty() || frame.body.front() > 0x01)
        return false;

    add = (frame.body.front() == 0x01);
    topic.assign(std::next(frame.body.begin()), frame.body.end());
    return true;
}

bool protocol_bitcoind_zmq::subscribed(const data_stack& subscriptions,
    std::string_view topic) NOEXCEPT
{
    return std::any_of(subscriptions.begin(), subscriptions.end(),
        [&](const data_chunk& subscription) NOEXCEPT
        {
            return subscription.size() <= topic.size() &&
                std::equal(subscription.begin(), subscription.end(),
                    topic.begin());
        });
}

data_chunk protocol_bitcoind_zmq::notification(std::string_view topic,
    const data_chunk& body, uint32_t sequence) NOEXCEPT
{
    const data_stack parts
    {
        data_chunk{ topic.begin(), topic.end() },
        body,
        to_chunk(to_little_endian(sequence))
    };

    return zmtp::stream::frame_message(parts);
}

data_chunk protocol_bitcoind_zmq::sequence_body(const hash_digest& hash,
    uint8_t label) NOEXCEPT
{
    // Hashes are published in rpc (reversed) byte order.
    auto body = to_chunk(reverse_copy(hash));
    body.push_back(label);
    return body;
}

// private static
size_t protocol_bitcoind_zmq::index(std::string_view topic) NOEXCEPT
{
    const auto it = std::find(topics::names.begin(), topics::names.end(),
        topic);

    if (it == topics::names.end())
        return zero;

    return possible_narrow_sign_cast<size_t>(
        std::distance(topics::names.begin(), it));
}

// Subscriptions.
// ----------------------------------------------------------------------------

void protocol_bitcoind_zmq::handle_frame(const code& ec,
    size_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (ec)
    {
        stop(ec);
        return;
    }

    bool add{};
    data_chunk topic{};
    if (subscription(add, topic, frame_))
    {
        const auto it = std::find(subscriptions_.begin(),
            subscriptions_.end(), topic);

        // Prefix subscription set; duplicates collapse to a single entry.
        if (add && it == subscriptions_.end())
        {
            // A subscription beyond the configured limit is not recorded.
            if (subscriptions_.size() < options_.maximum_subscriptions)
                subscriptions_.push_back(std::move(topic));
        }
        else if (!add && it != subscriptions_.end())
        {
            subscriptions_.erase(it);
        }
    }

    // A publisher expects nothing else, other frames are ignored.
    channel_->read_frame(frame_, BIND(handle_frame, _1, _2));
}

// Notifications.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_zmq::handle_chase(const code&,
    node::chase event_, node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case node::chase::organized:
        {
            BC_ASSERT(std::holds_alternative<node::header_t>(value));
            POST(do_organized, std::get<node::header_t>(value));
            break;
        }
        case node::chase::transaction:
        {
            // There is no transaction pool, and the event does not identify
            // a transaction, so mempool notifications (hashtx/rawtx and the
            // sequence 'A'/'R' labels) are not issued.
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

void protocol_bitcoind_zmq::do_organized(node::header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    const auto& query = archive();
    const auto block = query.get_block(link, true);
    if (!block)
    {
        LOGF("Zmq::do_organized, block not found (" << link << ").");
        return;
    }

    const auto hash = block->hash();
    publish(topics::hash_block, to_chunk(reverse_copy(hash)));
    publish(topics::raw_block, block->to_data(true));
    publish(topics::sequence, sequence_body(hash, topics::block_connected));

    // Every transaction of a connected block is published (as bitcoind).
    for (const auto& tx: *block->transactions_ptr())
    {
        publish(topics::hash_tx, to_chunk(reverse_copy(tx->hash(false))));
        publish(topics::raw_tx, tx->to_data(true));
    }
}

void protocol_bitcoind_zmq::publish(std::string_view topic,
    data_chunk&& body) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!subscribed(subscriptions_, topic))
        return;

    // The per-topic sequence is incremented after each publication.
    auto& sequence = sequences_.at(index(topic));
    const auto packet = to_shared(notification(topic, body, sequence++));
    channel_->write_packet(packet, BIND(handle_publish, _1, _2));
}

void protocol_bitcoind_zmq::handle_publish(const code& ec,
    size_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (ec)
        stop(ec);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
