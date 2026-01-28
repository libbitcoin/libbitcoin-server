/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#include "test.hpp"

BOOST_AUTO_TEST_SUITE(configuration_tests)

BOOST_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    const settings::embedded_pages web{};
    const settings::embedded_pages native{};
    const configuration instance(system::chain::selection::none, native, web);

    BOOST_REQUIRE(instance.file.empty());
    BOOST_REQUIRE(!instance.help);
    BOOST_REQUIRE(!instance.hardware);
    BOOST_REQUIRE(!instance.settings);
    BOOST_REQUIRE(!instance.version);
    BOOST_REQUIRE(!instance.newstore);
    BOOST_REQUIRE(!instance.backup);
    BOOST_REQUIRE(!instance.restore);
    BOOST_REQUIRE(!instance.flags);
    BOOST_REQUIRE(!instance.information);
    BOOST_REQUIRE(!instance.slabs);
    BOOST_REQUIRE(!instance.buckets);
    BOOST_REQUIRE(!instance.collisions);
    BOOST_REQUIRE_EQUAL(instance.test, system::null_hash);
    BOOST_REQUIRE_EQUAL(instance.write, system::null_hash);

    // Just a sample of settings.
    BOOST_REQUIRE(instance.node.headers_first);
    BOOST_REQUIRE_EQUAL(instance.network.threads, 1_u32);
    BOOST_REQUIRE_EQUAL(instance.bitcoin.first_version, 1_u32);
    BOOST_REQUIRE_EQUAL(instance.log.application, network::levels::application_defined);
}

BOOST_AUTO_TEST_SUITE_END()
