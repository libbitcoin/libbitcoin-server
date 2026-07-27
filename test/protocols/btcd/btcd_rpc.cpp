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
#include "../../test.hpp"
#include "btcd_setup_fixture.hpp"

using namespace system;

namespace {

bool has_error(const boost::json::value& response) NOEXCEPT
{
    return response.is_object() && response.as_object().contains("error") &&
        !response.at("error").is_null();
}

bool has_result(const boost::json::value& response) NOEXCEPT
{
    return response.is_object() && response.as_object().contains("result") &&
        !response.at("result").is_null();
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(btcd_rpc_tests, btcd_ten_block_setup_fixture)

// session management
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__authenticate__no_auth_configured__null_result)
{
    // Without basic auth configured, authenticate is a no-op success.
    const auto response = rpc("authenticate", R"(["user","pass"])");
    BOOST_REQUIRE(!has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__session__returns_id)
{
    const auto response = rpc("session");
    BOOST_REQUIRE(!has_error(response));
    BOOST_REQUIRE(has_result(response));
    BOOST_REQUIRE(response.at("result").is_object());
    BOOST_REQUIRE(response.at("result").as_object().contains("id"));
}

// block subscription
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__notifyblocks__returns_null_result)
{
    const auto response = rpc("notifyblocks");
    BOOST_REQUIRE(!has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_rpc__stopnotifyblocks__returns_null_result)
{
    // Subscribe first so stop is meaningful.
    rpc("notifyblocks");
    const auto response = rpc("stopnotifyblocks");
    BOOST_REQUIRE(!has_error(response));
}

// Standard chain methods (getblockcount etc.) are inherited from
// protocol_bitcoind_rpc and remain reachable via plain http post on the same
// endpoint (see test/protocols/bitcoind), but are not yet bridged into this
// ws dispatcher -- see the phase B TODO on protocol_btcd_rpc.hpp.

// not implemented stubs
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__not_implemented__error)
{
    const std::vector<std::pair<std::string, std::string>> methods
    {
        { "stop",                      "[]" },
        { "notifynewtransactions",     "[false]" },
        { "stopnotifynewtransactions", "[]" },
        { "loadtxfilter",              R"([false,[],[]])" },
        { "rescanblocks",              "[[]]" },
        { "notifyreceived",            "[[]]" },
        { "stopnotifyreceived",        "[[]]" },
        { "notifyspent",               "[[]]" },
        { "stopnotifyspent",           "[[]]" },
        { "rescan",                    R"(["",[""],[""],""])" }
    };

    for (const auto& method: methods)
        BOOST_REQUIRE_MESSAGE(has_error(rpc(method.first, method.second)),
            method.first);
}

// response envelope
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(btcd_rpc__response__id_matches_request)
{
    // request_id_ increments each call: first call uses id=0, second id=1.
    const auto r0 = rpc("session");
    const auto r1 = rpc("notifyblocks");
    BOOST_REQUIRE_EQUAL(r0.at("id").as_int64(), 0);
    BOOST_REQUIRE_EQUAL(r1.at("id").as_int64(), 1);
}

BOOST_AUTO_TEST_CASE(btcd_rpc__unknown_method__error_keeps_connection_alive)
{
    // Unrecognized method returns a json-rpc error rather than dropping the
    // ws connection (matches real btcd, verified by the follow-up call).
    const auto unknown = rpc("nosuchmethod");
    BOOST_REQUIRE(has_error(unknown));

    const auto follow_up = rpc("session");
    BOOST_REQUIRE(!has_error(follow_up));
}

BOOST_AUTO_TEST_SUITE_END()
