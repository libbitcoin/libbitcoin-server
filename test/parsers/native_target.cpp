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

BC_AUTO_TEST_SUITE(native_target_tests)

using namespace system;
using namespace network::rpc;
using object_t = network::rpc::object_t;

// General errors

BC_AUTO_TEST_CASE(parsers__native_target__empty_path__empty_path)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "?foo=bar"), server::error::empty_path);
}

BC_AUTO_TEST_CASE(parsers__native_target__missing_version__missing_version)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/"), server::error::missing_version);
    BC_REQUIRE_EQUAL(native_target(out, "/block/confirmed/123"), server::error::missing_version);
}

BC_AUTO_TEST_CASE(parsers__native_target__invalid_version__missing_version)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/vinvalid/block/confirmed/123"), server::error::missing_version);
}

BC_AUTO_TEST_CASE(parsers__native_target__version_leading_zero__missing_version)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v01/block/confirmed/123"), server::error::missing_version);
}

BC_AUTO_TEST_CASE(parsers__native_target__missing_target__missing_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3"), server::error::missing_target);
}

BC_AUTO_TEST_CASE(parsers__native_target__invalid_target__invalid_target)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/invalid"), server::error::invalid_target);
}

BC_AUTO_TEST_CASE(parsers__native_target__top_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/top/invalid"), server::error::invalid_subcomponent);
}

BC_AUTO_TEST_CASE(parsers__native_target__input_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/3/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_missing_id_type__missing_id_type)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block"), server::error::missing_id_type);
}

// configuration

BC_AUTO_TEST_CASE(parsers__native_target__configuration_valid__expected)
{
    const std::string path = "/v42/configuration";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "configuration");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__configuration_extra_segment__extra_segment)
{
    const std::string path = "/v3/configuration/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// top

BC_AUTO_TEST_CASE(parsers__native_target__top_valid__expected)
{
    const std::string path = "/v42/top";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "top");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

// top/subscribe

BC_AUTO_TEST_CASE(parsers__native_target__top_subscribe_valid__expected)
{
    const std::string path = "/v42/top/subscribe";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "top_subscribe");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__top_subscribe_extra_segment__extra_segment)
{
    const std::string path = "/v3/top/subscribe/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// top/confirmed, top/candidate

BC_AUTO_TEST_CASE(parsers__native_target__top_confirmed_valid__expected)
{
    const std::string path = "/v42/top/confirmed";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "top_chain");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__top_candidate_valid__expected)
{
    const std::string path = "/v42/top/candidate";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "top_chain");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
    BC_REQUIRE(std::get<bool>(object.at("candidate").value()));
}

BC_AUTO_TEST_CASE(parsers__native_target__top_confirmed_height_valid__expected)
{
    const std::string path = "/v42/top/confirmed/height";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "top_height");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__top_candidate_height_valid__expected)
{
    const std::string path = "/v42/top/candidate/height";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "top_height");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
    BC_REQUIRE(std::get<bool>(object.at("candidate").value()));
}

BC_AUTO_TEST_CASE(parsers__native_target__top_confirmed_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/top/confirmed/invalid"), server::error::invalid_subcomponent);
}

BC_AUTO_TEST_CASE(parsers__native_target__top_candidate_height_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/top/candidate/height/extra"), server::error::extra_segment);
}

// block/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_confirmed_valid__expected)
{
    const std::string path = "/v42/block/confirmed/123456";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_confirmed_missing_height__missing_height)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed"), server::error::missing_height);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_confirmed_invalid_height__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/invalid"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_confirmed_invalid_component__invalid_component)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/invalid"), server::error::invalid_component);
}

// block/candidate

BC_AUTO_TEST_CASE(parsers__native_target__block_candidate_valid__expected)
{
    const std::string path = "/v42/block/candidate/123456";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
    BC_REQUIRE(std::get<bool>(object.at("candidate").value()));
}

BC_AUTO_TEST_CASE(parsers__native_target__block_candidate_header_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/candidate/123456/header"));
    BC_REQUIRE_EQUAL(request.method, "block_header");

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);
    BC_REQUIRE(std::get<bool>(object.at("candidate").value()));
}

BC_AUTO_TEST_CASE(parsers__native_target__block_candidate_missing_height__missing_height)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/candidate"), server::error::missing_height);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_candidate_invalid_height__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/candidate/invalid"), server::error::invalid_number);
}

// block/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_hash_valid__expected)
{
    const std::string path = "//v42//block//hash//0000000000000000000000000000000000000000000000000000000000000042//?foo=bar";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_hash_missing_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_hash_invalid_hash__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/invalidhex"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_hash_invalid_component__invalid_component)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_component);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_invalid_id_type__invalid_id_type)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/invalid/123"), server::error::invalid_id_type);
}

// block_header/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_header_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/header/"));
    BC_REQUIRE_EQUAL(request.method, "block_header");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_header_confirmed_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/header/invalid"), server::error::invalid_subcomponent);
}

// block_header/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_header_hash_valid__expected)
{
    const std::string path = "v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/header";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_header");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_header_hash_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/header/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

// block_header_context/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_header_context_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/header/context"));
    BC_REQUIRE_EQUAL(request.method, "block_header_context");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_header_context_confirmed_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/header/context/extra"), server::error::extra_segment);
}

// block_header_context/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_header_context_hash_valid__expected)
{
    const std::string path = "v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/header/context";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_header_context");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_header_context_hash_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/header/context/extra"), server::error::extra_segment);
}

// block_txs/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/txs"));
    BC_REQUIRE_EQUAL(request.method, "block_txs");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_confirmed_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/txs/invalid"), server::error::invalid_subcomponent);
}

// block_txs/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/txs";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_txs");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_hash_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/txs/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

// block_txs_details/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_details_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/txs/details"));
    BC_REQUIRE_EQUAL(request.method, "block_txs_details");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_details_confirmed_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/txs/details/extra"), server::error::extra_segment);
}

// block_txs_details/candidate

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_details_candidate_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/candidate/123456/txs/details"));
    BC_REQUIRE_EQUAL(request.method, "block_txs_details");

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
    BC_REQUIRE(std::get<bool>(object.at("candidate").value()));
}

// block_txs_details/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_details_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/txs/details";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_txs_details");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_txs_details_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/txs/details/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// block_tx/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/tx/7"));
    BC_REQUIRE_EQUAL(request.method, "block_tx");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);

    const auto position = std::get<uint32_t>(object.at("position").value());
    BC_REQUIRE_EQUAL(position, 7u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_confirmed_missing_position__missing_position)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx"), server::error::missing_position);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_confirmed_invalid_position__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/invalid"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_confirmed_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/extra"), server::error::invalid_subcomponent);
}

// block_tx/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/tx/7";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_tx");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto position = std::get<uint32_t>(object.at("position").value());
    BC_REQUIRE_EQUAL(position, 7u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_hash_missing_position__missing_position)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/tx"), server::error::missing_position);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_hash_invalid_position__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/tx/invalid"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_hash_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/tx/7/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

// block_tx_input_script, block_tx_output_script

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_input_script_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/tx/7/input/3/script"));
    BC_REQUIRE_EQUAL(request.method, "block_tx_input_script");
    BC_REQUIRE(request.params.has_value());

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 4u);
    BC_REQUIRE_EQUAL(std::get<uint8_t>(object.at("version").value()), 42u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 123456u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("position").value()), 7u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("index").value()), 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_output_script_candidate_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/candidate/123456/tx/7/output/3/script"));
    BC_REQUIRE_EQUAL(request.method, "block_tx_output_script");
    BC_REQUIRE(request.params.has_value());

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 5u);
    BC_REQUIRE_EQUAL(std::get<uint8_t>(object.at("version").value()), 42u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("height").value()), 123456u);
    BC_REQUIRE(std::get<bool>(object.at("candidate").value()));
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("position").value()), 7u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("index").value()), 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_output_script_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/tx/7/output/3/script";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_tx_output_script");
    BC_REQUIRE(request.params.has_value());

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 4u);
    BC_REQUIRE_EQUAL(std::get<uint8_t>(object.at("version").value()), 42u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("position").value()), 7u);
    BC_REQUIRE_EQUAL(std::get<uint32_t>(object.at("index").value()), 3u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_input_script_hash_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000000/tx/0/input/0/script"));
    BC_REQUIRE_EQUAL(request.method, "block_tx_input_script");
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_input_script_missing_index__missing_position)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/input"), server::error::missing_position);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_output_script_invalid_index__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/output/invalid/script"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_input_script_missing_script__missing_component)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/input/3"), server::error::missing_component);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_input_witness__invalid_component)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/input/3/witness"), server::error::invalid_component);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_output_spender__invalid_component)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/output/3/spender"), server::error::invalid_component);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_tx_output_script_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/tx/7/output/3/script/extra"), server::error::extra_segment);
}

// block/subscribe

BC_AUTO_TEST_CASE(parsers__native_target__block_subscribe_valid__expected)
{
    const std::string path = "/v42/block/subscribe";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_subscribe");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_subscribe_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/subscribe/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// tx

BC_AUTO_TEST_CASE(parsers__native_target__tx_valid__expected)
{
    const std::string path = "/v42/tx/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "tx");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__tx_missing_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/tx"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__transaction_invalid_hash__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/tx/invalidhex"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__tx_invalid_component__invalid_component)
{
    const std::string path = "/v3/tx/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_component);
}

// tx_header

BC_AUTO_TEST_CASE(parsers__native_target__tx_header_valid__expected)
{
    const std::string path = "/v42/tx/0000000000000000000000000000000000000000000000000000000000000042/header";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "tx_header");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__tx_header_invalid_component__invalid_component)
{
    const std::string path = "/v3/tx/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_component);
}

BC_AUTO_TEST_CASE(parsers__native_target__tx_header_extra_segment__extra_segment)
{
    const std::string path = "/v3/tx/0000000000000000000000000000000000000000000000000000000000000000/header/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// tx/subscribe

BC_AUTO_TEST_CASE(parsers__native_target__tx_subscribe_valid__expected)
{
    const std::string path = "/v42/tx/subscribe";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "tx_subscribe");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__tx_subscribe_extra_segment__extra_segment)
{
    const std::string path = "/v3/tx/subscribe/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// inputs

BC_AUTO_TEST_CASE(parsers__native_target__inputs_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "inputs");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__inputs_missing_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/input"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__inputs_invalid_hash__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/input/invalidhex"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__inputs_invalid_number__invalid_number)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_number);
}

// input

BC_AUTO_TEST_CASE(parsers__native_target__input_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/3";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "input");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__input_missing_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/input"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__input_invalid_hash__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/input/invalidhex/3"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__input_invalid_number__invalid_number)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_number);
}

// input_script

BC_AUTO_TEST_CASE(parsers__native_target__input_script_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/3/script";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "input_script");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__input_script_extra_segment__extra_segment)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/3/script/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// input_witness

BC_AUTO_TEST_CASE(parsers__native_target__input_witness_valid__expected)
{
    const std::string path = "/v255/input/0000000000000000000000000000000000000000000000000000000000000042/3/witness";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "input_witness");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__input_witness_extra_segment__extra_segment)
{
    const std::string path = "/v3/input/0000000000000000000000000000000000000000000000000000000000000000/3/witness/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// outputs

BC_AUTO_TEST_CASE(parsers__native_target__outputs_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "outputs");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__outputs_missing_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/output"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__outputs_invalid_hash__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/output/invalidhex"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__outputs_invalid_number__invalid_number)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_number);
}

// output

BC_AUTO_TEST_CASE(parsers__native_target__output_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/3";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "output");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__output_invalid_number__invalid_number)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_number);
}

// output_script

BC_AUTO_TEST_CASE(parsers__native_target__output_script_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/3/script";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "output_script");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__output_script_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/3/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

BC_AUTO_TEST_CASE(parsers__native_target__output_script_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/3/script/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// output_spender

BC_AUTO_TEST_CASE(parsers__native_target__output_spender_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/3/spender";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "output_spender");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__output_spender_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/3/spender/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// output_spenders

BC_AUTO_TEST_CASE(parsers__native_target__output_spenders_valid__expected)
{
    const std::string path = "/v255/output/0000000000000000000000000000000000000000000000000000000000000042/1/spenders";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "output_spenders");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 1u);

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__output_spenders_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000000/1/spenders/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// output_subscribe

BC_AUTO_TEST_CASE(parsers__native_target__output_subscribe_valid__expected)
{
    const std::string path = "/v42/output/0000000000000000000000000000000000000000000000000000000000000042/3/subscribe";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "output_subscribe");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto index = std::get<uint32_t>(object.at("index").value());
    BC_REQUIRE_EQUAL(index, 3u);
}

BC_AUTO_TEST_CASE(parsers__native_target__output_subscribe_extra_segment__extra_segment)
{
    const std::string path = "/v3/output/0000000000000000000000000000000000000000000000000000000000000042/3/subscribe/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// address

BC_AUTO_TEST_CASE(parsers__native_target__address_valid__reversed_expected)
{
    const std::string path = "/v255/address/0000000000000000000000000000000000000000000000000000000000000042";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "address");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 255u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__address_missing_hash__missing_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/address"), server::error::missing_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__address_invalid_hash__invalid_hash)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/address/invalidhex"), server::error::invalid_hash);
}

BC_AUTO_TEST_CASE(parsers__native_target__address_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/address/0000000000000000000000000000000000000000000000000000000000000000/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

// TODO:
// address/confirmed
// address/unconfirmed
// address/balance

// address/subscribe

BC_AUTO_TEST_CASE(parsers__native_target__address_subscribe_valid__expected)
{
    const std::string path = "/v42/address/0000000000000000000000000000000000000000000000000000000000000042/subscribe";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "address_subscribe");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__address_subscribe_extra_segment__extra_segment)
{
    const std::string path = "/v3/address/0000000000000000000000000000000000000000000000000000000000000042/subscribe/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// block_filter/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "v42/block/confirmed/123456/filter/255"));
    BC_REQUIRE_EQUAL(request.method, "block_filter");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);

    const auto type = std::get<uint8_t>(object.at("type").value());
    BC_REQUIRE_EQUAL(type, 255u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_confirmed_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/filter/42/invalid"), server::error::invalid_subcomponent);
}

// block_filter/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/filter/255";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_filter");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto type = std::get<uint8_t>(object.at("type").value());
    BC_REQUIRE_EQUAL(type, 255u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_hash_invalid_subcomponent__invalid_subcomponent)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/invalid";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::invalid_subcomponent);
}

// block_filter_hash/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_hash_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/filter/255/hash"));
    BC_REQUIRE_EQUAL(request.method, "block_filter_hash");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);

    const auto type = std::get<uint8_t>(object.at("type").value());
    BC_REQUIRE_EQUAL(type, 255u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_hash_confirmed_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/filter/42/hash/extra"), server::error::extra_segment);
}

// block_filter_hash/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_hash_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/filter/255/hash";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_filter_hash");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto type = std::get<uint8_t>(object.at("type").value());
    BC_REQUIRE_EQUAL(type, 255u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_hash_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/hash/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// block_filter_header/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_header_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/filter/255/header"));
    BC_REQUIRE_EQUAL(request.method, "block_filter_header");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);

    const auto type = std::get<uint8_t>(object.at("type").value());
    BC_REQUIRE_EQUAL(type, 255u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_header_confirmed_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/filter/42/header/extra"), server::error::extra_segment);
}

// block_filter_header/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_header_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/filter/255/header";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_filter_header");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 3u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });

    const auto type = std::get<uint8_t>(object.at("type").value());
    BC_REQUIRE_EQUAL(type, 255u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_header_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/header/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_missing_type_id__missing_type_id)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/filter"), server::error::missing_type_id);
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter"), server::error::missing_type_id);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_invalid_type__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/filter/invalid"), server::error::invalid_number);
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/invalid"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_filter_invalid_subcomponent__invalid_subcomponent)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/filter/42/invalid"), server::error::invalid_subcomponent);
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/filter/42/invalid"), server::error::invalid_subcomponent);
}

// tx_details

BC_AUTO_TEST_CASE(parsers__native_target__tx_details_valid__expected)
{
    const std::string path = "/v42/tx/0000000000000000000000000000000000000000000000000000000000000042/details";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "tx_details");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__tx_details_extra_segment__extra_segment)
{
    const std::string path = "/v3/tx/0000000000000000000000000000000000000000000000000000000000000000/details/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// block_details/confirmed

BC_AUTO_TEST_CASE(parsers__native_target__block_details_confirmed_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/block/confirmed/123456/details"));
    BC_REQUIRE_EQUAL(request.method, "block_details");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__block_details_confirmed_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/block/confirmed/123/details/extra"), server::error::extra_segment);
}

// block_details/hash

BC_AUTO_TEST_CASE(parsers__native_target__block_details_hash_valid__expected)
{
    const std::string path = "/v42/block/hash/0000000000000000000000000000000000000000000000000000000000000042/details";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "block_details");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto& any = std::get<any_t>(object.at("hash").value());
    BC_REQUIRE(any.holds_alternative<const hash_digest>());

    const auto& hash_cptr = any.get<const hash_digest>();
    BC_REQUIRE(hash_cptr);
    BC_REQUIRE_EQUAL(to_uintx(*hash_cptr), uint256_t{ 0x42 });
}

BC_AUTO_TEST_CASE(parsers__native_target__block_details_hash_extra_segment__extra_segment)
{
    const std::string path = "/v3/block/hash/0000000000000000000000000000000000000000000000000000000000000000/details/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

// template

BC_AUTO_TEST_CASE(parsers__native_target__template_valid__expected)
{
    request_t request{};
    BC_REQUIRE(!native_target(request, "/v42/template/123456"));
    BC_REQUIRE_EQUAL(request.method, "template");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 2u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);

    const auto height = std::get<uint32_t>(object.at("height").value());
    BC_REQUIRE_EQUAL(height, 123456u);
}

BC_AUTO_TEST_CASE(parsers__native_target__template_missing_height__missing_height)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/template"), server::error::missing_height);
}

BC_AUTO_TEST_CASE(parsers__native_target__template_invalid_height__invalid_number)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/template/invalid"), server::error::invalid_number);
}

BC_AUTO_TEST_CASE(parsers__native_target__template_extra_segment__extra_segment)
{
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, "/v3/template/123/extra"), server::error::extra_segment);
}

// fees

BC_AUTO_TEST_CASE(parsers__native_target__fees_valid__expected)
{
    const std::string path = "/v42/fees";

    request_t request{};
    BC_REQUIRE(!native_target(request, path));
    BC_REQUIRE_EQUAL(request.method, "fees");
    BC_REQUIRE(request.params.has_value());

    const auto& params = request.params.value();
    BC_REQUIRE(std::holds_alternative<object_t>(params));

    const auto& object = std::get<object_t>(request.params.value());
    BC_REQUIRE_EQUAL(object.size(), 1u);

    const auto version = std::get<uint8_t>(object.at("version").value());
    BC_REQUIRE_EQUAL(version, 42u);
}

BC_AUTO_TEST_CASE(parsers__native_target__fees_extra_segment__extra_segment)
{
    const std::string path = "/v3/fees/extra";
    request_t out{};
    BC_REQUIRE_EQUAL(native_target(out, path), server::error::extra_segment);
}

BC_AUTO_TEST_SUITE_END()
