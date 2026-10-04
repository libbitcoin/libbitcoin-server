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
#include <bitcoin/server/protocols/protocol_sparrow.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_sparrow
#define SUBSCRIBE_SPARROW(method, ...) \
    sparrow_subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

constexpr auto relaxed = std::memory_order_relaxed;
constexpr size_t page_size = 1000;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Start.
// ----------------------------------------------------------------------------

void protocol_sparrow::start() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (started())
        return;

    SUBSCRIBE_SPARROW(handle_blockchain_block_stats, _1, _2, _3);
    SUBSCRIBE_SPARROW(handle_blockchain_silent_payments_subscribe, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_SPARROW(handle_blockchain_silent_payments_unsubscribe, _1, _2, _3, _4);

    protocol_electrum::start();
}

void protocol_sparrow::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    cancel_.store(true);
    sparrow_dispatcher_.stop(ec);
    protocol_electrum::stopping(ec);
}

// Handlers (event subscription).
// ----------------------------------------------------------------------------

// Archived txs and organized blocks add rows, and scans coalesce while queued.
bool protocol_sparrow::handle_chase(const code& ec,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    // Notifications require a full duplex transport.
    if (!is_duplex())
        return true;

    switch (node::to_chase(value))
    {
        case node::chase::transaction:
        case node::chase::organized:
        {
            if (subscribed_silent_.load(relaxed) &&
                !queued_silent_.exchange(true))
            {
                POST_NOTIFY(do_silent, node::header_t{});
            }

            break;
        }
        default:
        {
            break;
        }
    }

    // Silent payment notifications precede those of the electrum protocol.
    return protocol_electrum::handle_chase(ec, value);
}

// Dispatch.
// ----------------------------------------------------------------------------

// A method the electrum interface does not define is the sparrow interface,
// or is not served at all (the electrum terminal responds).
void protocol_sparrow::handle_unclaimed(const request_t& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!sparrow_dispatcher::contains(message.method))
    {
        protocol_electrum::handle_unclaimed(message);
        return;
    }

    if (const auto ec = sparrow_dispatcher_.notify(message))
        stop(ec);
}

// Features.
// ----------------------------------------------------------------------------

// Silent payment (bip352) support, as the version list frigate publishes.
void protocol_sparrow::add_features(object_t& features) const NOEXCEPT
{
    features["silent_payments"] = array_t{ silent_payments_version };
}

// Handlers (stubs, claimed but not yet bound to the store).
// ----------------------------------------------------------------------------
// github.com/sparrowwallet/frigate ElectrumServerService

void protocol_sparrow::handle_blockchain_block_stats(const code& ec,
    sparrow_interface::blockchain_block_stats, double) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    // TODO: height -> { height, blockhash, feerate_percentiles, total_weight,
    // TODO: txs, time }.
    send_code(error::electrum::method_not_found);
}

// Subscribe.
// ----------------------------------------------------------------------------

// The client sends the scan secret, never the spend secret (bip352).
void protocol_sparrow::handle_blockchain_silent_payments_subscribe(
    const code& ec, sparrow_interface::blockchain_silent_payments_subscribe,
    const std::string& scan_private_key, const std::string& spend_public_key,
    const interface::value_t& start, const interface::array_t& labels) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    if (!archive().silent_enabled())
    {
        send_code(error::electrum::method_not_found);
        return;
    }

    silent_cut cut{};
    ec_secret scan{};
    ec_compressed spend{};
    std::vector<uint32_t> scan_labels{};
    if (!decode_base16(scan, scan_private_key) ||
        !decode_base16(spend, spend_public_key) ||
        !to_labels(scan_labels, labels) || !to_cut(cut, start))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    silent_payment scanner{ scan, spend, scan_labels };
    if (!scanner)
    {
        send_code(error::electrum::bad_request);
        return;
    }

    array_t values{};
    for (const auto label: scan_labels)
        values.emplace_back(label);

    const auto address = to_address(scan, spend);
    silent_subscription subscription
    {
        .scanner = std::move(scanner),
        .subscription = object_t
        {
            { "address", address },
            { "labels", std::move(values) },
            { "start_height", cut.start }
        },
        .cut = cut
    };

    POST_NOTIFY(do_silent_subscribe, address, std::move(subscription), gate());
}

// A resubscription never narrows its start and always rescans its history.
void protocol_sparrow::do_silent_subscribe(const std::string& address,
    const silent_subscription& subscription, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(notifying());

    auto it = silent_subscriptions_.find(address);
    if (it == silent_subscriptions_.end())
    {
        if (silent_subscriptions_.size() >= options().maximum_subscriptions)
        {
            POST(complete_silent_subscribe,
                error::electrum::excessive_resource_usage, object_t{}, gate);
            return;
        }

        it = silent_subscriptions_.emplace(address, subscription).first;
    }
    else
    {
        auto& current = it->second;
        const auto same = current.cut.time == subscription.cut.time;
        const auto start = std::min(current.cut.start, subscription.cut.start);
        current = subscription;
        if (same)
        {
            current.cut.start = start;
            current.subscription["start_height"] = start;
        }
    }

    auto& sub = it->second;
    sub.cursor = zero;
    subscribed_silent_.store(true, relaxed);
    POST(complete_silent_subscribe, error::success, sub.subscription, gate);

    const auto& query = archive();
    scan_silent(sub, query.get_silent_frontier(zero), true);
}

void protocol_sparrow::complete_silent_subscribe(const code& ec,
    const object_t& result, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return;

    if (ec)
    {
        using namespace error::electrum;
        send_code(translate(ec, daemon_error));
        return;
    }

    send_result(value_t{ result });
}

// Unsubscribe.
// ----------------------------------------------------------------------------

void protocol_sparrow::handle_blockchain_silent_payments_unsubscribe(
    const code& ec, sparrow_interface::blockchain_silent_payments_unsubscribe,
    const std::string& scan_private_key,
    const std::string& spend_public_key) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    ec_secret scan{};
    ec_compressed spend{};
    if (!decode_base16(scan, scan_private_key) ||
        !decode_base16(spend, spend_public_key) || !verify_secret(scan))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    POST_NOTIFY(do_silent_unsubscribe, to_address(scan, spend));
}

void protocol_sparrow::do_silent_unsubscribe(
    const std::string& address) NOEXCEPT
{
    BC_ASSERT(notifying());

    const auto found = to_bool(silent_subscriptions_.erase(address));
    if (silent_subscriptions_.empty())
        subscribed_silent_.store(false, relaxed);

    const auto result = found ? value_t{ address } : value_t{};
    POST(complete_silent_unsubscribe, result);
}

void protocol_sparrow::complete_silent_unsubscribe(
    const value_t& result) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return;

    send_result(value_t{ result });
}

// Notify.
// ----------------------------------------------------------------------------

// Rows are scanned from each cursor up to the written frontier.
void protocol_sparrow::do_silent(node::header_t) NOEXCEPT
{
    BC_ASSERT(notifying());

    queued_silent_.store(false);
    if (silent_subscriptions_.empty())
        return;

    auto low = max_size_t;
    for (const auto& [address, subscription]: silent_subscriptions_)
        low = std::min(low, subscription.cursor);

    const auto& query = archive();
    const auto last = query.get_silent_frontier(low);
    for (auto& [address, subscription]: silent_subscriptions_)
        scan_silent(subscription, last, false);
}

// History is sent in pages, and the last page completes progress.
void protocol_sparrow::scan_silent(silent_subscription& subscription,
    size_t last, bool initial) NOEXCEPT
{
    BC_ASSERT(notifying());

    std::mutex mutex{};
    matches found{};
    auto& query = archive();
    const auto& keys = subscription.scanner.keys();
    if (!query.scan_silent(cancel_, keys, subscription.cursor, last,
        [&](const code&, tx_link_t link, const ec_compressed& tweak) NOEXCEPT
        {
            std::unique_lock lock{ mutex };
            found.emplace(link, tweak);
        }))
        return;

    subscription.cursor = last;
    auto history = confirm_silent(subscription, found);
    if (!initial && history.empty())
        return;

    const auto count = history.size();
    const auto pages = std::max(one, ceilinged_divide(count, page_size));
    for (size_t page{}; page < pages; ++page)
    {
        const auto first = page * page_size;
        const auto end = std::min(first + page_size, count);
        const auto progress = (add1(page) == pages) ? 1.0 :
            to_floating(end) / to_floating(count);

        array_t part{};
        part.reserve(end - first);
        std::move(std::next(history.begin(), first),
            std::next(history.begin(), end), std::back_inserter(part));

        POST(silent_notify, subscription.subscription, progress,
            std::move(part));
    }
}

// Confirmation is strong association, otherwise height is zero (unconfirmed).
// A tx archived only on a stale branch reports as unconfirmed, and one also
// archived there before the store began pooling may report twice (benign).
array_t protocol_sparrow::confirm_silent(
    const silent_subscription& subscription,
    const matches& found) const NOEXCEPT
{
    BC_ASSERT(notifying());

    const auto& query = archive();
    std::vector<std::tuple<size_t, hash_digest, ec_compressed>> rows{};
    for (const auto& [link, tweak]: found)
    {
        size_t height{};
        if (!query.get_tx_height(height, link))
            rows.emplace_back(zero, query.get_tx_key(link), tweak);
        else if (!is_cut(subscription.cut, link, height))
            rows.emplace_back(height, query.get_tx_key(link), tweak);
    }

    // Confirmed by height, then unconfirmed (zero) last.
    std::sort(rows.begin(), rows.end(),
        [](const auto& left, const auto& right) NOEXCEPT
        {
            return sub1(std::get<0>(left)) < sub1(std::get<0>(right));
        });

    array_t history{};
    history.reserve(rows.size());
    for (const auto& [height, hash, tweak]: rows)
    {
        history.emplace_back(object_t
        {
            { "height", height },
            { "tx_hash", encode_hash(hash) },
            { "tweak_key", encode_base16(tweak) }
        });
    }

    return history;
}

bool protocol_sparrow::is_cut(const silent_cut& cut, const tx_link_t& link,
    size_t height) const NOEXCEPT
{
    if (!cut.time)
        return height < cut.start || height > cut.stop;

    uint32_t timestamp{};
    const auto& query = archive();
    return !query.get_timestamp(timestamp, query.find_strong(link)) ||
        timestamp < cut.start || timestamp > cut.stop;
}

void protocol_sparrow::silent_notify(const object_t& subscription,
    double progress, const array_t& history) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped())
        return;

    send_notification("blockchain.silentpayments.subscribe", object_t
    {
        { "subscription", subscription },
        { "progress", progress },
        { "history", history }
    });
}

// Utility.
// ----------------------------------------------------------------------------

// A height or time, or a "FROM-TO" range of either (as locktime).
bool protocol_sparrow::to_cut(silent_cut& out,
    const interface::value_t& value) NOEXCEPT
{
    constexpr auto timestamp = chain::locktime_threshold;
    out = {};

    if (std::holds_alternative<null_t>(value.value()))
        return true;

    if (const auto number = std::get_if<number_t>(&value.value()))
    {
        uint32_t height_or_time{};
        if (!to_integer(height_or_time, *number))
            return false;

        out.start = height_or_time;
        out.time = out.start >= timestamp;
        return true;
    }

    if (const auto text = std::get_if<string_t>(&value.value()))
    {
        const auto parts = split(*text, "-");
        if (parts.size() != two || !deserialize(out.start, parts.front()) ||
            !deserialize(out.stop, parts.back()) || out.start > out.stop)
            return false;

        out.time = out.start >= timestamp;
        return out.time == (out.stop >= timestamp);
    }

    return false;
}

// Change (label zero) is always scanned.
bool protocol_sparrow::to_labels(std::vector<uint32_t>& out,
    const interface::array_t& labels) NOEXCEPT
{
    out.assign(one, zero);
    for (const auto& label: labels)
    {
        uint32_t value{};
        const auto number = std::get_if<number_t>(&label.value());
        if (is_null(number) || !to_integer(value, *number))
            return false;

        out.push_back(value);
    }

    distinct(out);
    return true;
}

// The address of the public key of the scan secret and the spend key.
std::string protocol_sparrow::to_address(const ec_secret& scan,
    const ec_compressed& spend) const NOEXCEPT
{
    ec_compressed key{};
    if (!secret_to_public(key, scan))
        return {};

    return silent_payment::to_address(key, spend, prefix_);
}

BC_POP_WARNING()

#undef SUBSCRIBE_SPARROW
#undef CLASS

} // namespace server
} // namespace libbitcoin
