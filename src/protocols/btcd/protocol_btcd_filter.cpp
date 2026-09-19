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

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

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
    if (btcd::filter_keys(keys, addresses, p2kh_, p2sh_, witness_))
    {
        send_error(error::btcd::invalid_parameter);
        return true;
    }

    chain::points points{};
    if (btcd::filter_points(points, outpoints))
    {
        send_error(error::btcd::invalid_parameter);
        return true;
    }

    if (!keys.empty() && !archive().address_enabled())
    {
        send_error(error::btcd::unimplemented);
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
    const auto maximum = btcd_options().maximum_filters;
    const auto limit = btcd_options().maximum_history;

    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        if (watch_count() >= maximum)
        {
            ec = error::btcd::misc_error;
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

        if (watch_count() >= maximum)
        {
            ec = error::btcd::misc_error;
            break;
        }

        // Prime the spender set to present, so matching reports new only.
        const auto at = outpoint_watches_.try_emplace(prevout, outpoint_watch{});
        if (at.second)
            at.first->second.spenders = query.get_spenders_history(prevout);
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
        using namespace error::btcd;
        send_error(translate(ec, internal_error));
        return;
    }

    send_result({});
}

// Handlers (notifyreceived/notifyspent).
// ----------------------------------------------------------------------------
// Confirmed-block matching only (no tx pool in v4), reusing loadtxfilter's
// cursor-based history matching.

bool protocol_btcd::handle_notify_received(const code& ec,
    btcd_interface::notify_received, const value_t& addresses) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hashes keys{};
    if (btcd::filter_keys(keys, addresses, p2kh_, p2sh_, witness_))
    {
        send_error(error::btcd::invalid_parameter);
        return true;
    }

    if (!keys.empty() && !archive().address_enabled())
    {
        send_error(error::btcd::unimplemented);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_notify_received, std::move(keys));
    return true;
}

void protocol_btcd::do_notify_received(const hashes& keys) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    histories discard{};
    code ec{ error::success };
    const auto& query = archive();
    const auto limit = btcd_options().maximum_history;

    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        if (watch_count() >= btcd_options().maximum_filters)
        {
            ec = error::btcd::misc_error;
            break;
        }

        // Prime the cursor to present, so matching reports new blocks only.
        const auto at = receive_watches_.try_emplace(key, address_watch{});
        if (at.second)
        {
            watching_legacy_.store(true, relaxed);
            const auto fault = query.get_history(stopping_,
                at.first->second.cursor, discard, key, limit, turbo_);
            if (fault == database::error::query_canceled)
                return;
        }
    }

    POST_BTCD(complete_notify_received, ec);
}

void protocol_btcd::complete_notify_received(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        using namespace error::btcd;
        send_error(translate(ec, internal_error));
        return;
    }

    send_result({});
}

bool protocol_btcd::handle_stop_notify_received(const code& ec,
    btcd_interface::stop_notify_received, const value_t& addresses) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hashes keys{};
    if (btcd::filter_keys(keys, addresses, p2kh_, p2sh_, witness_))
    {
        send_error(error::btcd::invalid_parameter);
        return true;
    }

    POST_NOTIFY(do_stop_notify_received, std::move(keys));
    send_result({});
    return true;
}

void protocol_btcd::do_stop_notify_received(const hashes& keys) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (const auto& key: keys)
        receive_watches_.erase(key);

    if (receive_watches_.empty() && spent_watches_.empty())
        watching_legacy_.store(false, relaxed);
}

bool protocol_btcd::handle_notify_spent(const code& ec,
    btcd_interface::notify_spent, const value_t& outpoints) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::points points{};
    if (btcd::filter_points(points, outpoints))
    {
        send_error(error::btcd::invalid_parameter);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_notify_spent, std::move(points));
    return true;
}

void protocol_btcd::do_notify_spent(const chain::points& points) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    code ec{ error::success };

    for (const auto& prevout: points)
    {
        if (stopping_)
            return;

        if (watch_count() >= btcd_options().maximum_filters)
        {
            ec = error::btcd::misc_error;
            break;
        }

        if (spent_watches_.try_emplace(prevout, outpoint_watch{}).second)
            watching_legacy_.store(true, relaxed);
    }

    POST_BTCD(complete_notify_spent, ec);
}

void protocol_btcd::complete_notify_spent(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        using namespace error::btcd;
        send_error(translate(ec, internal_error));
        return;
    }

    send_result({});
}

bool protocol_btcd::handle_stop_notify_spent(const code& ec,
    btcd_interface::stop_notify_spent, const value_t& outpoints) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::points points{};
    if (btcd::filter_points(points, outpoints))
    {
        send_error(error::btcd::invalid_parameter);
        return true;
    }

    POST_NOTIFY(do_stop_notify_spent, std::move(points));
    send_result({});
    return true;
}

void protocol_btcd::do_stop_notify_spent(const chain::points& points) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    for (const auto& prevout: points)
        spent_watches_.erase(prevout);

    if (receive_watches_.empty() && spent_watches_.empty())
        watching_legacy_.store(false, relaxed);
}

bool protocol_btcd::handle_rescan_blocks(const code& ec,
    btcd_interface::rescan_blocks, const value_t& blockhashes) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!std::holds_alternative<array_t>(blockhashes.value()))
    {
        send_error(error::btcd::invalid_params);
        return true;
    }

    hash_digest hash{};
    hashes block_hashes{};
    for (const auto& item: std::get<array_t>(blockhashes.value()))
    {
        // btcd wraps the hash parse failure as an internal error.
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            send_error(error::btcd::internal_error);
            return true;
        }

        block_hashes.push_back(hash);
    }

    if (!archive().address_enabled())
    {
        send_error(error::btcd::unimplemented);
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
            POST_BTCD(complete_rescan_blocks, error::btcd::invalid_address_or_key,
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
        if (at != matched.cend() && !at->second.empty())
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
        using namespace error::btcd;
        send_error(translate(ec, internal_error));
        return;
    }

    send_result(std::move(*discovered));
}

// Handlers (transactions).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_search_raw_transactions(const code& ec,
    btcd_interface::search_raw_transactions, const std::string& address,
    double verbose, double skip, double count, double prevouts, bool reverse,
    const array_t& filteraddrs) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!archive().address_enabled())
    {
        send_error(error::btcd::misc_error);
        return true;
    }

    hashes keys{};
    if (btcd::search_keys(keys, address, p2kh_, p2sh_, witness_))
    {
        send_error(error::btcd::invalid_address_or_key);
        return true;
    }

    int64_t level{}, first{}, extra{}, requested{};
    if (!to_integer(level, verbose)   || !to_integer(first, skip) ||
        !to_integer(requested, count) || !to_integer(extra, prevouts))
    {
        send_error(error::btcd::invalid_params);
        return true;
    }

    if (is_zero(requested))
    {
        send_result(null_t{});
        return true;
    }

    std::set<std::string> filter{};
    for (const auto& item: filteraddrs)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::btcd::invalid_params);
            return true;
        }

        filter.emplace(std::get<string_t>(item.value()));
    }

    monitor(true);
    PARALLEL(do_search_raw_transactions, std::move(keys), !is_zero(level),
        limit<size_t>(first, zero, max_size_t),
        limit<size_t>(requested, one, max_size_t), !is_zero(extra), reverse,
        std::move(filter));
    return true;
}

void protocol_btcd::do_search_raw_transactions(const hashes& keys,
    bool verbose, size_t skip, size_t count, bool prevouts, bool reverse,
    const std::set<std::string>& filter) NOEXCEPT
{
    BC_ASSERT(!stranded());

    histories history{};
    auto& query = archive();
    const auto limit = btcd_options().maximum_history;
    for (const auto& key: keys)
    {
        histories part{};
        database::height_link cursor{};
        if (const auto fault = query.get_confirmed_history(stopping_, cursor,
            part, key, limit, turbo_))
        {
            if (fault == database::error::query_canceled)
                return;

            POST_BTCD(complete_search_raw_transactions, fault,
                to_shared<array_t>());
            return;
        }

        history.insert(history.end(), part.cbegin(), part.cend());
    }

    // A transaction can pay more than one of the searched scripts.
    if (keys.size() > one)
        database::history::filter_sort_and_dedup(history);

    // Confirmed history ascends by height, btcd's default order.
    if (reverse)
        std::reverse(history.begin(), history.end());

    array_t found{};
    for (const auto& entry: history)
    {
        if (stopping_)
            return;

        if (to_bool(skip))
        {
            --skip;
            continue;
        }

        if (found.size() == count)
            break;

        constexpr auto witness = true;
        const auto link = query.to_tx(entry.tx.hash());
        const auto tx = query.get_transaction(link, witness);
        if (!tx)
        {
            POST_BTCD(complete_search_raw_transactions,
                error::btcd::internal_error, to_shared<array_t>());
            return;
        }

        if (!verbose)
        {
            const auto size = tx->serialized_size(witness);
            found.emplace_back(to_text(*tx, size, witness));
            continue;
        }

        found.emplace_back(boost::json::value{ btcd::search_transaction(
            query, link, *tx, filter, prevouts, p2kh_, p2sh_, witness_,
            flags_) });
    }

    POST_BTCD(complete_search_raw_transactions, error::success,
        emplace_shared<array_t>(std::move(found)));
}

void protocol_btcd::complete_search_raw_transactions(const code& ec,
    const array_ptr& found) NOEXCEPT
{
    BC_ASSERT(stranded());

    monitor(false);
    if (stopped())
        return;

    if (ec)
    {
        using namespace error::btcd;
        send_error(translate(ec, internal_error));
        return;
    }

    // btcd reports an address without transactions as -5 (no tx info).
    if (found->empty())
    {
        send_error(error::btcd::invalid_address_or_key);
        return;
    }

    send_result(std::move(*found));
}

// Notification event handlers.
// ----------------------------------------------------------------------------

// TODO: the matchers query the store once per watch, so cost is O(watches)
// TODO: per block, per channel, against a maximum_filters ceiling. Invert to
// TODO: a scan of the block's scripts and points against the watch maps, for
// TODO: O(block) independent of the watch count.
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

    // Cursors advance in the matchers, so this stays on the notification
    // strand, and receives are matched first, as each arms a spent-watch.
    const sizes heights{ height };
    array_t txs{};
    if (match_filters(txs, height, heights))
        return;

    std::vector<array_t> receive_notifications{};
    if (const auto fault = match_receives(receive_notifications, header, height,
        heights))
    {
        if (fault != database::error::query_canceled)
            POST_BTCD(complete_overflow, fault);

        return;
    }

    std::vector<array_t> spent_notifications{};
    if (match_spends(spent_notifications, header, height, heights))
        return;

    POST_BTCD(notify_connected, header, height,
        emplace_shared<array_t>(std::move(txs)),
        emplace_shared<std::vector<array_t>>(std::move(receive_notifications)),
        emplace_shared<std::vector<array_t>>(std::move(spent_notifications)));
}

// Filter (loadtxfilter) block-level notification.
code protocol_btcd::match_filters(array_t& out, size_t height,
    const sizes& heights) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    matches matched{};
    for (auto& [key, sub]: address_watches_)
    {
        if (stopping_)
            return database::error::query_canceled;

        const auto fault = match_addresses(matched, sub, key, heights);
        if (fault == database::error::query_canceled)
            return fault;
    }

    for (auto& [prevout, sub]: outpoint_watches_)
    {
        if (stopping_)
            return database::error::query_canceled;

        match_outpoints(matched, sub, prevout, heights);
    }

    const auto at = matched.find(height);
    if (at != matched.cend())
        out = serialize_matches(at->second);

    return error::success;
}

// Legacy (notifyreceived) individual notifications; auto-arms spent-watches.
code protocol_btcd::match_receives(std::vector<array_t>& out,
    const header_cptr& header, size_t height, const sizes& heights) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    matches received{};
    for (auto& [key, sub]: receive_watches_)
    {
        if (stopping_)
            return database::error::query_canceled;

        const auto fault = match_addresses(received, sub, key, heights);
        if (fault == database::error::query_canceled)
            return fault;
    }

    const auto& query = archive();
    const auto at = received.find(height);
    if (at == received.cend())
        return error::success;

    for (const auto& [position, hash]: at->second)
        if (const auto tx = query.get_transaction(query.to_tx(hash), true); tx)
        {
            // The address walk matches spends, which are not receives.
            bool paid{};
            if (const auto fault = arm_spent_watches(paid, *tx, hash))
                return fault;

            if (paid)
                out.push_back(serialize_legacy(*tx, header, height, position));
        }

    return error::success;
}

// Legacy (notifyspent, including auto-armed) one-shot notifications.
code protocol_btcd::match_spends(std::vector<array_t>& out,
    const header_cptr& header, size_t height, const sizes& heights) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    const auto& query = archive();
    for (auto it = spent_watches_.begin(); it != spent_watches_.end();)
    {
        if (stopping_)
            return database::error::query_canceled;

        matches spent{};
        match_outpoints(spent, it->second, it->first, heights);
        const auto at = spent.find(height);
        if (at == spent.cend() || at->second.empty())
        {
            ++it;
            continue;
        }

        for (const auto& [position, hash]: at->second)
            if (const auto tx = query.get_transaction(query.to_tx(hash), true); tx)
                out.push_back(serialize_legacy(*tx, header, height, position));

        it = spent_watches_.erase(it);
    }

    return error::success;
}

void protocol_btcd::do_disconnected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    // Reset watch cursors, disconnected blocks invalidate the walks.
    for (auto& [key, sub]: address_watches_)
        sub.cursor = {};

    for (auto& [key, sub]: receive_watches_)
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
    size_t height, const array_ptr& txs, const legacy_ptr& received,
    const legacy_ptr& redeemed) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    // recvtx/redeemingtx (below), unlike blockconnected, don't need notifyblocks.
    if (subscribed_blocks_.load(relaxed))
    {
        // Elements are moved, as a braced initializer list always copies.
        array_t connected{};
        connected.emplace_back(encode_hash(header->get_hash()));
        connected.emplace_back(height);
        connected.emplace_back(header->timestamp());
        send_notification("blockconnected", std::move(connected));

        array_t filtered{};
        filtered.emplace_back(height);
        filtered.emplace_back(to_text(*header, chain::header::serialized_size()));
        filtered.emplace_back(std::move(*txs));
        send_notification("filteredblockconnected", std::move(filtered));
    }

    for (auto& params: *received)
        send_notification("recvtx", std::move(params));

    for (auto& params: *redeemed)
        send_notification("redeemingtx", std::move(params));
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
    send_notification("blockdisconnected", std::move(disconnected));

    array_t filtered{};
    filtered.emplace_back(height);
    filtered.emplace_back(to_text(*header, chain::header::serialized_size()));
    send_notification("filteredblockdisconnected", std::move(filtered));
}

// Utilities.
// ----------------------------------------------------------------------------

// Called from notification strand (live) and parallel (rescan).
code protocol_btcd::match_addresses(matches& out, address_watch& sub,
    const hash_digest& key, const sizes& heights) NOEXCEPT
{
    histories delta{};
    const auto& query = archive();
    const auto limit = btcd_options().maximum_history;
    if (const auto ec = query.get_history(stopping_, sub.cursor, delta, key,
        limit, turbo_); ec)
        return ec;

    for (const auto& entry: delta)
        if (entry.confirmed() && heights.contains(entry.tx.height()))
            out[entry.tx.height()].emplace(entry.position, entry.tx.hash());

    return error::success;
}

// Called from notification strand (live) and parallel (rescan).
// TODO: uncursored, unlike match_addresses, so the full spender set is read
// TODO: and sorted on each block.
void protocol_btcd::match_outpoints(matches& out, outpoint_watch& sub,
    const point& prevout, const sizes& heights) NOEXCEPT
{
    outpoint_watch next{};
    next.spenders = archive().get_spenders_history(prevout);

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

// Mirrors real btcd's auto-registration of a spent-watch on a match.
code protocol_btcd::arm_spent_watches(bool& paid, const chain::transaction& tx,
    const hash_digest& hash) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    paid = false;
    const auto maximum = btcd_options().maximum_filters;
    uint32_t index{};
    for (const auto& out: *tx.outputs_ptr())
    {
        if (receive_watches_.contains(out->script().hash()))
        {
            paid = true;

            if (watch_count() >= maximum)
                return error::btcd::misc_error;

            spent_watches_.try_emplace(point{ hash, index }, outpoint_watch{});
        }

        ++index;
    }

    return error::success;
}

void protocol_btcd::complete_overflow(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stop(ec);
}

// [txHex, blockDetails] wire shape of a real btcd recvtx/redeemingtx.
array_t protocol_btcd::serialize_legacy(const chain::transaction& tx,
    const header_cptr& header, size_t height, size_t position) NOEXCEPT
{
    constexpr auto witness = true;
    return array_t
    {
        to_text(tx, tx.serialized_size(witness), witness),
        object_t
        {
            { "height", height },
            { "hash", encode_hash(header->get_hash()) },
            { "index", position },
            { "time", header->timestamp() }
        }
    };
}

// Combined DoS budget across all watch-list maps.
size_t protocol_btcd::watch_count() const NOEXCEPT
{
    return ceilinged_add(ceilinged_add(ceilinged_add(
        address_watches_.size(), outpoint_watches_.size()),
        receive_watches_.size()), spent_watches_.size());
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
