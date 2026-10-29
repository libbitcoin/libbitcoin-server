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

BOOST_AUTO_TEST_SUITE(bitcoind_descriptor_tests)

static const std::string compressed_key{ "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd" };
static const std::string uncompressed_key{ "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235" };
static const std::string bip67_key1{ "02fe6f0a5a297eb38c391581c4413e084773ea23954d93f7753db7dc0adc188b2f" };
static const std::string bip67_key2{ "02ff12471208c14bd580709cb2358d98975247d8765f92bc25eab3b2763ed605f8" };
static const std::string bip67_script{ "522102fe6f0a5a297eb38c391581c4413e084773ea23954d93f7753db7dc0adc188b2f2102ff12471208c14bd580709cb2358d98975247d8765f92bc25eab3b2763ed605f852ae" };

BOOST_AUTO_TEST_CASE(bitcoind_descriptor__infer_descriptor__bare_multisig__multi)
{
    system::data_chunk data{};
    BOOST_REQUIRE(system::decode_base16(data, "5121" + compressed_key + "41" + uncompressed_key + "52ae"));
    const system::chain::script script{ data, false };
    const auto body = "multi(1," + compressed_key + "," + uncompressed_key + ")";
    BOOST_REQUIRE_EQUAL(server::infer_descriptor(script, 0x00, 0x05, "bc"), body + "#" + system::wallet::descriptor::to_checksum(body));
}

BOOST_AUTO_TEST_CASE(bitcoind_descriptor__create_multisig__p2sh_segwit__nested_descriptor)
{
    const network::rpc::array_t keys{ bip67_key1, bip67_key2 };
    const auto result = server::create_multisig(2, keys, "p2sh-segwit", 0x05, "bc");
    const auto body = "sh(wsh(multi(2," + bip67_key1 + "," + bip67_key2 + ")))";
    BOOST_REQUIRE_EQUAL(std::get<network::rpc::string_t>(result.at("redeemScript").value()), bip67_script);
    BOOST_REQUIRE_EQUAL(std::get<network::rpc::string_t>(result.at("descriptor").value()), body + "#" + system::wallet::descriptor::to_checksum(body));
    BOOST_REQUIRE(std::get<network::rpc::string_t>(result.at("address").value()).starts_with('3'));
    BOOST_REQUIRE(!result.contains("warnings"));
}

BOOST_AUTO_TEST_CASE(bitcoind_descriptor__create_multisig__invalid_key__empty)
{
    const network::rpc::array_t keys{ bip67_key1, std::string{ "nothex" } };
    BOOST_REQUIRE(server::create_multisig(1, keys, "legacy", 0x05, "bc").empty());
}

BOOST_AUTO_TEST_CASE(bitcoind_descriptor__create_multisig__legacy_oversized__empty)
{
    const network::rpc::array_t keys{ uncompressed_key, uncompressed_key, uncompressed_key, uncompressed_key, uncompressed_key, uncompressed_key, uncompressed_key, uncompressed_key };
    BOOST_REQUIRE(server::create_multisig(1, keys, "legacy", 0x05, "bc").empty());
}

BOOST_AUTO_TEST_SUITE_END()
