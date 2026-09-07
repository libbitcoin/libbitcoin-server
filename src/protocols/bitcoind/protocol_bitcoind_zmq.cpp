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

    SUBSCRIBE_RPC(handle_subscribe, _1, _2, _3);
    protocol_rpc<channel_bitcoind_zmq>::start();
}

// Events unsubscription is asynchronous, race is ok.
void protocol_bitcoind_zmq::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    unsubscribe_chase();
    protocol_rpc<channel_bitcoind_zmq>::stopping(ec);
}

// Utilities (static).
// ----------------------------------------------------------------------------

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

data_chunk protocol_bitcoind_zmq::sequence_body(const hash_digest& hash,
    label label) NOEXCEPT
{
    // Hashes are published in rpc (reversed) byte order.
    auto body = to_chunk(reverse_copy(hash));
    body.push_back(to_value(label));
    return body;
}

// private static
size_t protocol_bitcoind_zmq::index(std::string_view topic) NOEXCEPT
{
    const auto it = std::find(topics.begin(), topics.end(), topic);
    if (it == topics.end())
        return zero;

    return possible_narrow_sign_cast<size_t>(std::distance(topics.begin(), it));
}

// Handlers.
// ----------------------------------------------------------------------------

// A subscription is a topic prefix (as libzmq), so it is matched against each
// topic at publication. A subscription beyond the configured limit is not
// recorded (there is no response by which to refuse it).
bool protocol_bitcoind_zmq::handle_subscribe(const code& ec,
    const chunk_cptr& prefix, bool stop) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto it = std::find(subscriptions_.begin(), subscriptions_.end(),
        *prefix);

    // Prefix subscription set; duplicates collapse to a single entry.
    if (!stop && it == subscriptions_.end())
    {
        if (subscriptions_.size() < options_.maximum_subscriptions)
            subscriptions_.push_back(*prefix);
    }
    else if (stop && it != subscriptions_.end())
    {
        subscriptions_.erase(it);
    }

    // A subscription has no response, so the read is resumed once handled.
    // Qualified, as node::protocol::resume resumes the suspended network.
    network::protocol::resume();
    return true;
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
    publish(hash_block, to_chunk(reverse_copy(hash)));
    publish(raw_block, block->to_data(true));
    publish(sequence, sequence_body(hash, label::block_connected));

    // Every transaction of a connected block is published (as bitcoind).
    for (const auto& tx: *block->transactions_ptr())
    {
        publish(hash_tx, to_chunk(reverse_copy(tx->hash(false))));
        publish(raw_tx, tx->to_data(true));
    }
}

// The socket frames the notification as [topic][body][sequence] (the params
// are one part each, the sequence as 32-bit little-endian).
void protocol_bitcoind_zmq::publish(std::string_view topic,
    data_chunk&& body) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!subscribed(subscriptions_, topic))
        return;

    // The per-topic sequence is incremented after each publication.
    auto& sequence = sequences_.at(index(topic));
    const auto size = topic.size() + body.size() + sizeof(uint32_t);
    send_notification(std::string{ topic }, rpc::array_t
    {
        rpc::any_t{ to_shared(std::move(body)) },
        rpc::value_t{ sequence++ }
    }, size);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
