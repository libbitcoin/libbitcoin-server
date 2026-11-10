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

BC_AUTO_TEST_SUITE(error_tests)

// error_t
// These test std::error_code equality operator overrides.

// general

BC_AUTO_TEST_CASE(error_t__code__success__false_expected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BC_REQUIRE(!ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "success");
}

// server (url parse codes)

BC_AUTO_TEST_CASE(error_t__code__empty_path__true_expected_message)
{
    constexpr auto value = error::empty_path;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "empty_path");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_number__true_expected_message)
{
    constexpr auto value = error::invalid_number;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_number");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_hash__true_expected_message)
{
    constexpr auto value = error::invalid_hash;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_hash");
}

BC_AUTO_TEST_CASE(error_t__code__missing_version__true_expected_message)
{
    constexpr auto value = error::missing_version;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_version");
}

BC_AUTO_TEST_CASE(error_t__code__missing_target__true_expected_message)
{
    constexpr auto value = error::missing_target;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_target");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_target__true_expected_message)
{
    constexpr auto value = error::invalid_target;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_target");
}

BC_AUTO_TEST_CASE(error_t__code__missing_hash__true_expected_message)
{
    constexpr auto value = error::missing_hash;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_hash");
}

BC_AUTO_TEST_CASE(error_t__code__missing_height__true_expected_message)
{
    constexpr auto value = error::missing_height;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_height");
}

BC_AUTO_TEST_CASE(error_t__code__missing_position__true_expected_message)
{
    constexpr auto value = error::missing_position;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_position");
}

BC_AUTO_TEST_CASE(error_t__code__missing_id_type__true_expected_message)
{
    constexpr auto value = error::missing_id_type;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_id_type");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_id_type__true_expected_message)
{
    constexpr auto value = error::invalid_id_type;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_id_type");
}

BC_AUTO_TEST_CASE(error_t__code__missing_type_id__true_expected_message)
{
    constexpr auto value = error::missing_type_id;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_type_id");
}

BC_AUTO_TEST_CASE(error_t__code__missing_component__true_expected_message)
{
    constexpr auto value = error::missing_component;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "missing_component");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_component__true_expected_message)
{
    constexpr auto value = error::invalid_component;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_component");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_subcomponent__true_expected_message)
{
    constexpr auto value = error::invalid_subcomponent;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_subcomponent");
}

BC_AUTO_TEST_CASE(error_t__code__extra_segment__true_expected_message)
{
    constexpr auto value = error::extra_segment;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "extra_segment");
}

// server (rpc response codes)

BC_AUTO_TEST_CASE(error_t__code__not_found__true_expected_message)
{
    constexpr auto value = error::not_found;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "not_found");
}

BC_AUTO_TEST_CASE(error_t__code__not_implemented__true_expected_message)
{
    constexpr auto value = error::not_implemented;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "not_implemented");
}

BC_AUTO_TEST_CASE(error_t__code__invalid_argument__true_expected_message)
{
    constexpr auto value = error::invalid_argument;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "invalid_argument");
}

BC_AUTO_TEST_CASE(error_t__code__subscription_limit__true_expected_message)
{
    constexpr auto value = error::subscription_limit;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "subscription_limit");
}

BC_AUTO_TEST_CASE(error_t__code__unsupported_argument__true_expected_message)
{
    constexpr auto value = error::unsupported_argument;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "unsupported_argument");
}

BC_AUTO_TEST_CASE(error_t__code__unconfirmable_transaction__true_expected_message)
{
    constexpr auto value = error::unconfirmable_transaction;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "unconfirmable_transaction");
}

BC_AUTO_TEST_CASE(error_t__code__argument_overflow__true_expected_message)
{
    constexpr auto value = error::argument_overflow;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "argument_overflow");
}

BC_AUTO_TEST_CASE(error_t__code__target_overflow__true_expected_message)
{
    constexpr auto value = error::target_overflow;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "target_overflow");
}

BC_AUTO_TEST_CASE(error_t__code__maximum_depth__true_expected_message)
{
    constexpr auto value = error::maximum_depth;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "maximum_depth");
}

BC_AUTO_TEST_CASE(error_t__code__wrong_version__true_expected_message)
{
    constexpr auto value = error::wrong_version;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "wrong_version");
}

BC_AUTO_TEST_CASE(error_t__code__server_error__true_expected_message)
{
    constexpr auto value = error::server_error;
    const auto ec = code(value);
    BC_REQUIRE(ec);
    BC_REQUIRE(ec == value);
    BC_REQUIRE_EQUAL(ec.message(), "server_error");
}

// bitcoind::parameter_error

BC_AUTO_TEST_CASE(error_t__bitcoind_parameter_error__mistyped__type_error)
{
    using namespace error::bitcoind;
    BC_REQUIRE(parameter_error(network::error::unexpected_type) == type_error);
    BC_REQUIRE(parameter_error(network::error::unexpected_null) == type_error);
    BC_REQUIRE(parameter_error(network::error::missing_array) == type_error);
    BC_REQUIRE(parameter_error(network::error::missing_object) == type_error);
}

BC_AUTO_TEST_CASE(error_t__bitcoind_parameter_error__unknown_name__invalid_parameter)
{
    using namespace error::bitcoind;
    BC_REQUIRE(parameter_error(network::error::extra_named) == invalid_parameter);
}

BC_AUTO_TEST_CASE(error_t__bitcoind_parameter_error__missing_or_extra__misc_error)
{
    using namespace error::bitcoind;
    BC_REQUIRE(parameter_error(network::error::missing_parameter) == misc_error);
    BC_REQUIRE(parameter_error(network::error::extra_positional) == misc_error);
}

BC_AUTO_TEST_CASE(error_t__bitcoind_parameter_error__other_code__success)
{
    using namespace error::bitcoind;
    BC_REQUIRE(!parameter_error(network::error::undefined_type));
    BC_REQUIRE(!parameter_error(network::error::unexpected_method));
    BC_REQUIRE(!parameter_error(error::not_found));
}

// btcd::parameter_error

BC_AUTO_TEST_CASE(error_t__btcd_parameter_error__parameter_fault__invalid_params)
{
    using namespace error::btcd;
    BC_REQUIRE(parameter_error(network::error::unexpected_type) == invalid_params);
    BC_REQUIRE(parameter_error(network::error::unexpected_null) == invalid_params);
    BC_REQUIRE(parameter_error(network::error::missing_array) == invalid_params);
    BC_REQUIRE(parameter_error(network::error::missing_object) == invalid_params);
    BC_REQUIRE(parameter_error(network::error::extra_named) == invalid_params);
    BC_REQUIRE(parameter_error(network::error::extra_positional) == invalid_params);
    BC_REQUIRE(parameter_error(network::error::missing_parameter) == invalid_params);
}

BC_AUTO_TEST_CASE(error_t__btcd_parameter_error__other_code__success)
{
    using namespace error::btcd;
    BC_REQUIRE(!parameter_error(network::error::undefined_type));
    BC_REQUIRE(!parameter_error(error::not_found));
}

// electrum::translate

BC_AUTO_TEST_CASE(error_t__electrum_translate__success__success)
{
    BC_REQUIRE(!error::electrum::translate(error::success, error::electrum::bad_request));
}

BC_AUTO_TEST_CASE(error_t__electrum_translate__electrum_code__unchanged)
{
    const code ec{ error::electrum::server_busy };
    BC_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == ec);
}

BC_AUTO_TEST_CASE(error_t__electrum_translate__depth_limited__excessive_history)
{
    const code ec{ database::error::depth_limited };
    BC_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == error::electrum::excessive_history);
}

BC_AUTO_TEST_CASE(error_t__electrum_translate__database_code__daemon_error)
{
    const code ec{ database::error::integrity };
    BC_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == error::electrum::daemon_error);
}

BC_AUTO_TEST_CASE(error_t__electrum_translate__other_code__failure)
{
    const code ec{ error::not_found };
    BC_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == error::electrum::bad_request);
}

// btcd::translate

BC_AUTO_TEST_CASE(error_t__btcd_translate__success__success)
{
    BC_REQUIRE(!error::btcd::translate(error::success, error::btcd::misc_error));
}

BC_AUTO_TEST_CASE(error_t__btcd_translate__btcd_code__unchanged)
{
    const code ec{ error::btcd::invalid_parameter };
    BC_REQUIRE(error::btcd::translate(ec, error::btcd::misc_error) == ec);
}

BC_AUTO_TEST_CASE(error_t__btcd_translate__database_code__internal_error)
{
    const code ec{ database::error::integrity };
    BC_REQUIRE(error::btcd::translate(ec, error::btcd::misc_error) == error::btcd::internal_error);
}

BC_AUTO_TEST_CASE(error_t__btcd_translate__other_code__failure)
{
    const code ec{ error::not_found };
    BC_REQUIRE(error::btcd::translate(ec, error::btcd::misc_error) == error::btcd::misc_error);
}

// bitcoind::translate

BC_AUTO_TEST_CASE(error_t__bitcoind_translate__success__success)
{
    BC_REQUIRE(!error::bitcoind::translate(error::success, error::bitcoind::misc_error));
}

BC_AUTO_TEST_CASE(error_t__bitcoind_translate__bitcoind_code__unchanged)
{
    const code ec{ error::bitcoind::invalid_parameter };
    BC_REQUIRE(error::bitcoind::translate(ec, error::bitcoind::misc_error) == ec);
}

BC_AUTO_TEST_CASE(error_t__bitcoind_translate__database_code__internal_error)
{
    const code ec{ database::error::integrity };
    BC_REQUIRE(error::bitcoind::translate(ec, error::bitcoind::misc_error) == error::bitcoind::internal_error);
}

BC_AUTO_TEST_CASE(error_t__bitcoind_translate__other_code__failure)
{
    const code ec{ error::not_found };
    BC_REQUIRE(error::bitcoind::translate(ec, error::bitcoind::misc_error) == error::bitcoind::misc_error);
}

// bitcoind::reject

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__node_orphan_block__prev_blk_not_found)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(node::error::orphan_block), "prev-blk-not-found");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__node_orphan_header__prev_blk_not_found)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(node::error::orphan_header), "prev-blk-not-found");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__node_duplicate_block__duplicate)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(node::error::duplicate_block), "duplicate");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__node_duplicate_header__duplicate)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(node::error::duplicate_header), "duplicate");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__empty_transaction__bad_txns_vin_empty)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(system::error::empty_transaction), "bad-txns-vin-empty");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__unmapped_transaction_code__message)
{
    const code ec{ system::error::double_spend };
    BC_REQUIRE_EQUAL(error::bitcoind::reject(ec), ec.message());
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__invalid_proof_of_work__high_hash)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(system::error::invalid_proof_of_work), "high-hash");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__block_weight_limit__bad_blk_weight)
{
    BC_REQUIRE_EQUAL(error::bitcoind::reject(system::error::block_weight_limit), "bad-blk-weight");
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__unmapped_block_code__message)
{
    const code ec{ system::error::checkpoint_conflict };
    BC_REQUIRE_EQUAL(error::bitcoind::reject(ec), ec.message());
}

BC_AUTO_TEST_CASE(error_t__bitcoind_reject__other_code__message)
{
    const code ec{ error::not_found };
    BC_REQUIRE_EQUAL(error::bitcoind::reject(ec), "not_found");
}

BC_AUTO_TEST_SUITE_END()
