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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <atomic>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

#define CLASS protocol_native
#define SUBSCRIBE_NATIVE(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

// Start.
// ----------------------------------------------------------------------------

void protocol_native::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Chaser subscription is asynchronous, events may be missed.
    subscribe_chase(BIND(handle_chase, _1, _2));

    // Configuration methods.
    SUBSCRIBE_NATIVE(handle_get_configuration, _1, _2, _3, _4);

    // Top methods.
    SUBSCRIBE_NATIVE(handle_get_top, _1, _2, _3, _4);
    SUBSCRIBE_NATIVE(handle_get_top_subscribe, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_top_chain, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_top_height, _1, _2, _3, _4, _5);

    // Block methods.
    SUBSCRIBE_NATIVE(handle_get_block, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_NATIVE(handle_get_block_header, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_header_context, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_details, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_txs, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_txs_details, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_block_filter, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_NATIVE(handle_get_block_filter_hash, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_NATIVE(handle_get_block_filter_header, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_NATIVE(handle_get_block_tx, _1, _2, _3, _4, _5, _6, _7, _8, _9);
    SUBSCRIBE_NATIVE(handle_get_block_tx_input_script, _1, _2, _3, _4, _5, _6, _7, _8, _9);
    SUBSCRIBE_NATIVE(handle_get_block_tx_output_script, _1, _2, _3, _4, _5, _6, _7, _8, _9);
    SUBSCRIBE_NATIVE(handle_get_block_subscribe, _1, _2, _3, _4, _5);

    // Transaction methods.
    SUBSCRIBE_NATIVE(handle_get_tx, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_tx_header, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_tx_details, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_tx_subscribe, _1, _2, _3, _4, _5);

    // Input methods.
    SUBSCRIBE_NATIVE(handle_get_inputs, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_input, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_NATIVE(handle_get_input_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_input_witness, _1, _2, _3, _4, _5, _6);

    // Output methods.
    SUBSCRIBE_NATIVE(handle_get_outputs, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_output, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_script, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_spender, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_spenders, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_output_subscribe, _1, _2, _3, _4, _5, _6, _7);

    // Address methods.
    SUBSCRIBE_NATIVE(handle_get_address, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_confirmed, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_unconfirmed, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_balance, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_NATIVE(handle_get_address_subscribe, _1, _2, _3, _4, _5, _6, _7);

    // Fee methods.
    SUBSCRIBE_NATIVE(handle_get_template, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_template_txs, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_template_txs_details, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_template_subscribe, _1, _2, _3, _4, _5);
    SUBSCRIBE_NATIVE(handle_get_fees, _1, _2, _3, _4);
    protocol_html::start();
}

void protocol_native::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    stopping_.store(true);
    if (template_subscribe_.exchange(media_type::unknown) != media_type::unknown)
        demand_template(false);

    dispatcher_.stop(ec);
    unsubscribe_chase();
    protocol_html::stopping(ec);
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_native::try_dispatch_object(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto target = request.target();
    BC_POP_WARNING()

    rpc::request_t model{};
    if (const auto ec = native_target(model, target))
    {
        // A target without a version or requesting html is a page request.
        if (ec == error::missing_version || ec == error::empty_path ||
            is_page_request(request))
            return false;

        send_bad_target(ec, *reset_request());
        return true;
    }

    // No media defaults injected for an http request.
    if (!native_query(model, request))
    {
        send_bad_request();
        return true;
    }

    const auto media = get_media(model);
    if (media == media_type::unknown)
    {
        send_not_acceptable();
        return true;
    }

    // Falls through to html page dispatch.
    if (media == media_type::text_html)
        return false;

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(model.method))
    {
        send_forbidden();
        return true;
    }

    if (dispatcher_.notify(model))
        send_bad_request();

    return true;
}

bool protocol_native::is_page_request(
    const http::request& request) const NOEXCEPT
{
    rpc::request_t model{ .params = rpc::object_t{} };
    return native_query(model, request) &&
        get_media(model) == media_type::text_html;
}

void protocol_native::dispatch_websocket(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // ws frames carry no headers, so ws authorization is enforced here, not by
    // channel (established by basic auth on upgrade request).
    if (!authorized())
    {
        send_unauthorized(request);
        return;
    }

    // Native websocket interface supports only string requests.
    if (!request.body().contains<http::string_value>())
    {
        send_not_acceptable(request);
        return;
    }

    // Target with query string is passed via websocket string body.
    const auto target = request.body().get<http::string_value>();

    rpc::request_t model{};
    if (const auto ec = native_target(model, target))
    {
        send_bad_target(ec, request);
        return;
    }

    // Default to json by simulating a json accept header (format overrides).
    if (!native_query(model, target, { media_type::application_json }))
    {
        send_bad_request(request);
        return;
    }

    if (get_media(model) == media_type::text_html)
    {
        send_not_acceptable(request);
        return;
    }

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(model.method))
    {
        send_forbidden(request);
        return;
    }

    if (dispatcher_.notify(model))
        send_bad_request(request);
}

// Senders.
// ----------------------------------------------------------------------------

void protocol_native::send_not_found(const http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (websocket())
    {
        send_empty();
        return;
    }

    protocol_html::send_not_found(request);
}

// Event handlers.
// ----------------------------------------------------------------------------

bool protocol_native::handle_chase(const code&,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    if (!websocket())
        return true;

    switch (node::to_chase(value))
    {
        case node::chase::block:
        {
            const auto block = node::to_payload<node::chase::block>(value);

            auto media = top_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                POST(do_top, media);
            }

            // No block emission for a fork point (start/reorg).
            media = block_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                POST(do_block, block.link, media);
            }

            break;
        }
        case node::chase::reorganized:
        case node::chase::headers:
        case node::chase::regressed:
        case node::chase::disorganized:
        {
            const auto media = top_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                POST(do_top, media);
            }

            break;
        }
        case node::chase::transaction:
        {
            const auto media = tx_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                POST(do_transaction,
                    node::to_payload<node::chase::transaction>(value).link,
                    media);
            }

            break;
        }
        case node::chase::template_:
        {
            const auto media = template_subscribe_.load(relaxed);
            if (media != media_type::unknown)
            {
                POST(do_template,
                    node::to_payload<node::chase::template_>(value).height,
                    media);
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

// Utilities.
// ----------------------------------------------------------------------------
// private

protocol_native::media_type protocol_native::to_media(bool stop,
    uint8_t media) NOEXCEPT
{
    return stop ? media_type::unknown : static_cast<media_type>(media);
}

using namespace system::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_native::inject(value& out, std::optional<uint32_t> height,
    const database::header_link& link) const NOEXCEPT
{
    out.as_object().emplace("height", height.has_value() ? height.value() :
        archive().get_height(link).value);
}

size_t protocol_native::to_root(std::vector<size_t>& roots,
    size_t index) NOEXCEPT
{
    while (roots.at(index) != index)
        index = roots.at(index) = roots.at(roots.at(index));

    return index;
}

std::vector<size_t> protocol_native::to_roots(
    const database::template_txs& txs) NOEXCEPT
{
    std::vector<size_t> roots(txs.size());
    std::iota(roots.begin(), roots.end(), zero);
    for (size_t index{}; index < txs.size(); ++index)
    {
        for (const auto parent: txs.at(index).parents)
        {
            const auto child = to_root(roots, index);
            roots.at(child) = to_root(roots, parent);
        }
    }

    return roots;
}

std::vector<size_t> protocol_native::to_roots(
    const database::tx_links& txs) const NOEXCEPT
{
    const auto& query = archive();
    const auto count = txs.size();
    std::unordered_map<hash_digest, size_t> positions{};
    positions.reserve(count);
    for (size_t index{}; index < count; ++index)
        positions.emplace(query.get_tx_key(txs.at(index)), index);

    std::vector<size_t> roots(count);
    std::iota(roots.begin(), roots.end(), zero);
    for (size_t index{}; index < count; ++index)
    {
        for (const auto& point: query.to_points(txs.at(index)))
        {
            const auto parent = positions.find(query.get_point_hash(point));
            if (parent == positions.end())
                continue;

            const auto child = to_root(roots, index);
            roots.at(child) = to_root(roots, parent->second);
        }
    }

    return roots;
}

// The details of each tx, with its cluster: the label, in order of first
// appearance, of its component under spends of other txs of the set.
bool protocol_native::to_tx_details(system::json::array& out,
    const database::tx_links& txs, std::vector<size_t>&& roots) const NOEXCEPT
{
    const auto& query = archive();
    const auto count = txs.size();
    out.clear();
    out.reserve(count);
    size_t clusters{};
    std::vector<size_t> labels(count, max_size_t);
    for (size_t index{}; index < count; ++index)
    {
        uint64_t value{}, spend{};
        size_t nominal{}, maximal{};
        const auto& link = txs.at(index);
        const auto coinbase = query.is_coinbase(link);
        if (!query.get_tx_sizes(nominal, maximal, link) ||
            !query.get_tx_value(value, link) ||
            !query.get_tx_spend(spend, link) ||
            (!coinbase && is_subtract_overflow(value, spend)))
            return false;

        auto& label = labels.at(to_root(roots, index));
        if (label == max_size_t)
            label = clusters++;

        out.emplace_back(system::json::object
        {
            { "segregated", maximal != nominal },
            { "nominal", nominal },
            { "maximal", maximal },
            { "weight", chain::weighted_size(nominal, maximal) },
            { "virtual", chain::virtual_size(nominal, maximal) },
            { "value", value },
            { "spend", spend },
            { "fee", floored_subtract(value, spend) },
            { "cluster", label }
        });
    }

    return true;
}

bool protocol_native::to_pruned_input(system::json::value& out,
    const database::ins_link& link) const NOEXCEPT
{
    const auto& query = archive();
    uint32_t sequence{};
    if (!query.get_input_sequence(sequence, link))
        return false;

    out = system::json::object
    {
        { "point", value_from(query.get_point(link)) },
        { "sequence", sequence }
    };

    return true;
}

bool protocol_native::to_pruned_inputs(system::json::array& out,
    const database::tx_link& link) const NOEXCEPT
{
    const auto points = archive().to_points(link);
    if (points.empty())
        return false;

    out.clear();
    out.reserve(points.size());
    for (const auto& point: points)
        if (!to_pruned_input(out.emplace_back(), point))
            return false;

    return true;
}

chain::header::cptr protocol_native::get_top(bool candidate,
    size_t& height) const NOEXCEPT
{
    const auto& query = archive();
    height = candidate ? query.get_top_candidate() : query.get_top_confirmed();
    return query.get_header(candidate ? query.to_candidate(height) :
        query.to_confirmed(height));
}

// A top is its checkpoint followed by its header.
data_chunk protocol_native::to_top(bool candidate) const NOEXCEPT
{
    size_t height{};
    const auto header = get_top(candidate, height);
    return splice(chain::checkpoint{ header->hash(), height }.to_data(),
        header->to_data());
}

value protocol_native::to_top_json(bool candidate) const NOEXCEPT
{
    size_t height{};
    const auto header = get_top(candidate, height);
    auto model = value_from(chain::checkpoint{ header->hash(), height });
    model.as_object().emplace("header", value_from(header));
    return model;
}

data_chunk protocol_native::to_tops() const NOEXCEPT
{
    return splice(to_top(false), to_top(true));
}

value protocol_native::to_tops_json() const NOEXCEPT
{
    return
    {
        { "confirmed", to_top_json(false) },
        { "candidate", to_top_json(true) }
    };
}

BC_POP_WARNING()

database::header_link protocol_native::to_header(
    const std::optional<uint32_t>& height,
    const std::optional<hash_cptr>& hash, bool candidate) NOEXCEPT
{
    const auto& query = archive();

    if (hash.has_value())
        return query.to_header(*(hash.value()));

    if (!height.has_value())
        return {};

    return candidate ? query.to_candidate(height.value()) :
        query.to_confirmed(height.value());
}

// Use if deserialization is required.
#if defined(UNDEFINED)
using inpoints = database::inpoints;
using outpoints = database::outpoints;

DECLARE_JSON_TAG_INVOKE(inpoints);
DECLARE_JSON_TAG_INVOKE(outpoints);

DEFINE_JSON_TO_TAG(inpoints)
{
    inpoints out{};
    for (const auto& point: value.as_array())
        out.insert(value_to<inpoint>(point));

    return out;
}

DEFINE_JSON_TO_TAG(outpoints)
{
    outpoints out{};
    for (const auto& point: value.as_array())
        out.insert(value_to<outpoint>(point));

    return out;
}
#endif

} // namespace server
} // namespace libbitcoin
