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
#include "../test.hpp"

using namespace interface;

// setup
// -----------------------------------------------------------------------------

// Keyed by name, so that assertions are independent of interface order. A
// misspelled name matches neither, failing both btcd_served and btcd_unserved.
constexpr bool btcd_declared(const std::string_view& name, bool implemented) NOEXCEPT
{
    auto result = false;
    std::apply([&](const auto&... items) NOEXCEPT
    {
        ((result = result || (items.name == name &&
            items.implemented() == implemented)), ...);
    }, btcd_methods::methods);

    return result;
}

constexpr bool btcd_served(const std::string_view& name) NOEXCEPT
{
    return btcd_declared(name, true);
}

constexpr bool btcd_unserved(const std::string_view& name) NOEXCEPT
{
    return btcd_declared(name, false);
}

// btcd_methods::implemented
// -----------------------------------------------------------------------------

// These are dispatchable but answer not_implemented (see protocol_btcd).
static_assert(btcd_unserved("stop"));
static_assert(btcd_unserved("notifynewtransactions"));
static_assert(btcd_unserved("stopnotifynewtransactions"));
static_assert(btcd_unserved("notifyreceived"));
static_assert(btcd_unserved("stopnotifyreceived"));
static_assert(btcd_unserved("notifyspent"));
static_assert(btcd_unserved("stopnotifyspent"));

// rescan is served for the empty addresses/outpoints form (btcwallet sync
// bootstrap), so it is published despite its group.
static_assert(btcd_served("rescan"));

// A name absent from the interface satisfies neither.
static_assert(!btcd_served("getblockcount") && !btcd_unserved("getblockcount"));

// btcd_methods::names
// -----------------------------------------------------------------------------

// The published list is the served subset, in interface order.
static_assert(btcd_methods::names ==
    "authenticate help session "
    "getbestblock getcurrentnet getdifficulty getinfo getnettotals "
    "getnetworkhashps "
    "createrawtransaction decoderawtransaction decodescript validateaddress "
    "notifyblocks stopnotifyblocks "
    "loadtxfilter rescanblocks "
    "rescan");
