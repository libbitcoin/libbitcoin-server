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
#include "test.hpp"

BC_AUTO_TEST_SUITE(configuration_tests)

BC_AUTO_TEST_CASE(configuration__construct1__none_context__expected)
{
    const settings::embedded_pages admin{};
    const settings::embedded_pages native{};
    const configuration instance(system::chain::selection::none, native, admin);

    BC_REQUIRE(instance.file.empty());
    BC_REQUIRE(instance.root.empty());
    BC_REQUIRE(!instance.help);
    BC_REQUIRE(!instance.hardware);
    BC_REQUIRE(!instance.settings);
    BC_REQUIRE(!instance.version);
    BC_REQUIRE(instance.net == system::chain::selection::none);
    BC_REQUIRE(!instance.make);
    BC_REQUIRE(!instance.backup);
    BC_REQUIRE(!instance.restore);
    BC_REQUIRE(!instance.daemon);
    BC_REQUIRE(!instance.user);
    BC_REQUIRE(!instance.flags);
    BC_REQUIRE(!instance.information);
    BC_REQUIRE(!instance.buckets);
    BC_REQUIRE(!instance.collisions);
    BC_REQUIRE_EQUAL(instance.get, system::null_hash);
    BC_REQUIRE_EQUAL(instance.put, system::null_hash);

    // Just a sample of settings.
    BC_REQUIRE_EQUAL(instance.network.threads, 0u);
    BC_REQUIRE_EQUAL(instance.bitcoin.first_version, 1u);
    BC_REQUIRE_EQUAL(instance.log.application, network::levels::application_defined);
}

BC_AUTO_TEST_SUITE_END()
