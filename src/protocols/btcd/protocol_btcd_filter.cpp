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
#include <bitcoin/server/protocols/protocol_btcd.hpp>

#include <memory>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd

// protocol_bitcoind declares 'using post = network::http::method::post',
// which shadows network::protocol::post<Derived>. Qualify explicitly.
#define POST_BTCD(method, ...) \
    this->network::protocol::template post<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
constexpr auto relaxed = std::memory_order_relaxed;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Handlers (filters).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_load_tx_filter(const code& ec,
    btcd_interface::load_tx_filter, bool reload, const value_t& addresses,
    const value_t& outpoints) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hashes keys{};
    if (const auto fault = btcd::filter_keys(keys, addresses, p2kh_, p2sh_,
        witness_))
    {
        send_error(fault);
        return true;
    }

    chain::points points{};
    if (const auto fault = btcd::filter_points(points, outpoints))
    {
        send_error(fault);
        return true;
    }

    if (!keys.empty() && !archive().address_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_load_tx_filter, reload, std::move(keys), std::move(points));
    return true;
}

void protocol_btcd::do_load_tx_filter(bool reload, const hashes& keys,
    const chain::points& points) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    if (reload)
    {
        address_watches_.clear();
        outpoint_watches_.clear();
    }

    histories discard{};
    code ec{ error::success };
    const auto& query = archive();
    const auto maximum = server_settings().btcd.maximum_filters;
    const auto limit = server_settings().btcd.maximum_history;

    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        if (ceilinged_add(address_watches_.size(), outpoint_watches_.size()) >=
            maximum)
        {
            ec = error::subscription_limit;
            break;
        }

        // Prime the cursor to present, so matching reports new blocks only.
        const auto at = address_watches_.try_emplace(key, address_watch{});
        if (at.second)
        {
            const auto fault = query.get_history(stopping_,
                at.first->second.cursor, discard, key, limit, turbo_);
            if (fault == database::error::query_canceled)
                return;
        }
    }

    for (const auto& prevout: points)
    {
        if (stopping_)
            return;

        if (ec)
            break;

        if (ceilinged_add(address_watches_.size(), outpoint_watches_.size()) >=
            maximum)
        {
            ec = error::subscription_limit;
            break;
        }

        // Prime the spender set to present, so matching reports new only.
        const auto at = outpoint_watches_.try_emplace(prevout, outpoint_watch{});
        if (at.second)
        {
            auto& sub = at.first->second;
            sub.outpoint = query.get_tx_history(query.to_tx(prevout.hash()));
            sub.spenders = query.get_spenders_history(prevout);
        }
    }

    POST_BTCD(complete_load_tx_filter, ec);
}

void protocol_btcd::complete_load_tx_filter(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        send_error(ec);
        return;
    }

    send_result({}, 4);
}

bool protocol_btcd::handle_rescan_blocks(const code& ec,
    btcd_interface::rescan_blocks, const value_t& blockhashes) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!std::holds_alternative<array_t>(blockhashes.value()))
    {
        send_error(error::invalid_argument);
        return true;
    }

    hash_digest hash{};
    hashes block_hashes{};
    for (const auto& item: std::get<array_t>(blockhashes.value()))
    {
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            send_error(error::invalid_argument);
            return true;
        }

        block_hashes.push_back(hash);
    }

    if (!archive().address_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_rescan_blocks, emplace_shared<hashes>(
        std::move(block_hashes)));
    return true;
}

// Snapshot the watch-list, so the query runs parallel (not on the strand).
void protocol_btcd::do_rescan_blocks(const hashes_ptr& block_hashes) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    hashes keys{};
    keys.reserve(address_watches_.size());
    for (const auto& watch: address_watches_)
        keys.push_back(watch.first);

    chain::points points{};
    points.reserve(outpoint_watches_.size());
    for (const auto& watch: outpoint_watches_)
        points.push_back(watch.first);

    PARALLEL(do_rescan_watches, block_hashes, std::move(keys),
        std::move(points));
}

void protocol_btcd::do_rescan_watches(const hashes_ptr& block_hashes,
    const hashes& keys, chain::points& points) NOEXCEPT
{
    BC_ASSERT(!stranded());

    sizes heights{};
    const auto& query = archive();
    std::vector<std::pair<hash_digest, size_t>> named{};

    // Resolve named blocks to heights (result retains request order).
    for (auto& hash: *block_hashes)
    {
        if (stopping_)
            return;

        size_t height{};
        if (!query.get_height(height, query.to_header(hash)))
        {
            POST_BTCD(complete_rescan_blocks, error::not_found,
                to_shared<array_t>());
            return;
        }

        heights.insert(height);
        named.emplace_back(std::move(hash), height);
    }

    // Match the snapshot against the named heights (full replay).
    matches matched{};
    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        address_watch replay{};
        const auto fault = match_addresses(matched, replay, key, heights);
        if (fault == database::error::query_canceled)
            return;
    }

    for (const auto& prevout: points)
    {
        if (stopping_)
            return;

        outpoint_watch replay{};
        match_outpoints(matched, replay, prevout, heights);
    }

    array_t discovered{};
    for (const auto& [hash, height]: named)
    {
        if (stopping_)
            return;

        const auto at = matched.find(height);
        if (at != matched.end() && !at->second.empty())
        {
            discovered.emplace_back(object_t
            {
                { "hash", encode_hash(hash) },
                { "transactions", serialize_matches(at->second) }
            });
        }
    }

    POST_BTCD(complete_rescan_blocks, error::success,
        emplace_shared<array_t>(std::move(discovered)));
}

void protocol_btcd::complete_rescan_blocks(const code& ec,
    const array_ptr& discovered) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        send_error(ec);
        return;
    }

    send_result(std::move(*discovered), 256);
}

// Notification event handlers.
// ----------------------------------------------------------------------------

void protocol_btcd::do_connected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    const auto& query = archive();
    const database::header_link link{ link_value };

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    // Match the watch-list against the connected block (cursored delta).
    // Cursors advance here, so this stays on the notification strand.
    matches matched{};
    const sizes heights{ height };
    for (auto& [key, sub]: address_watches_)
    {
        if (stopping_)
            return;

        const auto fault = match_addresses(matched, sub, key, heights);
        if (fault == database::error::query_canceled)
            return;
    }

    for (auto& [prevout, sub]: outpoint_watches_)
    {
        if (stopping_)
            return;

        match_outpoints(matched, sub, prevout, heights);
    }

    array_t txs{};
    const auto at = matched.find(height);
    if (at != matched.end())
        txs = serialize_matches(at->second);

    POST_BTCD(notify_connected, header, height,
        emplace_shared<array_t>(std::move(txs)));
}

void protocol_btcd::do_disconnected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    // Reset watch cursors, disconnected blocks invalidate the walks.
    for (auto& [key, sub]: address_watches_)
        sub.cursor = {};

    const database::header_link link{ link_value };
    const auto& query = archive();

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    POST_BTCD(notify_disconnected, header, height);
}

void protocol_btcd::notify_connected(const header_cptr& header,
    size_t height, const array_ptr& txs) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(relaxed))
        return;

    // Elements are moved, as a braced initializer list always copies.
    array_t connected{};
    connected.emplace_back(encode_hash(header->get_hash()));
    connected.emplace_back(height);
    connected.emplace_back(header->timestamp());
    send_notification("blockconnected", std::move(connected), 256);

    array_t filtered{};
    filtered.emplace_back(height);
    filtered.emplace_back(to_text(*header, chain::header::serialized_size()));
    filtered.emplace_back(std::move(*txs));
    send_notification("filteredblockconnected", std::move(filtered), 256);
}

void protocol_btcd::notify_disconnected(const header_cptr& header,
    size_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(relaxed))
        return;

    array_t disconnected{};
    disconnected.emplace_back(encode_hash(header->get_hash()));
    disconnected.emplace_back(height);
    disconnected.emplace_back(header->timestamp());
    send_notification("blockdisconnected", std::move(disconnected), 256);

    array_t filtered{};
    filtered.emplace_back(height);
    filtered.emplace_back(to_text(*header, chain::header::serialized_size()));
    send_notification("filteredblockdisconnected", std::move(filtered), 256);
}

// Utilities.
// ----------------------------------------------------------------------------

// Called from notification strand (live) and parallel (rescan).
code protocol_btcd::match_addresses(matches& out, address_watch& sub,
    const hash_digest& key, const sizes& heights) NOEXCEPT
{
    histories delta{};
    const auto& query = archive();
    const auto limit = server_settings().btcd.maximum_history;
    if (const auto ec = query.get_history(stopping_, sub.cursor, delta, key,
        limit, turbo_); ec)
        return ec;

    for (const auto& entry: delta)
        if (entry.confirmed() && heights.contains(entry.tx.height()))
            out[entry.tx.height()].emplace(entry.position, entry.tx.hash());

    return error::success;
}

// Called from notification strand (live) and parallel (rescan).
void protocol_btcd::match_outpoints(matches& out, outpoint_watch& sub,
    const point& prevout, const sizes& heights) NOEXCEPT
{
    outpoint_watch next{};
    const auto& query = archive();
    next.outpoint = query.get_tx_history(query.to_tx(prevout.hash()));
    next.spenders = query.get_spenders_history(prevout);

    for (const auto& spender: difference(next.spenders, sub.spenders))
        if (spender.confirmed() && heights.contains(spender.tx.height()))
            out[spender.tx.height()].emplace(spender.position,
                spender.tx.hash());

    sub = std::move(next);
}

array_t protocol_btcd::serialize_matches(const matched_txs& txs) NOEXCEPT
{
    array_t out{};
    constexpr auto witness = true;
    const auto& query = archive();
    for (const auto& [position, hash]: txs)
    {
        if (stopping_)
            return out;

        if (const auto tx = query.get_transaction(query.to_tx(hash), witness); tx)
            out.emplace_back(to_text(*tx, tx->serialized_size(witness), witness));
    }

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
