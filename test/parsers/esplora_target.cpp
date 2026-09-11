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

BOOST_AUTO_TEST_SUITE(esplora_target_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;

constexpr auto text = to_value(network::http::media_type::text_plain);
constexpr auto json = to_value(network::http::media_type::application_json);
constexpr auto data = to_value(network::http::media_type::application_octet_stream);

// General errors

BOOST_AUTO_TEST_CASE(parsers__esplora_target__empty_path__empty_path)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "?foo=bar"), server::error::empty_path);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__missing_target__missing_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/"), server::error::missing_target);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__invalid_target__invalid_target)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/invalid"), server::error::invalid_target);
}

// tx

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_valid__expected)
{
    const std::string path = "//tx//0000000000000000000000000000000000000000000000000000000000000042//?foo=bar";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx");
    BOOST_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BOOST_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_hex__text)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/hex";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_raw__data)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/raw";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), data);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_status__expected)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/status";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx_status");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_merkleblock_proof__text)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/merkleblock-proof";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx_merkleblock_proof");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_merkle_proof__expected)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/merkle-proof";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx_merkle_proof");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_outspend__expected)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/outspend/3";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx_outspend");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("index").value()), 3u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_outspend_missing_index__missing_position)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/tx/0000000000000000000000000000000000000000000000000000000000000042/outspend"), server::error::missing_position);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_outspend_invalid_index__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/tx/0000000000000000000000000000000000000000000000000000000000000042/outspend/x"), server::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_outspends__expected)
{
    const std::string path = "/tx/0000000000000000000000000000000000000000000000000000000000000042/outspends";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "tx_outspends");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/tx/invalid"), server::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/tx/0000000000000000000000000000000000000000000000000000000000000042/bogus"), server::error::invalid_component);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tx_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/tx/0000000000000000000000000000000000000000000000000000000000000042/hex/extra"), server::error::extra_segment);
}

// broadcast

BOOST_AUTO_TEST_CASE(parsers__esplora_target__broadcast__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/tx"));
    BOOST_REQUIRE_EQUAL(request.method, "broadcast");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__broadcast_package__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/txs/package"));
    BOOST_REQUIRE_EQUAL(request.method, "broadcast_package");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__txs_missing_component__missing_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/txs"), server::error::missing_component);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__txs_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/txs/bogus"), server::error::invalid_component);
}

// address

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/address/bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4"));
    BOOST_REQUIRE_EQUAL(request.method, "address");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
    BOOST_REQUIRE_EQUAL(std::get<std::string>(object.at("address").value()), "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_missing__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/address"), server::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_txs__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/address/1abc/txs"));
    BOOST_REQUIRE_EQUAL(request.method, "address_txs");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_txs_chain__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/address/1abc/txs/chain"));
    BOOST_REQUIRE_EQUAL(request.method, "address_txs_chain");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_txs_chain_last_seen__expected)
{
    const std::string path = "/address/1abc/txs/chain/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "address_txs_chain");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);

    const auto& any = std::get<any_t>(object.at("last_seen").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_txs_chain_invalid_last_seen__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/address/1abc/txs/chain/invalid"), server::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_txs_mempool__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/address/1abc/txs/mempool"));
    BOOST_REQUIRE_EQUAL(request.method, "address_txs_mempool");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_txs_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/address/1abc/txs/bogus"), server::error::invalid_subcomponent);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_utxo__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/address/1abc/utxo"));
    BOOST_REQUIRE_EQUAL(request.method, "address_utxo");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__address_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/address/1abc/bogus"), server::error::invalid_component);
}

// scripthash

BOOST_AUTO_TEST_CASE(parsers__esplora_target__scripthash_valid__expected)
{
    const std::string path = "/scripthash/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "address");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__scripthash_utxo__expected)
{
    const std::string path = "/scripthash/0000000000000000000000000000000000000000000000000000000000000042/utxo";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "address_utxo");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__scripthash_missing__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/scripthash"), server::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__scripthash_invalid__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/scripthash/invalid"), server::error::invalid_hash);
}

// block

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_valid__expected)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BOOST_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BOOST_REQUIRE(hash_cptr);
    BOOST_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_raw__data)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/raw";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), data);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_header__text)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/header";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_header");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_status__expected)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/status";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_status");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_txs__expected)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/txs";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_txs");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_txs_start__expected)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/txs/25";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_txs");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("start").value()), 25u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_txs_invalid_start__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block/0000000000000000000000000000000000000000000000000000000000000042/txs/x"), server::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_txids__expected)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/txids";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_txids");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_txid__expected)
{
    const std::string path = "/block/0000000000000000000000000000000000000000000000000000000000000042/txid/7";

    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, path));
    BOOST_REQUIRE_EQUAL(request.method, "block_txid");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 3u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("index").value()), 7u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_txid_missing_index__missing_position)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block/0000000000000000000000000000000000000000000000000000000000000042/txid"), server::error::missing_position);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_missing_hash__missing_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block"), server::error::missing_hash);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_invalid_hash__invalid_hash)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block/invalid"), server::error::invalid_hash);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block/0000000000000000000000000000000000000000000000000000000000000042/bogus"), server::error::invalid_component);
}

// block-height

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_height_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/block-height/123"));
    BOOST_REQUIRE_EQUAL(request.method, "block_height");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 123u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_height_zero__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/block-height/0"));

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 0u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_height_leading_zero__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block-height/0123"), server::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_height_missing__missing_height)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block-height"), server::error::missing_height);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__block_height_invalid__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/block-height/x"), server::error::invalid_number);
}

// blocks

BOOST_AUTO_TEST_CASE(parsers__esplora_target__blocks_valid__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/blocks"));
    BOOST_REQUIRE_EQUAL(request.method, "blocks");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__blocks_height__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/blocks/100"));
    BOOST_REQUIRE_EQUAL(request.method, "blocks");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 2u);
    BOOST_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 100u);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__blocks_invalid_height__invalid_number)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/blocks/x"), server::error::invalid_number);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tip_height__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/blocks/tip/height"));
    BOOST_REQUIRE_EQUAL(request.method, "tip_height");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tip_hash__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/blocks/tip/hash"));
    BOOST_REQUIRE_EQUAL(request.method, "tip_hash");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), text);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tip_missing__missing_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/blocks/tip"), server::error::missing_component);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__tip_invalid__invalid_subcomponent)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/blocks/tip/bogus"), server::error::invalid_subcomponent);
}

// mempool

BOOST_AUTO_TEST_CASE(parsers__esplora_target__mempool__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/mempool"));
    BOOST_REQUIRE_EQUAL(request.method, "mempool");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__mempool_txids__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/mempool/txids"));
    BOOST_REQUIRE_EQUAL(request.method, "mempool_txids");
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__mempool_recent__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/mempool/recent"));
    BOOST_REQUIRE_EQUAL(request.method, "mempool_recent");
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__mempool_invalid_component__invalid_component)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/mempool/bogus"), server::error::invalid_component);
}

// fee-estimates

BOOST_AUTO_TEST_CASE(parsers__esplora_target__fee_estimates__expected)
{
    request_t request{};
    BOOST_REQUIRE(!esplora_target(request, "/fee-estimates"));
    BOOST_REQUIRE_EQUAL(request.method, "fee_estimates");

    const auto& object = std::get<object_t>(request.params.value());
    BOOST_REQUIRE_EQUAL(object.size(), 1u);
    BOOST_REQUIRE_EQUAL(std::get<uint8_t>(object.at("media").value()), json);
}

BOOST_AUTO_TEST_CASE(parsers__esplora_target__fee_estimates_extra_segment__extra_segment)
{
    request_t out{};
    BOOST_REQUIRE_EQUAL(esplora_target(out, "/fee-estimates/extra"), server::error::extra_segment);
}

BOOST_AUTO_TEST_SUITE_END()
