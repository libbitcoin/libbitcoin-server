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
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace std::placeholders;

#define CLASS protocol_native

// The vsize of a fee histogram bin (electrs).
constexpr size_t histogram_bin = 50'000;

BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_native::handle_get_tx(const code& ec, interface::tx, uint8_t,
    uint8_t media, const hash_cptr& hash, bool witness) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = query.to_tx(*hash);
    if (query.is_pruned(link))
    {
        send_not_found();
        return;
    }

    if (const auto tx = query.get_transaction(link, witness))
    {
        const auto size = tx->serialized_size(witness);
        switch (media)
        {
            case data:
                send_chunk(to_bin(*tx, size, witness));
                return;
            case text:
                send_text(to_hex(*tx, size, witness));
                return;
            case json:
                send_json(value_from(tx));
                return;
        }
    }

    send_not_found();
}

void protocol_native::handle_get_tx_header(const code& ec,
    interface::tx_header, uint8_t, uint8_t media,
    const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto& query = archive();
    const auto link = query.find_confirmed_block(*hash);
    if (link.is_terminal())
    {
        send_not_found();
        return;
    }

    const auto header = query.get_header(link);
    if (!header)
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    constexpr auto size = chain::header::serialized_size();
    switch (media)
    {
        case data:
            send_chunk(to_bin(*header, size));
            return;
        case text:
            send_text(to_hex(*header, size));
            return;
        case json:
            auto model = value_from(header);
            inject(model, {}, link);
            send_json(std::move(model));
            return;
    }

    send_not_found();
}

void protocol_native::handle_get_tx_details(const code& ec,
    interface::tx_details, uint8_t, uint8_t media,
    const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (media != json)
    {
        send_not_acceptable();
        return;
    }

    const auto& query = archive();
    const auto link = query.to_tx(*hash);

    // Missing tx.
    if (link.is_terminal())
    {
        send_not_found();
        return;
    }

    uint64_t value{}, spend{};
    size_t nominal{}, maximal{};
    const auto coinbase = query.is_coinbase(link);
    if (!query.get_tx_sizes(nominal, maximal, link) ||
        !query.get_tx_value(value, link) ||
        !query.get_tx_spend(spend, link) ||
        (!coinbase && is_subtract_overflow(value, spend)))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    // sigops and wtxid are not cached, so removed for now.
    system::json::object object
    {
        { "segregated", maximal != nominal },
        { "coinbase", coinbase },
        { "nominal", nominal },
        { "maximal", maximal },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "virtual", chain::virtual_size(nominal, maximal) },
        { "value", value },
        { "spend", spend },
        { "fee", floored_subtract(value, spend) }
    };

    if (const auto block = query.find_strong(link); !block.is_terminal())
    {
        size_t position{};
        database::context context{};
        if (!query.get_context(context, block) ||
            !query.get_tx_position(position, link, block))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        object["confirmed"] = system::json::object
        {
            { "height", context.height },
            { "position", position }
        };
    }
    else if (uint32_t time{}; query.get_pooled_time(time, link))
    {
        object["time"] = time;
    }

    send_json(std::move(object));
}

void protocol_native::handle_get_tx_subscribe(const code& ec,
    interface::tx_subscribe, uint8_t, uint8_t media, bool stop) NOEXCEPT
{
    if (stopped(ec))
        return;

    tx_subscribe_.store(to_media(stop, media), std::memory_order_relaxed);
    notify_empty();
}

// notify
void protocol_native::do_transaction(node::transaction_t link,
    media_type media) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto hash = archive().get_tx_key(link);
    switch (to_value(media))
    {
        case data:
            notify_chunk(to_chunk(hash));
            return;
        case text:
            notify_text(encode_base16(hash));
            return;
        case json:
            notify_json(value_from(encode_hash(hash)));
            return;
    }
}

void protocol_native::handle_get_template_subscribe(const code& ec,
    interface::template_subscribe, uint8_t, uint8_t media, bool stop) NOEXCEPT
{
    if (stopped(ec))
        return;

    // Templates are built for notification only while demanded.
    const auto prior = template_subscribe_.exchange(to_media(stop, media));
    if (prior == media_type::unknown && !stop)
        demand_template(true);
    else if (prior != media_type::unknown && stop)
        demand_template(false);

    notify_empty();
}

// notify
void protocol_native::do_template(node::height_t height,
    media_type media) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto value = possible_narrow_cast<uint32_t>(height);
    switch (to_value(media))
    {
        case data:
            notify_chunk(to_chunk(to_little_endian(value)));
            return;
        case text:
            notify_text(encode_base16(to_little_endian(value)));
            return;
        case json:
            notify_json(value);
            return;
    }
}

void protocol_native::handle_get_template(const code& ec,
    interface::template_, uint8_t, uint8_t media, uint32_t height) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (media != json)
    {
        send_not_acceptable();
        return;
    }

    get_template_view(height, template_view::summary, media);
}

void protocol_native::handle_get_template_txs(const code& ec,
    interface::template_txs, uint8_t, uint8_t media, uint32_t height) NOEXCEPT
{
    if (stopped(ec))
        return;

    get_template_view(height, template_view::txs, media);
}

void protocol_native::handle_get_template_txs_details(const code& ec,
    interface::template_txs_details, uint8_t, uint8_t media,
    uint32_t height) NOEXCEPT
{
    if (stopped(ec))
        return;

    get_template_view(height, template_view::details, media);
}

// private
// Only the template of the next block is held.
void protocol_native::get_template_view(uint32_t height, template_view view,
    uint8_t media) NOEXCEPT
{
    if (add1(archive().get_top_confirmed()) != height)
    {
        send_not_found();
        return;
    }

    get_template(BIND(handle_template, _1, _2, height, view, media, gate()));
}

void protocol_native::handle_template(const code& ec,
    const node::block_template::cptr& held, uint32_t height,
    template_view view, uint8_t media, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_get_template, ec, held, height, view, media, gate);
}

void protocol_native::complete_get_template(const code& ec,
    const node::block_template::cptr& held, uint32_t height,
    template_view view, uint8_t media, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec == node::error::pooling_disabled)
    {
        if (network_settings().enable_relay)
            send_service_unavailable();
        else
            send_not_implemented();

        return;
    }

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    // A template held for another height is of a chain that has since moved.
    if (held->context.height != height)
    {
        if (add1(archive().get_top_confirmed()) == height)
            get_template(BIND(handle_template, _1, _2, height, view, media,
                gate));
        else
            send_not_found();

        return;
    }

    switch (view)
    {
        case template_view::summary:
            send_template_summary(*held);
            return;
        case template_view::txs:
            send_template_txs(*held, media);
            return;
        case template_view::details:
            send_template_txs_details(*held, media);
            return;
    }
}

void protocol_native::send_template_summary(
    const node::block_template& held) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    size_t nominal{}, maximal{};
    for (const auto& link: held.links)
    {
        size_t light{}, heavy{};
        if (!query.get_tx_sizes(light, heavy, link))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        nominal = ceilinged_add(nominal, light);
        maximal = ceilinged_add(maximal, heavy);
    }

    auto roots = to_roots(held.txs);
    std::vector<bool> labeled(roots.size(), false);
    size_t clusters{};
    for (size_t index{}; index < roots.size(); ++index)
    {
        const auto root = to_root(roots, index);
        if (!labeled.at(root))
        {
            labeled.at(root) = true;
            ++clusters;
        }
    }

    send_json(system::json::object
    {
        { "height", held.context.height },
        { "count", held.links.size() },
        { "virtual", chain::virtual_size(nominal, maximal) },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "fees", held.fees },
        { "clusters", clusters }
    });
}

void protocol_native::send_template_txs(const node::block_template& held,
    uint8_t media) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Position zero is the coinbase, which is not of the template.
    hashes txids{ null_hash };
    txids.reserve(add1(held.txs.size()));
    for (const auto& tx: held.txs)
        txids.push_back(tx.txid);

    const auto size = txids.size() * hash_size;
    const auto bytes = pointer_cast<const uint8_t>(txids.data());
    switch (media)
    {
        case data:
            send_chunk(to_chunk({ bytes, std::next(bytes, size) }));
            return;
        case text:
            send_text(encode_base16({ bytes, std::next(bytes, size) }));
            return;
        case json:
        {
            system::json::array out(txids.size());
            std::ranges::transform(txids, out.begin(),
                [](const auto& hash) { return encode_hash(hash); });
            send_json(std::move(out));
            return;
        }
    }

    send_not_acceptable();
}

void protocol_native::send_template_txs_details(
    const node::block_template& held, uint8_t media) NOEXCEPT
{
    BC_ASSERT(stranded());
    tx_details txs{};
    if (!to_tx_details(txs, held.links, to_roots(held.txs)))
    {
        send_internal_server_error(database::error::integrity);
        return;
    }

    const auto weight = size_t{ node_settings().pool.coinbase_weight };
    const auto base = floored_subtract(weight, coinbase_witness_size) /
        chain::light_weight_factor;
    const auto maximal = base + coinbase_witness_size;
    const auto nominal = floored_subtract(maximal,
        coinbase_witness_size + coinbase_commitment_size);
    const auto bip42 = held.context.is_enabled(chain::flags::bip42_rule);
    const auto reward = ceilinged_add(to_subsidy(held.context.height, bip42),
        held.fees);

    tx_details out{};
    out.reserve(add1(txs.size()));
    out.push_back(tx_detail
    {
        possible_narrow_cast<uint32_t>(nominal),
        possible_narrow_cast<uint32_t>(maximal),
        reward,
        reward,
        0u,
        0u,
        0u
    });

    for (auto& tx: txs)
    {
        ++tx.cluster;
        out.push_back(tx);
    }

    send_tx_details(out, media);
}

void protocol_native::handle_get_txs(const code& ec, interface::txs,
    uint8_t, uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_not_implemented();
}

void protocol_native::handle_get_txs_details(const code& ec,
    interface::txs_details, uint8_t, uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_not_implemented();
}

void protocol_native::handle_get_txs_confirmed(const code& ec,
    interface::txs_confirmed, uint8_t, uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_not_implemented();
}

void protocol_native::handle_get_txs_confirmed_details(const code& ec,
    interface::txs_confirmed_details, uint8_t, uint8_t) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_not_implemented();
}

void protocol_native::handle_get_txs_unconfirmed(const code& ec,
    interface::txs_unconfirmed, uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return;

    get_txs_unconfirmed(false, media);
}

void protocol_native::handle_get_txs_unconfirmed_details(const code& ec,
    interface::txs_unconfirmed_details, uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return;

    get_txs_unconfirmed(true, media);
}

void protocol_native::get_txs_unconfirmed(bool details,
    uint8_t media) NOEXCEPT
{
    get_confirmable_txs(BIND(handle_txs_unconfirmed, _1, _2, details, media,
        gate()));
}

void protocol_native::handle_txs_unconfirmed(const code& ec,
    const database::tx_links& links, bool details, uint8_t media,
    const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_get_txs_unconfirmed, ec, links, details, media, gate);
}

void protocol_native::complete_get_txs_unconfirmed(const code& ec,
    const database::tx_links& links, bool details, uint8_t media,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec == node::error::pooling_disabled)
    {
        if (network_settings().enable_relay)
            send_service_unavailable();
        else
            send_not_implemented();

        return;
    }

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    if (details)
    {
        tx_details out{};
        if (!to_tx_details(out, links, to_roots(links)))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        const auto& query = archive();
        for (size_t index{}; index < links.size(); ++index)
            query.get_pooled_time(out.at(index).time, links.at(index));

        send_tx_details(out, media, true);
        return;
    }

    const auto& query = archive();
    hashes txids(links.size());
    std::ranges::transform(links, txids.begin(),
        [&](const auto& link) NOEXCEPT { return query.get_tx_key(link); });

    const auto size = txids.size() * hash_size;
    const auto bytes = pointer_cast<const uint8_t>(txids.data());
    switch (media)
    {
        case data:
            send_chunk(to_chunk({ bytes, std::next(bytes, size) }));
            return;
        case text:
            send_text(encode_base16({ bytes, std::next(bytes, size) }));
            return;
        case json:
        {
            system::json::array out(txids.size());
            std::ranges::transform(txids, out.begin(),
                [](const auto& hash) { return encode_hash(hash); });
            send_json(std::move(out));
            return;
        }
    }

    send_not_acceptable();
}

void protocol_native::handle_get_fees(const code& ec, interface::fees,
    uint8_t, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (media != json)
    {
        send_not_acceptable();
        return;
    }

    get_confirmable_fees(BIND(handle_fees, _1, _2, gate()));
}

void protocol_native::handle_fees(const code& ec,
    const database::fee_rates& rates, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_get_fees, ec, rates, gate);
}

void protocol_native::complete_get_fees(const code& ec,
    const database::fee_rates& rates, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    // Closed pool is not implemented with relay disabled, and is otherwise
    // unavailable until it opens.
    if (ec == node::error::pooling_disabled)
    {
        if (network_settings().enable_relay)
            send_service_unavailable();
        else
            send_not_implemented();

        return;
    }

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    const auto summary = to_fee_summary(rates, histogram_bin);
    system::json::object object
    {
        { "count", summary.count },
        { "virtual", summary.vsize },
        { "fee", summary.fee },
        { "histogram", system::json::value_from(summary.histogram) }
    };

    send_json(std::move(object));
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
