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

BOOST_AUTO_TEST_SUITE(error_tests)

// error_t
// These test std::error_code equality operator overrides.

// general

BOOST_AUTO_TEST_CASE(error_t__code__success__false_expected_message)
{
    constexpr auto value = error::success;
    const auto ec = code(value);
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "success");
}

// server (url parse codes)

BOOST_AUTO_TEST_CASE(error_t__code__empty_path__true_expected_message)
{
    constexpr auto value = error::empty_path;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "empty_path");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_number__true_expected_message)
{
    constexpr auto value = error::invalid_number;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_number");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_hash__true_expected_message)
{
    constexpr auto value = error::invalid_hash;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_hash");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_version__true_expected_message)
{
    constexpr auto value = error::missing_version;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_version");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_target__true_expected_message)
{
    constexpr auto value = error::missing_target;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_target");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_target__true_expected_message)
{
    constexpr auto value = error::invalid_target;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_target");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_hash__true_expected_message)
{
    constexpr auto value = error::missing_hash;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_hash");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_height__true_expected_message)
{
    constexpr auto value = error::missing_height;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_height");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_position__true_expected_message)
{
    constexpr auto value = error::missing_position;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_position");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_id_type__true_expected_message)
{
    constexpr auto value = error::missing_id_type;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_id_type");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_id_type__true_expected_message)
{
    constexpr auto value = error::invalid_id_type;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_id_type");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_type_id__true_expected_message)
{
    constexpr auto value = error::missing_type_id;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_type_id");
}

BOOST_AUTO_TEST_CASE(error_t__code__missing_component__true_expected_message)
{
    constexpr auto value = error::missing_component;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "missing_component");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_component__true_expected_message)
{
    constexpr auto value = error::invalid_component;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_component");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_subcomponent__true_expected_message)
{
    constexpr auto value = error::invalid_subcomponent;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_subcomponent");
}

BOOST_AUTO_TEST_CASE(error_t__code__extra_segment__true_expected_message)
{
    constexpr auto value = error::extra_segment;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "extra_segment");
}

// server (rpc response codes)

BOOST_AUTO_TEST_CASE(error_t__code__not_found__true_expected_message)
{
    constexpr auto value = error::not_found;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "not_found");
}

BOOST_AUTO_TEST_CASE(error_t__code__not_implemented__true_expected_message)
{
    constexpr auto value = error::not_implemented;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "not_implemented");
}

BOOST_AUTO_TEST_CASE(error_t__code__invalid_argument__true_expected_message)
{
    constexpr auto value = error::invalid_argument;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "invalid_argument");
}

BOOST_AUTO_TEST_CASE(error_t__code__subscription_limit__true_expected_message)
{
    constexpr auto value = error::subscription_limit;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "subscription_limit");
}

BOOST_AUTO_TEST_CASE(error_t__code__unsupported_argument__true_expected_message)
{
    constexpr auto value = error::unsupported_argument;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "unsupported_argument");
}

BOOST_AUTO_TEST_CASE(error_t__code__unconfirmable_transaction__true_expected_message)
{
    constexpr auto value = error::unconfirmable_transaction;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "unconfirmable_transaction");
}

BOOST_AUTO_TEST_CASE(error_t__code__argument_overflow__true_expected_message)
{
    constexpr auto value = error::argument_overflow;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "argument_overflow");
}

BOOST_AUTO_TEST_CASE(error_t__code__target_overflow__true_expected_message)
{
    constexpr auto value = error::target_overflow;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "target_overflow");
}

BOOST_AUTO_TEST_CASE(error_t__code__maximum_depth__true_expected_message)
{
    constexpr auto value = error::maximum_depth;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "maximum_depth");
}

BOOST_AUTO_TEST_CASE(error_t__code__wrong_version__true_expected_message)
{
    constexpr auto value = error::wrong_version;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "wrong_version");
}

BOOST_AUTO_TEST_CASE(error_t__code__server_error__true_expected_message)
{
    constexpr auto value = error::server_error;
    const auto ec = code(value);
    BOOST_REQUIRE(ec);
    BOOST_REQUIRE(ec == value);
    BOOST_REQUIRE_EQUAL(ec.message(), "server_error");
}

// electrum::translate

BOOST_AUTO_TEST_CASE(error_t__electrum_translate__success__success)
{
    BOOST_REQUIRE(!error::electrum::translate(error::success, error::electrum::bad_request));
}

BOOST_AUTO_TEST_CASE(error_t__electrum_translate__electrum_code__unchanged)
{
    const code ec{ error::electrum::server_busy };
    BOOST_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == ec);
}

BOOST_AUTO_TEST_CASE(error_t__electrum_translate__depth_limited__excessive_history)
{
    const code ec{ database::error::depth_limited };
    BOOST_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == error::electrum::excessive_history);
}

BOOST_AUTO_TEST_CASE(error_t__electrum_translate__database_code__daemon_error)
{
    const code ec{ database::error::integrity };
    BOOST_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == error::electrum::daemon_error);
}

BOOST_AUTO_TEST_CASE(error_t__electrum_translate__other_code__failure)
{
    const code ec{ error::not_found };
    BOOST_REQUIRE(error::electrum::translate(ec, error::electrum::bad_request) == error::electrum::bad_request);
}

// btcd::translate

BOOST_AUTO_TEST_CASE(error_t__btcd_translate__success__success)
{
    BOOST_REQUIRE(!error::btcd::translate(error::success, error::btcd::misc_error));
}

BOOST_AUTO_TEST_CASE(error_t__btcd_translate__btcd_code__unchanged)
{
    const code ec{ error::btcd::invalid_parameter };
    BOOST_REQUIRE(error::btcd::translate(ec, error::btcd::misc_error) == ec);
}

BOOST_AUTO_TEST_CASE(error_t__btcd_translate__database_code__internal_error)
{
    const code ec{ database::error::integrity };
    BOOST_REQUIRE(error::btcd::translate(ec, error::btcd::misc_error) == error::btcd::internal_error);
}

BOOST_AUTO_TEST_CASE(error_t__btcd_translate__other_code__failure)
{
    const code ec{ error::not_found };
    BOOST_REQUIRE(error::btcd::translate(ec, error::btcd::misc_error) == error::btcd::misc_error);
}

// bitcoind::translate

BOOST_AUTO_TEST_CASE(error_t__bitcoind_translate__success__success)
{
    BOOST_REQUIRE(!error::bitcoind::translate(error::success, error::bitcoind::misc_error));
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_translate__bitcoind_code__unchanged)
{
    const code ec{ error::bitcoind::invalid_parameter };
    BOOST_REQUIRE(error::bitcoind::translate(ec, error::bitcoind::misc_error) == ec);
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_translate__database_code__internal_error)
{
    const code ec{ database::error::integrity };
    BOOST_REQUIRE(error::bitcoind::translate(ec, error::bitcoind::misc_error) == error::bitcoind::internal_error);
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_translate__other_code__failure)
{
    const code ec{ error::not_found };
    BOOST_REQUIRE(error::bitcoind::translate(ec, error::bitcoind::misc_error) == error::bitcoind::misc_error);
}

// bitcoind::reject

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__node_orphan_block__prev_blk_not_found)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(node::error::orphan_block), "prev-blk-not-found");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__node_orphan_header__prev_blk_not_found)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(node::error::orphan_header), "prev-blk-not-found");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__node_duplicate_block__duplicate)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(node::error::duplicate_block), "duplicate");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__node_duplicate_header__duplicate)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(node::error::duplicate_header), "duplicate");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__empty_transaction__bad_txns_vin_empty)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(system::error::empty_transaction), "bad-txns-vin-empty");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__unmapped_transaction_code__message)
{
    const code ec{ system::error::double_spend };
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(ec), ec.message());
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__invalid_proof_of_work__high_hash)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(system::error::invalid_proof_of_work), "high-hash");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__block_weight_limit__bad_blk_weight)
{
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(system::error::block_weight_limit), "bad-blk-weight");
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__unmapped_block_code__message)
{
    const code ec{ system::error::checkpoint_conflict };
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(ec), ec.message());
}

BOOST_AUTO_TEST_CASE(error_t__bitcoind_reject__other_code__message)
{
    const code ec{ error::not_found };
    BOOST_REQUIRE_EQUAL(error::bitcoind::reject(ec), "not_found");
}

BOOST_AUTO_TEST_SUITE_END()
