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
constexpr auto relaxed = std::memory_order_relaxed;

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

    SUBSCRIBE_CHANNEL(void, handle_subscribe, _1, _2, _3);
    network::protocol_rpc<channel_bitcoind_zmq>::start();
}

// Events unsubscription is asynchronous, race is ok.
void protocol_bitcoind_zmq::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    unsubscribe_chase();
    network::protocol_rpc<channel_bitcoind_zmq>::stopping(ec);
}

// Handlers.
// ----------------------------------------------------------------------------

// A subscription is a topic prefix, so it applies to every topic that it
// matches, and the empty prefix applies to all.
bool protocol_bitcoind_zmq::handle_subscribe(const code& ec,
    const chunk_cptr& prefix, bool cancel) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (cancel)
    {
        if (!is_zero(subscriptions_))
            --subscriptions_;
    }
    else if (++subscriptions_ > options_.maximum_subscriptions)
    {
        // As there is no way to indicate failure the channel must be dropped.
        network::protocol::stop(server::error::subscription_limit);
        return false;
    }

    const std::string text{ prefix->begin(), prefix->end() };
    set_subscription(subscribed_hash_block_, topic::hash_block, text, cancel);
    set_subscription(subscribed_raw_block_, topic::raw_block, text, cancel);
    set_subscription(subscribed_hash_tx_, topic::hash_tx, text, cancel);
    set_subscription(subscribed_raw_tx_, topic::raw_tx, text, cancel);
    set_subscription(subscribed_sequence_, topic::sequence, text, cancel);

    // A subscription has no response, so the next read is armed once handled.
    read_next();
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
            if (blocks() || transactions())
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST(do_organized, std::get<node::header_t>(value));
            }

            break;
        }
        case node::chase::transaction:
        {
            if (transactions() || sequences())
            {
                BC_ASSERT(std::holds_alternative<node::transaction_t>(value));
                POST(do_transaction, std::get<node::transaction_t>(value));
            }

            break;
        }
        case node::chase::reorganized:
        {
            if (blocks() || transactions())
            {
                BC_ASSERT(std::holds_alternative<node::header_t>(value));
                POST(do_reorganized, std::get<node::header_t>(value));
            }

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
    auto hash = query.get_header_key(link);
    if (hash == null_hash)
    {
        LOGF("zmq::do_organized, block not found.");
        return;
    }

    if (transactions())
        for (const auto& tx: query.to_transactions(link))
            publish(tx, false);

    if (subscribed_hash_block_.load(relaxed))
        publish(topic::hash_block, hash_blocks_++,
            reverse(to_chunk(hash)));

    if (subscribed_raw_block_.load(relaxed))
        publish(topic::raw_block, raw_blocks_++,
            query.get_wire_block(link, true));

    if (subscribed_sequence_.load(relaxed))
        publish(topic::sequence, sequences_++,
            sequence_body(label::block_connected, std::move(hash)));
}

void protocol_bitcoind_zmq::do_reorganized(node::header_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    const auto& query = archive();
    auto hash = query.get_header_key(link);
    if (hash == null_hash)
    {
        LOGF("zmq::do_reorganized, block not found.");
        return;
    }

    if (transactions())
        for (const auto& tx: query.to_transactions(link))
            publish(tx, false);

    if (subscribed_sequence_.load(relaxed))
        publish(topic::sequence, sequences_++,
            sequence_body(label::block_disconnected, std::move(hash)));
}

void protocol_bitcoind_zmq::do_transaction(node::transaction_t link) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    publish(link, true);
}

void protocol_bitcoind_zmq::publish(node::transaction_t link,
    bool sequenced) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto& query = archive();
    auto hash = query.get_tx_key(link);
    if (hash == null_hash)
    {
        LOGF("zmq::publish, tx not found.");
        return;
    }

    if (subscribed_hash_tx_.load(relaxed))
        publish(topic::hash_tx, hash_txs_++, reverse(to_chunk(hash)));

    if (subscribed_raw_tx_.load(relaxed))
        publish(topic::raw_tx, raw_txs_++, query.get_wire_tx(link, true));

    if (sequenced && subscribed_sequence_.load(relaxed))
        publish(topic::sequence, sequences_++,
            sequence_body(label::transaction_accepted, std::move(hash),
                pool_++));
}

void protocol_bitcoind_zmq::publish(const std::string_view& topic,
    uint32_t sequence, data_chunk&& body) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto size = topic.size() + body.size() + sizeof(uint32_t);
    send_notification(rpc::string_t{ topic }, rpc::array_t
        {
            rpc::any_t{ to_shared(std::move(body)) },
            rpc::value_t{ sequence }
        }, size);
}

// Utilities (static).
// ----------------------------------------------------------------------------
// private

bool protocol_bitcoind_zmq::blocks() const NOEXCEPT
{
    return subscribed_hash_block_.load(relaxed) ||
        subscribed_raw_block_.load(relaxed) ||
        subscribed_sequence_.load(relaxed);
}

bool protocol_bitcoind_zmq::transactions() const NOEXCEPT
{
    return subscribed_hash_tx_.load(relaxed) ||
        subscribed_raw_tx_.load(relaxed);
}

bool protocol_bitcoind_zmq::sequences() const NOEXCEPT
{
    return subscribed_sequence_.load(relaxed);
}

// static
void protocol_bitcoind_zmq::set_subscription(std::atomic_bool& subscribed,
    const std::string_view& topic, const std::string& prefix,
    bool cancel) NOEXCEPT
{
    if (topic.starts_with(prefix))
        subscribed.store(!cancel, relaxed);
}

// static
data_chunk protocol_bitcoind_zmq::sequence_body(label value,
    hash_digest&& hash, uint64_t pool) NOEXCEPT
{
    const auto tx =
        value == label::transaction_accepted ||
        value == label::transaction_removed;
    const auto size = hash_size + one + (tx ? sizeof(uint64_t) : zero);

    data_chunk out(size);
    stream::out::fast sink{ out };
    write::bytes::fast writer{ sink };
    writer.write_bytes(reverse(hash));
    writer.write_byte(to_value(value));
    if (tx) writer.write_8_bytes_little_endian(pool);
    return out;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
