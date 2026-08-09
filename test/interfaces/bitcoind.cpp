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
template <typename Methods>
constexpr bool declared(const std::string_view& name,
    bool implemented) NOEXCEPT
{
    auto result = false;
    std::apply([&](const auto&... items) NOEXCEPT
    {
        ((result = result || (items.name == name &&
            items.implemented() == implemented)), ...);
    }, Methods::methods);

    return result;
}

// The bitcoind interface is the union of the subgroup interfaces.
constexpr bool bitcoind_declared(const std::string_view& name,
    bool implemented) NOEXCEPT
{
    return
        declared<bitcoind_blockchain_methods>(name, implemented) ||
        declared<bitcoind_control_methods>(name, implemented) ||
        declared<bitcoind_mining_methods>(name, implemented) ||
        declared<bitcoind_network_methods>(name, implemented) ||
        declared<bitcoind_notifications_methods>(name, implemented) ||
        declared<bitcoind_test_methods>(name, implemented) ||
        declared<bitcoind_transaction_methods>(name, implemented) ||
        declared<bitcoind_utility_methods>(name, implemented) ||
        declared<bitcoind_wallet_methods>(name, implemented);
}

constexpr bool bitcoind_served(const std::string_view& name) NOEXCEPT
{
    return bitcoind_declared(name, true);
}

constexpr bool bitcoind_unserved(const std::string_view& name) NOEXCEPT
{
    return bitcoind_declared(name, false);
}

// implemented
// -----------------------------------------------------------------------------

// These are dispatchable but answer not_implemented (see protocol).
static_assert(bitcoind_unserved("getblockstats"));
static_assert(bitcoind_served("getchaintxstats"));
static_assert(bitcoind_unserved("gettxoutsetinfo"));
static_assert(bitcoind_unserved("pruneblockchain"));
static_assert(bitcoind_unserved("savemempool"));
static_assert(bitcoind_unserved("scantxoutset"));

// no-op that returns true (store is reliable, see protocol).
static_assert(bitcoind_served("verifychain"));

// Implemented from the wip backlog.
static_assert(bitcoind_served("getdeploymentinfo"));
static_assert(bitcoind_served("getchaintips"));
static_assert(bitcoind_served("getdifficulty"));
static_assert(bitcoind_served("verifymessage"));
static_assert(bitcoind_served("getindexinfo"));

// Moved from the btcd interface (btcd serves them by session attachment).
static_assert(bitcoind_served("help"));
static_assert(bitcoind_served("getnetworkhashps"));
static_assert(bitcoind_served("createrawtransaction"));
static_assert(bitcoind_served("decoderawtransaction"));
static_assert(bitcoind_served("decodescript"));
static_assert(bitcoind_served("testmempoolaccept"));
static_assert(bitcoind_served("validateaddress"));

// A name absent from the interface satisfies neither.
static_assert(!bitcoind_served("getcurrentnet") && !bitcoind_unserved("getcurrentnet"));

// A name is declared by exactly one subgroup interface.
static_assert(declared<bitcoind_blockchain_methods>("getblockcount", true));
static_assert(!declared<bitcoind_utility_methods>("getblockcount", true));
static_assert(declared<bitcoind_utility_methods>("getindexinfo", true));
static_assert(!declared<bitcoind_blockchain_methods>("getindexinfo", true));

// names
// -----------------------------------------------------------------------------

// The published lists are the served subsets, in interface order. Subgroups
// with only unimplemented methods publish no names.
static_assert(bitcoind_blockchain_methods::names ==
    "getbestblockhash getblock getblockchaininfo getblockcount "
    "getblockfilter getblockhash getblockheader getchaintxstats gettxout "
    "verifychain gettxoutproof verifytxoutproof getchaintips "
    "getdeploymentinfo getdifficulty");
static_assert(bitcoind_control_methods::names ==
    "help getmemoryinfo getrpcinfo logging uptime");
static_assert(bitcoind_mining_methods::names == "getnetworkhashps");
static_assert(bitcoind_network_methods::names ==
    "getnetworkinfo getconnectioncount getnettotals");
static_assert(bitcoind_notifications_methods::names == "");
static_assert(bitcoind_test_methods::names == "");
static_assert(bitcoind_transaction_methods::names ==
    "createrawtransaction decoderawtransaction getrawtransaction "
    "sendrawtransaction testmempoolaccept");
static_assert(bitcoind_utility_methods::names ==
    "decodescript validateaddress verifymessage getindexinfo");
static_assert(bitcoind_wallet_methods::names == "");
