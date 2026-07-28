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

// Method-scoped credentials (channel_btcd::permitted()): a credential's
// optional 'user:pass:method,...' suffix restricts which methods it may
// call, enforced over ws by protocol_btcd_rpc::dispatch_websocket.
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_scoped_credential_tests,
    btcd_scoped_credential_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_scoped_credential__listed_method__permitted)
{
    const auto auth = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", ")" BTCD_TEST_PASSWORD R"("])");
    BOOST_REQUIRE(!has_error(auth));

    // BTCD_TEST_SCOPED_METHOD ("session") is the credential's only permitted
    // method.
    const auto session = rpc("session");
    BOOST_REQUIRE(!has_error(session));
}

BOOST_AUTO_TEST_CASE(btcd_scoped_credential__unlisted_method__rejected)
{
    const auto auth = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", ")" BTCD_TEST_PASSWORD R"("])");
    BOOST_REQUIRE(!has_error(auth));

    // notifyblocks is a real, implemented handler, not scoped by this
    // credential -- confirms rejection is permitted()'s doing, not the
    // method being unimplemented.
    const auto response = rpc("notifyblocks");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_SUITE_END()

// authenticate (protocol_btcd_rpc::handle_authenticate): exercises the
// credential-configured branch, not covered by btcd_rpc_tests above (whose
// fixture leaves btcd.credential unset, so every one of those connections
// takes the no-op path).
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(btcd_auth_tests, btcd_credentialed_setup_fixture)

BOOST_AUTO_TEST_CASE(btcd_auth__correct_credentials__succeeds_then_session_works)
{
    const auto auth = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", ")" BTCD_TEST_PASSWORD R"("])");
    BOOST_REQUIRE(!has_error(auth));

    // Connection survives a successful handshake; other methods now work.
    const auto session = rpc("session");
    BOOST_REQUIRE(!has_error(session));
}

BOOST_AUTO_TEST_CASE(btcd_auth__wrong_password__rejected)
{
    const auto response = rpc("authenticate",
        R"([")" BTCD_TEST_USERNAME R"(", "definitely-wrong"])");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_CASE(btcd_auth__other_method_before_authenticating__rejected)
{
    // 'session' instead of 'authenticate' as the first message: the
    // handshake requires authenticate specifically when a credential is
    // configured.
    const auto response = rpc("session");
    BOOST_REQUIRE(has_error(response));
}

BOOST_AUTO_TEST_SUITE_END()
