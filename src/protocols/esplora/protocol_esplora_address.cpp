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
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_esplora

using namespace system;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The number of confirmed transactions returned by an address txs request.
constexpr size_t address_page = 25;

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

    monitor(true);
    PARALLEL(do_get_address, key, address);
    return true;
}

void protocol_esplora::do_get_address(const hash_digest& key,
    const std::optional<std::string>& address) NOEXCEPT
{
    BC_ASSERT(!stranded());

    const auto& query = archive();
    outpoints funded{}, unspent{};
    histories history{};
    height_link cursor{};

    auto ec = query.get_address_outpoints(stopping_, funded, key, turbo_);
    if (!ec)
        ec = query.get_confirmed_unspent_outpoints(stopping_, unspent, key,
            turbo_);
    if (!ec)
        ec = query.get_confirmed_history(stopping_, cursor, history, key,
            options().maximum_history, turbo_);

    address_stats stats{};
    if (!ec)
    {
        stats.funded_count = funded.size();
        stats.spent_count = floored_subtract(funded.size(), unspent.size());
        stats.tx_count = history.size();

        uint64_t value{};
        for (const auto& point: funded)
            if (query.get_value(value, query.to_output(point)))
                stats.funded_sum = ceilinged_add(stats.funded_sum, value);

        uint64_t unspent_sum{};
        for (const auto& point: unspent)
            if (query.get_value(value, query.to_output(point)))
                unspent_sum = ceilinged_add(unspent_sum, value);

        stats.spent_sum = floored_subtract(stats.funded_sum, unspent_sum);
    }

    POST(complete_get_address, ec, stats, key, address);
}

void protocol_esplora::complete_get_address(const code& ec,
    const address_stats& stats, const hash_digest& key,
    const std::optional<std::string>& address) NOEXCEPT
{
    BC_ASSERT(stranded());
    monitor(false);

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    boost::json::object out{};
    if (address.has_value())
        out["address"] = address.value();
    else
        out["scripthash"] = encode_hash(key);

    out["chain_stats"] = boost::json::object
    {
        { "funded_txo_count", stats.funded_count },
        { "funded_txo_sum", stats.funded_sum },
        { "spent_txo_count", stats.spent_count },
        { "spent_txo_sum", stats.spent_sum },
        { "tx_count", stats.tx_count }
    };

    // There is no tx pool.
    out["mempool_stats"] = boost::json::object
    {
        { "funded_txo_count", 0 },
        { "funded_txo_sum", 0 },
        { "spent_txo_count", 0 },
        { "spent_txo_sum", 0 },
        { "tx_count", 0 }
    };

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

    // There is no tx pool, so this is the first confirmed page.
    return get_address_txs(media, hash, address, {});
}

bool protocol_esplora::handle_get_address_txs_chain(const code& ec,
    interface::address_txs_chain, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<std::string> address,
    std::optional<hash_cptr> last_seen) NOEXCEPT
{
    if (stopped(ec))
        return false;

    return get_address_txs(media, hash, address, last_seen);
}

bool protocol_esplora::handle_get_address_txs_mempool(const code& ec,
    interface::address_txs_mempool, uint8_t media,
    std::optional<hash_cptr> hash, std::optional<std::string> address) NOEXCEPT
{
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

    // There is no tx pool.
    send_json(boost::json::array{}, 42);
    return true;
}

// common
bool protocol_esplora::get_address_txs(uint8_t media,
    const std::optional<hash_cptr>& hash,
    const std::optional<std::string>& address,
    const std::optional<hash_cptr>& last_seen) NOEXCEPT
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

    monitor(true);
    PARALLEL(do_get_address_txs, key, last_seen);
    return true;
}

void protocol_esplora::do_get_address_txs(const hash_digest& key,
    const std::optional<hash_cptr>& last_seen) NOEXCEPT
{
    BC_ASSERT(!stranded());

    histories history{};
    height_link cursor{};
    const auto ec = archive().get_confirmed_history(stopping_, cursor,
        history, key, options().maximum_history, turbo_);

    POST(complete_get_address_txs, ec, std::move(history), last_seen);
}

void protocol_esplora::complete_get_address_txs(const code& ec,
    const histories& history,
    const std::optional<hash_cptr>& last_seen) NOEXCEPT
{
    BC_ASSERT(stranded());
    monitor(false);

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    // The interface sorts newest first, the index sorts oldest first.
    auto first = history.rbegin();
    if (last_seen.has_value())
    {
        const auto seen = std::ranges::find_if(first, history.rend(),
            [&](const auto& item) NOEXCEPT
            {
                return item.tx.hash() == *last_seen.value();
            });

        if (seen == history.rend())
        {
            send_not_found();
            return;
        }

        first = std::next(seen);
    }

    const auto& query = archive();
    boost::json::array out{};
    for (auto item = first; item != history.rend() &&
        out.size() < address_page; ++item)
    {
        boost::json::object object{};
        if (!to_tx(object, query.to_tx(item->tx.hash())))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        out.emplace_back(std::move(object));
    }

    send_json(std::move(out), address_page * 1024);
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

    monitor(true);
    PARALLEL(do_get_address_utxo, key);
    return true;
}

void protocol_esplora::do_get_address_utxo(const hash_digest& key) NOEXCEPT
{
    BC_ASSERT(!stranded());

    unspent_outputs unspent{};
    const auto ec = archive().get_confirmed_unspent(stopping_, unspent, key,
        turbo_);

    POST(complete_get_address_utxo, ec, std::move(unspent));
}

void protocol_esplora::complete_get_address_utxo(const code& ec,
    const unspent_outputs& unspent) NOEXCEPT
{
    BC_ASSERT(stranded());
    monitor(false);

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    const auto& query = archive();
    boost::json::array out{};
    for (const auto& item: unspent)
    {
        uint64_t value{};
        if (!query.get_value(value, query.to_output(item.out)))
        {
            send_internal_server_error(database::error::integrity);
            return;
        }

        out.emplace_back(boost::json::object
        {
            { "txid", encode_hash(item.out.hash()) },
            { "vout", item.out.index() },
            { "value", value },
            { "status", to_status(query.to_tx(item.out.hash())) }
        });
    }

    send_json(std::move(out), add1(unspent.size()) * 256);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
