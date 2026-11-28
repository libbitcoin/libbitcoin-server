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

static std::string user()
{
    return stratum_v2_setup_fixture::address(0x01) + ".rig";
}

// Difficulty one target less one, converts to difficulty one.
constexpr auto inexact_target = base16_hash(
    "00000000fffeffffffffffffffffffffffffffffffffffffffffffffffffffff");

BC_FIXTURE_TEST_SUITE(stratum_v2_tests, stratum_v2_ten_block_setup_fixture)

// setup_connection
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__handshake__channel_stopped__completed_once)
{
    const auto codes = stop_handshake<session_stratum_v2>(config_.server.stratum_v2);
    BC_REQUIRE_EQUAL(codes.size(), 1u);
    BC_REQUIRE_EQUAL(codes.front(), network::error::peer_disconnect);
}

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

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__requires_standard_jobs__success)
{
    const auto ec = upgrade();
    send(setup_frame(2, 2, 0x00000001));
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

BC_AUTO_TEST_CASE(stratum_v2__setup_connection__all_flags__all_unsupported_reported)
{
    const auto ec = upgrade();
    const auto error = get_error(setup_frame(2, 2, 0xffffffff));
    BC_REQUIRE(!ec);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->flags, 0xfffffffau);
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

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__before_setup__dropped)
{
    const auto ec = upgrade();
    open_extended(user());
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

// open_extended_mining_channel
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__address_user__success)
{
    const auto ready = setup();
    open_extended(user());
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->request_id, 7u);
    BC_REQUIRE_EQUAL(success->channel_id, 1u);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(stratum_v2_minimum_difficulty));
    BC_REQUIRE_EQUAL(success->extranonce_size, 8u);
    BC_REQUIRE_EQUAL(success->extranonce_prefix.size(), 4u);
    BC_REQUIRE_EQUAL(success->group_channel_id, 0u);
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__first_job__future_job_then_previous)
{
    const auto started = start_extended();
    BC_REQUIRE(started.channel);
    BC_REQUIRE(started.job);
    BC_REQUIRE(started.previous);
    BC_REQUIRE_EQUAL(started.job->channel_id, 1u);
    BC_REQUIRE_EQUAL(started.job->job_id, 0u);
    BC_REQUIRE(!started.job->ntime_start.has_value());
    BC_REQUIRE(started.job->version_rolling_allowed);
    BC_REQUIRE(started.job->merkle_path.empty());
    BC_REQUIRE(!started.job->coinbase_tx_prefix.empty());
    BC_REQUIRE(!started.job->coinbase_tx_suffix.empty());
    BC_REQUIRE_EQUAL(started.previous->channel_id, 1u);
    BC_REQUIRE_EQUAL(started.previous->job_id, 0u);
    BC_REQUIRE_EQUAL(started.previous->prev_hash, query_.get_top_confirmed_hash());
    BC_REQUIRE_EQUAL(started.previous->nbits, 0x1d00ffffu);
    BC_REQUIRE(!is_zero(started.previous->ntime_start));
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__invalid_user__unknown_user)
{
    const auto ready = setup();
    open_extended("bogus");
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(ready);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->request_id, 7u);
    BC_REQUIRE_EQUAL(error->error_code, "unknown-user");
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__min_extranonce_above_size__min_extranonce_size_too_large)
{
    const auto ready = setup();
    open_extended(user(), 0.0f, maximum_target, 9);
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(ready);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "min-extranonce-size-too-large");
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__min_extranonce_of_size__success)
{
    const auto ready = setup();
    open_extended(user(), 0.0f, maximum_target, 8);
    const auto opened = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(opened);
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__zero_max_target__max_target_out_of_range)
{
    const auto ready = setup();
    open_extended(user(), 0.0f, null_hash);
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(ready);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "max-target-out-of-range");
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__max_target__target_floored)
{
    const auto ready = setup();
    open_extended(user(), 0.0f, server::to_target_hash(1.0));
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(1.0));
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__inexact_max_target__max_target)
{
    const auto ready = setup();
    open_extended(user(), 0.0f, inexact_target);
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, inexact_target);
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__hash_rate_without_interval__minimum_target)
{
    const auto ready = setup();
    open_extended(user(), 4294967296.0f);
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(stratum_v2_minimum_difficulty));
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__second__distinct_identifier_and_prefix)
{
    const auto first = start_extended();
    open_extended(user());
    const auto second = receive_message<extended_success>();
    BC_REQUIRE(first.channel);
    BC_REQUIRE(second);
    BC_REQUIRE_EQUAL(second->channel_id, 2u);
    BC_REQUIRE(second->extranonce_prefix != first.channel->extranonce_prefix);
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__second__first_job_of_second)
{
    const auto started = start_extended();
    open_extended(user());
    const auto opened = receive_message<extended_success>();
    const auto job = receive_message<extended_job>();
    const auto previous = receive_message<prev_hash>();
    BC_REQUIRE(started.channel);
    BC_REQUIRE(opened);
    BC_REQUIRE(job);
    BC_REQUIRE_EQUAL(job->channel_id, 2u);
    BC_REQUIRE(previous);
    BC_REQUIRE_EQUAL(previous->channel_id, 2u);
}

// open_standard_mining_channel
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__address_user__success)
{
    const auto ready = setup();
    open_standard(user());
    const auto success = receive_message<standard_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->request_id, 7u);
    BC_REQUIRE_EQUAL(success->channel_id, 1u);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(stratum_v2_minimum_difficulty));
    BC_REQUIRE_EQUAL(success->extranonce_prefix.size(), 12u);
    BC_REQUIRE_EQUAL(success->group_channel_id, 0u);
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__first_job__future_job_then_previous)
{
    const auto started = start_standard();
    BC_REQUIRE(started.channel);
    BC_REQUIRE(started.job);
    BC_REQUIRE(started.previous);
    BC_REQUIRE_EQUAL(started.job->channel_id, 1u);
    BC_REQUIRE_EQUAL(started.job->job_id, 0u);
    BC_REQUIRE(!started.job->ntime_start.has_value());
    BC_REQUIRE(started.job->merkle_root != null_hash);
    BC_REQUIRE_EQUAL(started.previous->job_id, 0u);
    BC_REQUIRE_EQUAL(started.previous->prev_hash, query_.get_top_confirmed_hash());
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__invalid_user__unknown_user)
{
    const auto ready = setup();
    open_standard("bogus");
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(ready);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "unknown-user");
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__unknown_witness_version__unknown_user)
{
    const auto ready = setup();
    open_standard(wallet::witness_address{ data_chunk(40, 0x01), 2 }.encoded());
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(ready);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "unknown-user");
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__zero_max_target__max_target_out_of_range)
{
    const auto ready = setup();
    open_standard(user(), 0.0f, null_hash);
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(ready);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "max-target-out-of-range");
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__inexact_max_target__max_target)
{
    const auto ready = setup();
    open_standard(user(), 0.0f, inexact_target);
    const auto success = receive_message<standard_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, inexact_target);
}

// submit_shares_extended
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__valid_share__success)
{
    const auto started = start_extended();
    send_message(to_share(started, 1, 42));
    const auto success = receive_message<submit_shares_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->channel_id, 1u);
    BC_REQUIRE_EQUAL(success->last_sequence_number, 42u);
    BC_REQUIRE_EQUAL(success->new_submits_accepted_count, 1u);
    BC_REQUIRE_EQUAL(success->new_shares_sum, 0u);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__distinct_shares__success)
{
    const auto started = start_extended();
    send_message(to_share(started, 1, 1));
    const auto first = receive_message<submit_shares_success>();
    send_message(to_share(started, 2, 2));
    const auto second = receive_message<submit_shares_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(first);
    BC_REQUIRE(second);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__duplicate_share__duplicate_share)
{
    const auto started = start_extended();
    send_message(to_share(started, 1, 1));
    const auto accepted = receive_message<submit_shares_success>();
    send_message(to_share(started, 1, 2));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->sequence_number, 2u);
    BC_REQUIRE_EQUAL(error->error_code, "duplicate-share");
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__unknown_channel__invalid_channel_id)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.channel_id = 9;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->channel_id, 9u);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-channel-id");
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__unknown_job__invalid_job_id)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.job_id = 9;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-job-id");
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__extranonce_size__dropped)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.extranonce.push_back(0x00);
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    const auto dropped = receive();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-extranonce-size");
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__version_within_mask__success)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.version |= 0x00002000;
    send_message(share);
    const auto accepted = receive_message<submit_shares_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__version_outside_mask__dropped)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.version ^= 0x00000001;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    const auto dropped = receive();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-version");
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__futuristic_ntime__dropped)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.ntime = max_uint32;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    const auto dropped = receive();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-share");
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__anachronistic_ntime__dropped)
{
    const auto started = start_extended();
    auto share = to_share(started, 1, 1);
    share.ntime = 1;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    const auto dropped = receive();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-share");
    BC_REQUIRE(dropped.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__standard_channel__invalid_channel_id)
{
    const auto started = start_standard();
    const auto share = to_share(started, 1, 1);
    send_message(extended_share{ 1, 1, 0, 1, share.ntime, share.version, data_chunk(8, 0x00) });
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-channel-id");
}

// submit_shares_standard
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_standard__valid_share__success)
{
    const auto started = start_standard();
    send_message(to_share(started, 1, 42));
    const auto success = receive_message<submit_shares_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->channel_id, 1u);
    BC_REQUIRE_EQUAL(success->last_sequence_number, 42u);
    BC_REQUIRE_EQUAL(success->new_submits_accepted_count, 1u);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_standard__duplicate_share__duplicate_share)
{
    const auto started = start_standard();
    send_message(to_share(started, 1, 1));
    const auto accepted = receive_message<submit_shares_success>();
    send_message(to_share(started, 1, 2));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "duplicate-share");
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_standard__extended_channel__invalid_channel_id)
{
    const auto started = start_extended();
    const auto share = to_share(started, 1, 1);
    send_message(standard_share{ 1, 1, 0, 1, share.ntime, share.version });
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-channel-id");
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_standard__version_outside_mask__dropped)
{
    const auto started = start_standard();
    auto share = to_share(started, 1, 1);
    share.version ^= 0x80000000;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    const auto dropped = receive();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-version");
    BC_REQUIRE(dropped.empty());
}

// update_channel
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__update_channel__unknown_channel__invalid_channel_id)
{
    const auto started = start_extended();
    send_message(update_channel{ 9, 0.0f, maximum_target });
    const auto error = receive_message<update_channel_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->channel_id, 9u);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-channel-id");
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__zero_max_target__max_target_out_of_range)
{
    const auto started = start_extended();
    send_message(update_channel{ 1, 0.0f, null_hash });
    const auto error = receive_message<update_channel_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "max-target-out-of-range");
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__lower_max_target__target_then_job)
{
    const auto started = start_extended();
    send_message(update_channel{ 1, 0.0f, server::to_target_hash(1.0) });
    const auto target = receive_message<set_target>();
    const auto job = receive_message<extended_job>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->channel_id, 1u);
    BC_REQUIRE_EQUAL(target->target, server::to_target_hash(1.0));
    BC_REQUIRE(job);
    BC_REQUIRE_EQUAL(job->job_id, 1u);
    BC_REQUIRE(job->ntime_start.has_value());
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__inexact_max_target__max_target_then_job)
{
    const auto started = start_extended();
    send_message(update_channel{ 1, 0.0f, inexact_target });
    const auto target = receive_message<set_target>();
    const auto job = receive_message<extended_job>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->target, inexact_target);
    BC_REQUIRE(job);
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__inexact_max_target_unchanged_floor__max_target)
{
    const auto ready = setup();
    open_extended(user(), 0.0f, server::to_target_hash(1.0));
    const auto success = receive_message<extended_success>();
    const auto job = receive_message<extended_job>();
    const auto previous = receive_message<prev_hash>();
    send_message(update_channel{ 1, 0.0f, inexact_target });
    send_message(update_channel{ 9, 0.0f, maximum_target });
    const auto target = receive_message<set_target>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(1.0));
    BC_REQUIRE(job);
    BC_REQUIRE(previous);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->target, inexact_target);
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__lower_max_target__prior_job_share_low_difficulty)
{
    const auto started = start_extended();
    send_message(update_channel{ 1, 0.0f, server::to_target_hash(1.0e30) });
    const auto target = receive_message<set_target>();
    const auto job = receive_message<extended_job>();
    auto share = to_share(started, 1, 1);
    share.job_id = 1;
    send_message(share);
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(target);
    BC_REQUIRE(job);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(job->job_id, 1u);
    BC_REQUIRE_EQUAL(error->error_code, "difficulty-too-low");
}

// close_channel
// ----------------------------------------------------------------------------

BC_AUTO_TEST_CASE(stratum_v2__close_channel__open__shares_refused)
{
    const auto started = start_extended();
    send_message(close_channel{ 1, "done" });
    send_message(to_share(started, 1, 1));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "invalid-channel-id");
}

BC_AUTO_TEST_CASE(stratum_v2__close_channel__unknown__retained)
{
    const auto started = start_extended();
    send_message(close_channel{ 9, "done" });
    open_extended(user());
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->channel_id, 2u);
}

BC_AUTO_TEST_CASE(stratum_v2__close_channel__reopened__identifier_not_reused)
{
    const auto started = start_extended();
    send_message(close_channel{ 1, "done" });
    open_extended(user());
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->channel_id, 2u);
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

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__credential__success)
{
    const auto ec = upgrade("miner", "pass");
    send(setup_frame());
    const auto response = receive();
    open_extended(user());
    const auto opened = receive_message<extended_success>();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(response, success_frame());
    BC_REQUIRE(opened);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_payout_tests, stratum_v2_payout_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__configured_payout__any_user)
{
    const auto ready = setup();
    open_extended("anyone");
    const auto opened = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(opened);
}

BC_AUTO_TEST_CASE(stratum_v2__new_extended_mining_job__configured_payout__paid_to_payout)
{
    const auto started = start_extended();
    const auto payout = chain::script{ chain::script::to_pay_key_hash_pattern(short_hash{ 0x42 }) }.to_data(false);
    BC_REQUIRE(started.job);
    BC_REQUIRE(std::ranges::search(started.job->coinbase_tx_suffix, payout).size() == payout.size());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_difficult_tests, stratum_v2_difficult_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__below_share_target__difficulty_too_low)
{
    const auto started = start_extended();
    send_message(to_share(started, 1, 1));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "difficulty-too-low");
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_standard__below_share_target__difficulty_too_low)
{
    const auto started = start_standard();
    send_message(to_share(started, 1, 1));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "difficulty-too-low");
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__minimum_difficulty__minimum_target)
{
    const auto ready = setup();
    open_extended(user());
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(1.0e30));
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_start_tests, stratum_v2_start_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__start_difficulty__start_target)
{
    const auto ready = setup();
    open_extended(user(), 4294967296.0f);
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(2.0));
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__start_difficulty__difficulty_too_low)
{
    const auto started = start_extended();
    send_message(to_share(started, 1, 1));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "difficulty-too-low");
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__start_difficulty__hash_rate_ignored)
{
    const auto started = start_extended();
    send_message(update_channel{ 1, 4294967296.0f, server::to_target_hash(4.0) });
    const auto target = receive_message<set_target>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->target, server::to_target_hash(4.0));
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_vardiff_tests, stratum_v2_vardiff_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__hash_rate__rate_target)
{
    const auto ready = setup();
    open_extended(user(), 4294967296.0f);
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(10.0));
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__hash_rate__rate_target)
{
    const auto ready = setup();
    open_standard(user(), 4294967296.0f);
    const auto success = receive_message<standard_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(10.0));
}

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__no_hash_rate__minimum_target)
{
    const auto ready = setup();
    open_extended(user());
    const auto success = receive_message<extended_success>();
    BC_REQUIRE(ready);
    BC_REQUIRE(success);
    BC_REQUIRE_EQUAL(success->target, server::to_target_hash(stratum_v2_minimum_difficulty));
}

BC_AUTO_TEST_CASE(stratum_v2__update_channel__hash_rate__rate_target_then_job)
{
    const auto started = start_extended();
    send_message(update_channel{ 1, 4294967296.0f, maximum_target });
    const auto target = receive_message<set_target>();
    const auto job = receive_message<extended_job>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->target, server::to_target_hash(10.0));
    BC_REQUIRE(job);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__fast_shares__target_reduced)
{
    const auto started = start_extended();
    const auto submitted = submit_shares(started, 16);
    const auto target = receive_message<set_target>();
    const auto job = receive_message<extended_job>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(submitted);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->target, server::to_target_hash(server::to_hash_difficulty(maximum_target) * 4.0));
    BC_REQUIRE(job);
    BC_REQUIRE(job->ntime_start.has_value());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_decay_tests, stratum_v2_decay_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__set_target__no_shares__target_raised)
{
    const auto started = start_extended();
    const auto target = receive_message<set_target>();
    const auto job = receive_message<extended_job>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(target);
    BC_REQUIRE_EQUAL(target->target, server::to_target_hash(0.25));
    BC_REQUIRE(job);
    BC_REQUIRE(job->ntime_start.has_value());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_one_channel_tests, stratum_v2_one_channel_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__open_extended_mining_channel__above_maximum__too_many_channels)
{
    const auto started = start_extended();
    open_extended(user());
    const auto error = receive_message<open_mining_channel_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "too-many-channels");
}

BC_AUTO_TEST_CASE(stratum_v2__open_standard_mining_channel__after_close__success)
{
    const auto started = start_extended();
    send_message(close_channel{ 1, "done" });
    open_standard(user());
    const auto opened = receive_message<standard_success>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(opened);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_authorize_timeout_tests, stratum_v2_authorize_timeout_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__authorize_timeout__no_setup__dropped)
{
    const auto ec = upgrade();
    const auto frame = receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE(frame.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__authorize_timeout__setup_no_channel__dropped)
{
    const auto ready = setup();
    const auto frame = receive();
    BC_REQUIRE(ready);
    BC_REQUIRE(frame.empty());
}

BC_AUTO_TEST_CASE(stratum_v2__authorize_timeout__channel_open__retained)
{
    const auto started = start_extended();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    BC_REQUIRE(submit_shares(started, 1));
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_broadcast_tests, stratum_v2_broadcast_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__new_extended_mining_job__pooled_transaction__merkle_path_of_one)
{
    const auto ec = submit(test::tx1c);
    const auto started = start_extended();
    BC_REQUIRE(!ec);
    BC_REQUIRE(started.job);
    BC_REQUIRE_EQUAL(started.job->merkle_path.size(), 1u);
}

BC_AUTO_TEST_CASE(stratum_v2__new_mining_job__pooled_transaction__share_accepted)
{
    const auto ec = submit(test::tx1c);
    const auto started = start_standard();
    send_message(to_share(started, 1, 1));
    const auto accepted = receive_message<submit_shares_success>();
    BC_REQUIRE(!ec);
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v2_regtest_tests, stratum_v2_regtest_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__block_target__block_confirmed_and_previous)
{
    const auto started = start_extended();
    const auto mined = mine(started);
    send_message(to_share(started, mined.first, 1));
    const auto accepted = receive_message<submit_shares_success>();
    const auto top = confirmed(1);
    const auto hash = query_.get_top_confirmed_hash();
    const auto previous = receive_previous();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
    BC_REQUIRE(top);
    BC_REQUIRE_EQUAL(hash, mined.second);
    BC_REQUIRE(previous);
    BC_REQUIRE_EQUAL(previous->prev_hash, mined.second);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_standard__block_target__block_confirmed_and_previous)
{
    const auto started = start_standard();
    const auto mined = mine(started);
    send_message(to_share(started, mined.first, 1));
    const auto accepted = receive_message<submit_shares_success>();
    const auto top = confirmed(1);
    const auto hash = query_.get_top_confirmed_hash();
    const auto previous = receive_previous();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
    BC_REQUIRE(top);
    BC_REQUIRE_EQUAL(hash, mined.second);
    BC_REQUIRE(previous);
    BC_REQUIRE_EQUAL(previous->prev_hash, mined.second);
}

BC_AUTO_TEST_CASE(stratum_v2__submit_shares_extended__prior_block_job__stale_share)
{
    const auto started = start_extended();
    const auto mined = mine(started);
    send_message(to_share(started, mined.first, 1));
    const auto accepted = receive_message<submit_shares_success>();
    const auto previous = receive_previous();
    send_message(to_share(started, add1(mined.first), 2));
    const auto error = receive_message<submit_shares_error>();
    BC_REQUIRE(started.previous);
    BC_REQUIRE(accepted);
    BC_REQUIRE(previous);
    BC_REQUIRE(error);
    BC_REQUIRE_EQUAL(error->error_code, "stale-share");
}

BC_AUTO_TEST_SUITE_END()
