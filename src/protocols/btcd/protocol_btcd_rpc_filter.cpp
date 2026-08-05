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
#include <bitcoin/server/protocols/protocol_btcd_rpc.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd_rpc

// protocol_bitcoind_rpc declares 'using post = network::http::method::post',
// which shadows network::protocol::post<Derived>. Qualify explicitly.
#define POST_BTCD(method, ...) \
    this->network::protocol::template post<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Filter (loadtxfilter).
// ----------------------------------------------------------------------------
// The watch-list is matched via the address index and spender lookups (the
// electrum subscription model), never by block scanning. Watches are keyed
// by index key (sha256 of the output script), matched by cursored history
// delta, so each connected block costs one bounded index walk per watch.

code protocol_btcd_rpc::parse_filter_keys(const value_t& addresses,
    std::vector<hash_digest>& out) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!std::holds_alternative<array_t>(addresses.value()))
        return error::invalid_argument;

    for (const auto& item: std::get<array_t>(addresses.value()))
    {
        if (!std::holds_alternative<string_t>(item.value()))
            return error::invalid_argument;

        chain::script script{};
        if (const auto ec = parse_output_script(
            std::get<string_t>(item.value()), script))
            return ec;

        out.push_back(script.hash());
    }

    return error::success;
}

code protocol_btcd_rpc::parse_filter_points(const value_t& outpoints,
    std::vector<point>& out) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!std::holds_alternative<array_t>(outpoints.value()))
        return error::invalid_argument;

    for (const auto& item: std::get<array_t>(outpoints.value()))
    {
        if (!std::holds_alternative<object_t>(item.value()))
            return error::invalid_argument;

        const auto& fields = std::get<object_t>(item.value());
        const auto hash_it = fields.find("hash");
        const auto index_it = fields.find("index");
        if (hash_it == fields.end() || index_it == fields.end() ||
            !std::holds_alternative<string_t>(hash_it->second.value()) ||
            !std::holds_alternative<number_t>(index_it->second.value()))
            return error::invalid_argument;

        hash_digest hash{};
        uint32_t index{};
        if (!decode_hash(hash, std::get<string_t>(hash_it->second.value())) ||
            !to_integer(index, std::get<number_t>(index_it->second.value())))
            return error::invalid_argument;

        out.emplace_back(hash, index);
    }

    return error::success;
}

// Handlers (address/outpoint filtering).
// ----------------------------------------------------------------------------

bool protocol_btcd_rpc::handle_load_tx_filter(const code& ec,
    btcd_interface::load_tx_filter, bool reload, const value_t& addresses,
    const value_t& outpoints) NOEXCEPT
{
    if (stopped(ec))
        return false;

    std::vector<hash_digest> keys{};
    if (const auto fault = parse_filter_keys(addresses, keys))
    {
        send_error(fault);
        return true;
    }

    std::vector<point> points{};
    if (const auto fault = parse_filter_points(outpoints, points))
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
    POST_NOTIFY(do_load_tx_filter, reload, std::move(keys),
        std::move(points));
    return true;
}

void protocol_btcd_rpc::do_load_tx_filter(bool reload,
    const std::vector<hash_digest>& keys,
    const std::vector<point>& points) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    if (reload)
    {
        address_watches_.clear();
        outpoint_watches_.clear();
    }

    code ec{ error::success };
    const auto& query = archive();
    const auto maximum = server_settings().btcd.maximum_filters;
    const auto limit = server_settings().btcd.maximum_history;

    for (const auto& key: keys)
    {
        if (stopping_)
            return;

        if (address_watches_.size() + outpoint_watches_.size() >= maximum)
        {
            ec = error::subscription_limit;
            break;
        }

        // Prime the cursor to present, so matching reports new blocks only.
        const auto at = address_watches_.try_emplace(key, address_watch{});
        if (at.second)
        {
            histories discard{};
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

        if (address_watches_.size() + outpoint_watches_.size() >= maximum)
        {
            ec = error::subscription_limit;
            break;
        }

        // Prime the spender set to present, so matching reports new only.
        const auto at = outpoint_watches_.try_emplace(prevout,
            outpoint_watch{});
        if (at.second)
        {
            auto& sub = at.first->second;
            sub.outpoint = query.get_tx_history(query.to_tx(prevout.hash()));
            sub.spenders = query.get_spenders_history(prevout);
        }
    }

    POST_BTCD(complete_load_tx_filter, ec);
}

void protocol_btcd_rpc::complete_load_tx_filter(const code& ec) NOEXCEPT
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

bool protocol_btcd_rpc::handle_rescan_blocks(const code& ec,
    btcd_interface::rescan_blocks, const value_t& blockhashes) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!std::holds_alternative<array_t>(blockhashes.value()))
    {
        send_error(error::invalid_argument);
        return true;
    }

    std::vector<hash_digest> hashes{};
    for (const auto& item: std::get<array_t>(blockhashes.value()))
    {
        hash_digest hash{};
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            send_error(error::invalid_argument);
            return true;
        }

        hashes.push_back(hash);
    }

    if (!archive().address_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    monitor(true);
    POST_NOTIFY(do_rescan_blocks, std::move(hashes));
    return true;
}

// Snapshot the watch-list on the notification strand, so the query itself
// runs parallel (as electrum's one-shot queries) without serializing against
// this channel's notifications.
void protocol_btcd_rpc::do_rescan_blocks(
    const std::vector<hash_digest>& hashes) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    std::vector<hash_digest> keys{};
    keys.reserve(address_watches_.size());
    for (const auto& watch: address_watches_)
        keys.push_back(watch.first);

    std::vector<point> points{};
    points.reserve(outpoint_watches_.size());
    for (const auto& watch: outpoint_watches_)
        points.push_back(watch.first);

    PARALLEL(do_rescan_watches, hashes, std::move(keys), std::move(points));
}

void protocol_btcd_rpc::do_rescan_watches(
    const std::vector<hash_digest>& hashes,
    const std::vector<hash_digest>& keys,
    const std::vector<point>& points) NOEXCEPT
{
    BC_ASSERT(!stranded());

    // Resolve named blocks to heights (result retains request order).
    std::set<size_t> heights{};
    std::vector<std::pair<hash_digest, size_t>> named{};
    const auto& query = archive();
    for (const auto& hash: hashes)
    {
        if (stopping_)
            return;

        size_t height{};
        if (!query.get_height(height, query.to_header(hash)))
        {
            POST_BTCD(complete_rescan_blocks, error::not_found, array_t{});
            return;
        }

        heights.insert(height);
        named.emplace_back(hash, height);
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

    POST_BTCD(complete_rescan_blocks, error::success, std::move(discovered));
}

void protocol_btcd_rpc::complete_rescan_blocks(const code& ec,
    const array_t& discovered) NOEXCEPT
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

    send_result(discovered, 256);
}

// Notification event handlers.
// ----------------------------------------------------------------------------

void protocol_btcd_rpc::do_connected(node::header_t link_value) NOEXCEPT
{
    BC_ASSERT(notification_strand_.running_in_this_thread());

    const database::header_link link{ link_value };
    const auto& query = archive();

    size_t height{};
    if (!query.get_height(height, link))
        return;

    const auto header = query.get_header(link);
    if (!header)
        return;

    // Match the watch-list against the connected block (cursored delta).
    // Cursors advance here, so this stays on the notification strand.
    matches matched{};
    const std::set<size_t> heights{ height };
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

    POST_BTCD(notify_connected, encode_hash(query.get_header_key(link)),
        height, header->timestamp(),
        to_text(*header, chain::header::serialized_size()), std::move(txs));
}

void protocol_btcd_rpc::do_disconnected(node::header_t link_value) NOEXCEPT
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

    POST_BTCD(notify_disconnected, encode_hash(query.get_header_key(link)),
        height, header->timestamp(),
        to_text(*header, chain::header::serialized_size()));
}

// btcd 'blockconnected' [hash, height, time] and 'filteredblockconnected'
// [height, header, subscribedtxs]. filtered is sent unconditionally
// alongside (as btcd) -- an empty filter just yields an empty array.
void protocol_btcd_rpc::notify_connected(const std::string& hash,
    size_t height, uint32_t time, const std::string& header,
    const array_t& txs) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(std::memory_order_relaxed))
        return;

    send_notification("blockconnected", array_t
    {
        hash,
        height,
        time
    }, 256);

    send_notification("filteredblockconnected", array_t
    {
        height,
        header,
        txs
    }, 256);
}

// btcd 'blockdisconnected' [hash, height, time] and
// 'filteredblockdisconnected' [height, header].
void protocol_btcd_rpc::notify_disconnected(const std::string& hash,
    size_t height, uint32_t time, const std::string& header) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped() || !subscribed_blocks_.load(std::memory_order_relaxed))
        return;

    send_notification("blockdisconnected", array_t
    {
        hash,
        height,
        time
    }, 256);

    send_notification("filteredblockdisconnected", array_t
    {
        height,
        header
    }, 256);
}

// Utilities (notification strand).
// ----------------------------------------------------------------------------

// Called from the notification strand (live, cursors advance) and parallel
// (rescan, against a snapshot), so neither strand is asserted here.
code protocol_btcd_rpc::match_addresses(matches& out, address_watch& sub,
    const hash_digest& key, const std::set<size_t>& heights) NOEXCEPT
{
    histories delta{};
    const auto& query = archive();
    const auto limit = server_settings().btcd.maximum_history;
    const auto ec = query.get_history(stopping_, sub.cursor, delta, key,
        limit, turbo_);
    if (ec)
        return ec;

    for (const auto& entry: delta)
        if (entry.confirmed() && heights.contains(entry.tx.height()))
            out[entry.tx.height()].emplace(entry.position, entry.tx.hash());

    return error::success;
}

void protocol_btcd_rpc::match_outpoints(matches& out, outpoint_watch& sub,
    const point& prevout, const std::set<size_t>& heights) NOEXCEPT
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

array_t protocol_btcd_rpc::serialize_matches(const matched_txs& txs) NOEXCEPT
{
    constexpr auto witness = true;
    array_t out{};
    const auto& query = archive();
    for (const auto& [position, hash]: txs)
    {
        if (stopping_)
            return out;

        const auto tx = query.get_transaction(query.to_tx(hash), witness);
        if (tx)
            out.emplace_back(to_text(*tx,
                tx->serialized_size(witness), witness));
    }

    return out;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
