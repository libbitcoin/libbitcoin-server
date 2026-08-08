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
// misspelled name matches neither, failing both served and unserved.
constexpr bool bitcoind_declared(const std::string_view& name,
    bool implemented) NOEXCEPT
{
    auto result = false;
    std::apply([&](const auto&... items) NOEXCEPT
    {
        ((result = result || (items.name == name &&
            items.implemented() == implemented)), ...);
    }, bitcoind_rpc_methods::methods);

    return result;
}

constexpr bool bitcoind_served(const std::string_view& name) NOEXCEPT
{
    return bitcoind_declared(name, true);
}

constexpr bool bitcoind_unserved(const std::string_view& name) NOEXCEPT
{
    return bitcoind_declared(name, false);
}

// bitcoind_rpc_methods::implemented
// -----------------------------------------------------------------------------

// These are dispatchable but answer not_implemented (see protocol).
static_assert(bitcoind_unserved("getblockstats"));
static_assert(bitcoind_unserved("getchaintxstats"));
static_assert(bitcoind_unserved("getchainwork"));
static_assert(bitcoind_unserved("gettxoutsetinfo"));
static_assert(bitcoind_unserved("pruneblockchain"));
static_assert(bitcoind_unserved("savemempool"));
static_assert(bitcoind_unserved("scantxoutset"));
static_assert(bitcoind_unserved("verifychain"));
static_assert(bitcoind_unserved("verifytxoutset"));

// Moved from the btcd interface (btcd serves them by derivation).
static_assert(bitcoind_served("help"));
static_assert(bitcoind_served("getnetworkhashps"));
static_assert(bitcoind_served("createrawtransaction"));
static_assert(bitcoind_served("decoderawtransaction"));
static_assert(bitcoind_served("decodescript"));
static_assert(bitcoind_served("testmempoolaccept"));
static_assert(bitcoind_served("testrawtransaction"));
static_assert(bitcoind_served("validateaddress"));

// A name absent from the interface satisfies neither.
static_assert(!bitcoind_served("getcurrentnet") && !bitcoind_unserved("getcurrentnet"));

// bitcoind_rpc_methods::names
// -----------------------------------------------------------------------------

// The published list is the served subset, in interface order.
static_assert(bitcoind_rpc_methods::names ==
    "getbestblockhash getblock getblockchaininfo getblockcount "
    "getblockfilter getblockhash getblockheader gettxout "
    "help getnetworkhashps getnetworkinfo "
    "createrawtransaction decoderawtransaction getrawtransaction "
    "sendrawtransaction testmempoolaccept testrawtransaction "
    "decodescript validateaddress");
