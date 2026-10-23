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
#include <bitcoin/server/protocols/protocol_esplora.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_esplora

using namespace system;
using namespace boost::json;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The numbers of confirmed and unconfirmed txs of an address txs request.
constexpr size_t address_page = 25;
constexpr size_t mempool_page = 50;

// The address index is keyed by the hash of the output script.
bool protocol_esplora::to_key(hash_digest& out,
    const std::optional<hash_cptr>& hash,
    const std::optional<std::string>& address) NOEXCEPT
{
    if (hash.has_value())
    {
        out = *hash.value();
        return true;
    }

    if (!address.has_value())
        return false;

    chain::script script{};
    if (output_script(script, address.value(), p2kh_, p2sh_, witness_))
        return false;

    out = sha256_hash(script.to_data(false));
    return true;
}

// address
// ----------------------------------------------------------------------------

bool protocol_esplora::handle_get_address(const code& ec, interface::address,
    uint8_t media, std::optional<hash_cptr> hash,
    std::optional<std::string> address) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    hash_digest key{};
    if (!to_key(key, hash, address))
    {
        send_bad_request();
        return true;
    }

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    PARALLEL(do_get_address, key, address, gate());
    return true;
}

void protocol_esplora::do_get_address(const hash_digest& key,
    const std::optional<std::string>& address,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    const auto limit = options().maximum_history;
    address_totals chain{}, mempool{};
    histories confirmed{}, unconfirmed{};
    height_link cursor{};

    auto ec = query.get_address_totals(stopping_, chain, mempool, key);
    if (!ec)
        ec = query.get_confirmed_history(stopping_, cursor, confirmed, key,
            limit, true);
    if (!ec)
        ec = query.get_unconfirmed_history(stopping_, unconfirmed, key,
            limit, true);

    POST(complete_get_address, ec, chain, confirmed.size(), mempool,
        unconfirmed.size(), key, address, gate);
}

void protocol_esplora::complete_get_address(const code& ec,
    const address_totals& chain, size_t chain_txs,
    const address_totals& mempool, size_t mempool_txs,
    const hash_digest& key, const std::optional<std::string>& address,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    object out{};
    if (address.has_value())
        out["address"] = address.value();
    else
        out["scripthash"] = encode_base16(key);

    const auto to_stats = [](const address_totals& totals, size_t txs) NOEXCEPT
    {
        return object
        {
            { "funded_txo_count", totals.funded_count },
            { "funded_txo_sum", totals.funded_value },
            { "spent_txo_count", totals.spent_count },
            { "spent_txo_sum", totals.spent_value },
            { "tx_count", txs }
        };
    };

    out["chain_stats"] = to_stats(chain, chain_txs);
    out["mempool_stats"] = to_stats(mempool, mempool_txs);

    send_json(std::move(out), 512);
}

// address/txs
// ----------------------------------------------------------------------------

bool protocol_esplora::handle_get_address_txs(const code& ec,
    interface::address_txs, uint8_t media, std::optional<hash_cptr> hash,
    std::optional<std::string> address) NOEXCEPT
{
    if (stopped(ec))
        return false;

    return get_address_txs(media, hash, address, {}, true, true);
}

bool protocol_esplora::handle_get_address_txs_chain(const code& ec,
    interface::address_txs_chain, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<std::string> address,
    std::optional<hash_cptr> last_seen) NOEXCEPT
{
    if (stopped(ec))
        return false;

    return get_address_txs(media, hash, address, last_seen, false, true);
}

bool protocol_esplora::handle_get_address_txs_mempool(const code& ec,
    interface::address_txs_mempool, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<std::string> address) NOEXCEPT
{
    if (stopped(ec))
        return false;

    return get_address_txs(media, hash, address, {}, true, false);
}

// common
bool protocol_esplora::get_address_txs(uint8_t media,
    const std::optional<hash_cptr>& hash,
    const std::optional<std::string>& address,
    const std::optional<hash_cptr>& last_seen, bool mempool,
    bool chain) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    hash_digest key{};
    if (!to_key(key, hash, address))
    {
        send_bad_request();
        return true;
    }

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    PARALLEL(do_get_address_txs, key, last_seen, mempool, chain, gate());
    return true;
}

void protocol_esplora::do_get_address_txs(const hash_digest& key,
    const std::optional<hash_cptr>& last_seen, bool mempool, bool chain,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    const auto limit = options().maximum_history;
    histories confirmed{}, unconfirmed{};
    height_link cursor{};
    code ec{};

    if (mempool)
        ec = query.get_unconfirmed_history(stopping_, unconfirmed, key,
            limit, true);
    if (!ec && chain)
        ec = query.get_confirmed_history(stopping_, cursor, confirmed, key,
            limit, true);

    POST(complete_get_address_txs, ec, std::move(unconfirmed),
        std::move(confirmed), last_seen, gate);
}

void protocol_esplora::complete_get_address_txs(const code& ec,
    const histories& unconfirmed, const histories& confirmed,
    const std::optional<hash_cptr>& last_seen, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    // The interface sorts newest first, the index sorts oldest first.
    auto first = confirmed.crbegin();
    if (last_seen.has_value())
    {
        const auto seen = std::find_if(first, confirmed.crend(),
            [&](const auto& item) NOEXCEPT
            {
                return item.tx.hash() == *last_seen.value();
            });

        if (seen == confirmed.crend())
        {
            send_not_found();
            return;
        }

        first = std::next(seen);
    }

    const auto& query = archive();
    const auto append = [&](array& out, const history& item) NOEXCEPT
    {
        object tx{};
        if (!to_tx(tx, query.to_tx(item.tx.hash())))
            return false;

        out.emplace_back(std::move(tx));
        return true;
    };

    array out{};
    const auto pooled = std::min(mempool_page, unconfirmed.size());
    const auto pooled_end = std::next(unconfirmed.crbegin(), pooled);
    for (auto item = unconfirmed.crbegin(); item != pooled_end; ++item)
    {
        if (!append(out, *item))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }
    }

    const auto remaining = std::distance(first, confirmed.crend());
    const auto paged = std::min(address_page, to_unsigned(remaining));
    const auto paged_end = std::next(first, paged);
    for (auto item = first; item != paged_end; ++item)
    {
        if (!append(out, *item))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }
    }

    send_json(std::move(out), (mempool_page + address_page) * 1024);
}

// address/utxo
// ----------------------------------------------------------------------------

bool protocol_esplora::handle_get_address_utxo(const code& ec,
    interface::address_utxo, uint8_t media, std::optional<hash_cptr> hash,
    std::optional<std::string> address) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    hash_digest key{};
    if (!to_key(key, hash, address))
    {
        send_bad_request();
        return true;
    }

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    PARALLEL(do_get_address_utxo, key, gate());
    return true;
}

void protocol_esplora::do_get_address_utxo(const hash_digest& key,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(!stranded());

    unspent_outputs unspent{};
    const auto ec = archive().get_unspent(stopping_, unspent, key, true);
    POST(complete_get_address_utxo, ec, std::move(unspent), gate);
}

void protocol_esplora::complete_get_address_utxo(const code& ec,
    const unspent_outputs& unspent, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    const auto& query = archive();
    array out{};
    for (const auto& item: unspent)
    {
        const auto& point = item.out.point();
        out.emplace_back(object
        {
            { "txid", encode_hash(point.hash()) },
            { "vout", point.index() },
            { "value", item.out.value() },
            { "status", to_status(query.to_tx(point.hash())) }
        });
    }

    send_json(std::move(out), add1(unspent.size()) * 256);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
