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
#include "stratum_v2_setup_fixture.hpp"

using namespace system;
using namespace network::messages::sv2;

// setup_connection_success{ 2, 0 } and a set_target of channel one.
static data_chunk success_frame()
{
    return base16_chunk("000001060000020000000000");
}

static data_chunk target_frame()
{
    return stratum_v2_setup_fixture::to_frame(set_target{ 1, null_hash });
}

BC_FIXTURE_TEST_SUITE(stratum_v2_tests, stratum_v2_ten_block_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__version_two__success)
{
    const auto ec = upgrade();
    send(setup_frame());
    const auto response = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(response, success_frame());
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__version_range_includes_two__success)
{
    const auto ec = upgrade();
    send(setup_frame(1, 3));
    const auto response = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(response, success_frame());
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__requires_version_rolling__success)
{
    const auto ec = upgrade();
    send(setup_frame(2, 2, 0x00000004));
    const auto response = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(response, success_frame());
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__job_declaration_protocol__unsupported_protocol)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(2, 2, 0, 1));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->flags, 0u);
    BC_REQUIRE_EQUAL(error->error_code, "unsupported-protocol");
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__version_above_two__protocol_version_mismatch)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(3, 3));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->flags, 0u);
    BC_REQUIRE_EQUAL(error->error_code, "protocol-version-mismatch");
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__version_below_two__protocol_version_mismatch)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(1, 1));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "protocol-version-mismatch");
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__requires_work_selection__unsupported_feature_flags)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(2, 2, 0x00000002));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->flags, 0x00000002u);
    BC_REQUIRE_EQUAL(error->error_code, "unsupported-feature-flags");
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__requires_standard_jobs__unsupported_feature_flags)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(2, 2, 0x00000001));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->flags, 0x00000001u);
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__all_flags__all_unsupported_reported)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(2, 2, 0xffffffff));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->flags, 0xfffffffbu);
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__refused__dropped)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(3, 3));
    const auto dropped = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__repeated__dropped)
{
    const auto ec = upgrade();
    send(setup_frame());
    const auto response = receive();
    send(setup_frame());
    const auto dropped = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(response, success_frame());
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__set_target__before_setup__dropped)
{
    const auto ec = upgrade();
    send(target_frame());
    const auto dropped = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__frame__length_above_message__dropped)
{
    const auto ec = upgrade();
    send(base16_chunk("0000010600000200000000"));
    const auto dropped = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__get__without_upgrade__upgrade_required)
{
    BC_REQUIRE(get_status() == network::http::status::upgrade_required);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_credential_tests, stratum_v2_credential_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__upgrade__without_credential__refused)
{
    BC_REQUIRE(upgrade());
}

BC_AUTO_TEST_CASE(stratum_v2__upgrade__wrong_credential__refused)
{
    BC_REQUIRE(upgrade("miner", "wrong"));
}

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__credential__success)
{
    const auto ec = upgrade("miner", "pass");
    send(setup_frame());
    const auto response = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(response, success_frame());
}

BC_AUTO_TEST_SUITE_END()
