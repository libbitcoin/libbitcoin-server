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
#include "stratum_v1_setup_fixture.hpp"

using namespace system;
static const code unknown{ server::error::stratum_v1::unknown };
static const code job_not_found{ server::error::stratum_v1::job_not_found };
static const code duplicate_share{ server::error::stratum_v1::duplicate_share };
static const code low_difficulty_share{ server::error::stratum_v1::low_difficulty_share };
static const code unauthorized_worker{ server::error::stratum_v1::unauthorized_worker };
static const code not_subscribed{ server::error::stratum_v1::not_subscribed };
static const code not_implemented{ server::error::not_implemented };
static const auto subsidy = 5'000'000'000_u64;

static chain::script p2kh(uint8_t fill)
{
    const short_hash hash{ fill };
    return chain::script{ chain::script::to_pay_key_hash_pattern(hash) };
}

BC_FIXTURE_TEST_SUITE(stratum_v1_tests, stratum_v1_ten_block_setup_fixture)

// handshake

BC_AUTO_TEST_CASE(stratum_v1__handshake__channel_stopped__completed_once)
{
    const auto codes = stop_handshake<session_stratum_v1>(config_.server.stratum_v1);
    BC_REQUIRE_EQUAL(codes.size(), 1u);
    BC_REQUIRE_EQUAL(codes.front(), network::error::peer_disconnect);
}

// mining.subscribe

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__tcp__subscriptions_extranonce1_and_size)
{
    const auto response = subscribe();
    const auto& result = response.at("result").as_array();
    BC_REQUIRE(!response.as_object().contains("jsonrpc"));
    BC_REQUIRE(response.at("error").is_null());
    BC_REQUIRE_EQUAL(response.at("id").as_int64(), 1);
    BC_REQUIRE(result.at(0).as_array().at(0).as_array().at(0).as_string() == "mining.set_difficulty");
    BC_REQUIRE(result.at(0).as_array().at(1).as_array().at(0).as_string() == "mining.notify");
    BC_REQUIRE_EQUAL(result.at(1).as_string().size(), 8u);
    BC_REQUIRE_EQUAL(result.at(2).as_int64(), 4);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__http__subscriptions)
{
    const auto response = post(R"({"id":1,"method":"mining.subscribe","params":[]})");
    BC_REQUIRE(response.at("error").is_null());
    BC_REQUIRE_EQUAL(response.at("result").as_array().size(), 3u);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__null_session_host_and_port__subscriptions)
{
    const auto response = get(R"({"id":1,"method":"mining.subscribe","params":["bosminer-plus-am1-s9.0.9.0",null,"127.0.0.1:3333",null]})");
    BC_REQUIRE(response.at("error").is_null());
    BC_REQUIRE_EQUAL(response.at("result").as_array().size(), 3u);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__numeric_port__subscriptions)
{
    const auto response = get(R"({"id":1,"method":"mining.subscribe","params":["test","","127.0.0.1",3333]})");
    BC_REQUIRE(response.at("error").is_null());
    BC_REQUIRE_EQUAL(response.at("result").as_array().size(), 3u);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__websocket__jobs_notified)
{
    const auto ec = ws_upgrade();
    const auto subscribed = ws_get(R"({"id":1,"method":"mining.subscribe","params":[]})");
    const auto authorized = ws_get(std::format(R"({{"id":2,"method":"mining.authorize","params":["{}","x"]}})", address(0x01)));
    const auto difficulty = ws_receive();
    const auto job = ws_receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE(subscribed.at("error").is_null());
    BC_REQUIRE(authorized.at("result").as_bool());
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE(job.at("method").as_string() == "mining.notify");
}

// mining.authorize

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__address_worker__true)
{
    const auto response = authorize(address(0x01) + ".rig");
    BC_REQUIRE(response.at("result").as_bool());
    BC_REQUIRE(response.at("error").is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__null_password__true)
{
    const auto response = get(std::format(R"({{"id":2,"method":"mining.authorize","params":["{}",null]}})", address(0x01)));
    BC_REQUIRE(response.at("result").as_bool());
    BC_REQUIRE(response.at("error").is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__invalid_address__unauthorized_worker)
{
    const auto response = authorize("bogus.rig");
    const auto& error = response.at("error").as_array();
    BC_REQUIRE(response.at("result").is_null());
    BC_REQUIRE_EQUAL(error.size(), 3u);
    BC_REQUIRE_EQUAL(error.at(0).as_int64(), unauthorized_worker.value());
    BC_REQUIRE(error.at(1).as_string() == "Unauthorized worker");
    BC_REQUIRE(error.at(2).is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__unknown_witness_version__unauthorized_worker)
{
    const auto response = authorize(wallet::witness_address{ data_chunk(40, 0x01), 2 }.encoded());
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__second_payout__unauthorized_worker)
{
    const auto first = authorize(address(0x01));
    const auto second = authorize(address(0x02));
    BC_REQUIRE(first.at("result").as_bool());
    BC_REQUIRE_EQUAL(second.at("error").as_array().at(0).as_int64(), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__second_worker_same_payout__true)
{
    const auto first = authorize(address(0x01) + ".one");
    const auto second = authorize(address(0x01) + ".two");
    BC_REQUIRE(first.at("result").as_bool());
    BC_REQUIRE(second.at("result").as_bool());
}

// mining.configure

BC_AUTO_TEST_CASE(stratum_v1__mining_configure__version_rolling__mask_intersected)
{
    const auto response = get(R"({"id":4,"method":"mining.configure","params":[["version-rolling"],{"version-rolling.mask":"ffffffff"}]})");
    const auto& result = response.at("result").as_object();
    BC_REQUIRE(result.at("version-rolling").as_bool());
    BC_REQUIRE(result.at("version-rolling.mask").as_string() == "1fffe000");
}

BC_AUTO_TEST_CASE(stratum_v1__mining_configure__no_mask__general_purpose_bits)
{
    const auto response = get(R"({"id":4,"method":"mining.configure","params":[["version-rolling"],{}]})");
    BC_REQUIRE(response.at("result").as_object().at("version-rolling.mask").as_string() == "1fffe000");
}

BC_AUTO_TEST_CASE(stratum_v1__mining_configure__invalid_mask__zero)
{
    const auto response = get(R"({"id":4,"method":"mining.configure","params":[["version-rolling"],{"version-rolling.mask":"zz"}]})");
    BC_REQUIRE(response.at("result").as_object().at("version-rolling.mask").as_string() == "00000000");
}

BC_AUTO_TEST_CASE(stratum_v1__mining_configure__other_extensions__declined)
{
    const auto response = get(R"({"id":4,"method":"mining.configure","params":[["minimum-difficulty","subscribe-extranonce",42],{}]})");
    const auto& result = response.at("result").as_object();
    BC_REQUIRE_EQUAL(result.size(), 2u);
    BC_REQUIRE(!result.at("minimum-difficulty").as_bool());
    BC_REQUIRE(!result.at("subscribe-extranonce").as_bool());
}

// mining.suggest_difficulty, mining.suggest_target

BC_AUTO_TEST_CASE(stratum_v1__mining_suggest_difficulty__before_job__job_difficulty)
{
    const auto suggested = get(R"({"id":5,"method":"mining.suggest_difficulty","params":[2.0]})");
    subscribe();
    authorize(address(0x01));
    const auto difficulty = receive();
    BC_REQUIRE(suggested.at("result").as_bool());
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), 2.0);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_suggest_difficulty__below_minimum__unchanged)
{
    const auto suggested = get(R"({"id":5,"method":"mining.suggest_difficulty","params":[1.0e-20]})");
    subscribe();
    authorize(address(0x01));
    const auto difficulty = receive();
    BC_REQUIRE(suggested.at("result").as_bool());
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), stratum_v1_minimum_difficulty);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_suggest_difficulty__after_job__job_reissued)
{
    const auto job = start_job();
    const auto suggested = get(R"({"id":5,"method":"mining.suggest_difficulty","params":[2.0]})");
    const auto difficulty = receive();
    const auto reissued = receive();
    BC_REQUIRE(suggested.at("result").as_bool());
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), 2.0);
    BC_REQUIRE(reissued.at("params").as_array().at(0).as_string() == "00000001");
    BC_REQUIRE(!reissued.at("params").as_array().at(8).as_bool());
    BC_REQUIRE(reissued.at("params").as_array().at(2) == job.at("params").as_array().at(2));
    BC_REQUIRE(reissued.at("params").as_array().at(7) != job.at("params").as_array().at(7));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__share_of_reissued_job__true)
{
    const auto job = start_job();
    const auto share = submit(job, "00000000", "00000000");
    const auto suggested = get(R"({"id":5,"method":"mining.suggest_difficulty","params":[2.0e-12]})");
    const auto difficulty = receive();
    const auto reissued = receive();
    const auto response = submit(reissued, "00000000", "00000000");
    BC_REQUIRE(share.at("result").as_bool());
    BC_REQUIRE(suggested.at("result").as_bool());
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE(response.at("result").as_bool());
    BC_REQUIRE(response.at("error").is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_suggest_target__difficulty_one__job_difficulty)
{
    const auto suggested = get(R"({"id":5,"method":"mining.suggest_target","params":["00000000ffff0000000000000000000000000000000000000000000000000000"]})");
    subscribe();
    authorize(address(0x01));
    const auto difficulty = receive();
    BC_REQUIRE(suggested.at("result").as_bool());
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), 1.0);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_suggest_target__not_hash__unknown)
{
    BC_REQUIRE_EQUAL(get_error(R"({"id":5,"method":"mining.suggest_target","params":["zz"]})"), unknown.value());
}

// mining.extranonce.subscribe, mining.get_transactions, mining.multi_version

BC_AUTO_TEST_CASE(stratum_v1__mining_extranonce_subscribe__true)
{
    BC_REQUIRE(get(R"({"id":6,"method":"mining.extranonce.subscribe","params":[]})").at("result").as_bool());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_get_transactions__unknown_not_implemented)
{
    start_job();
    const auto response = get(R"({"id":7,"method":"mining.get_transactions","params":["00000000"]})");
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(response.at("error").as_array().at(1).as_string() == not_implemented.message());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_multi_version__unknown_not_implemented)
{
    start_job();
    const auto response = get(R"({"id":8,"method":"mining.multi_version","params":[4]})");
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(response.at("error").as_array().at(1).as_string() == not_implemented.message());
}

// unclaimed and malformed

BC_AUTO_TEST_CASE(stratum_v1__unknown_method__method_not_found)
{
    BC_REQUIRE_EQUAL(get_error(R"({"id":9,"method":"mining.bogus","params":[]})"), network::rpc::reserved::method_not_found);
}

BC_AUTO_TEST_CASE(stratum_v1__server_notification__method_not_found)
{
    BC_REQUIRE_EQUAL(get_error(R"({"id":9,"method":"mining.notify","params":[]})"), network::rpc::reserved::method_not_found);
}

BC_AUTO_TEST_CASE(stratum_v1__unknown_method__mining__method_not_found)
{
    start_job();
    BC_REQUIRE_EQUAL(get_error(R"({"id":9,"method":"mining.bogus","params":[]})"), network::rpc::reserved::method_not_found);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__missing_params__invalid_params)
{
    start_job();
    BC_REQUIRE_EQUAL(get_error(R"({"id":10,"method":"mining.submit","params":["w"]})"), network::rpc::reserved::invalid_params);
}

// mining.notify

BC_AUTO_TEST_CASE(stratum_v1__mining_notify__first_job__expected)
{
    const auto job = start_job();
    const auto& params = job.at("params").as_array();
    const auto version = chain::chain_state::signal_version(config_.bitcoin);
    const auto suffix = server::coinbase_suffix(10, subsidy, p2kh(0x01));
    BC_REQUIRE(job.at("id").is_null());
    BC_REQUIRE(!job.as_object().contains("jsonrpc"));
    BC_REQUIRE_EQUAL(params.size(), 9u);
    BC_REQUIRE(params.at(0).as_string() == "00000000");
    BC_REQUIRE(params.at(1).as_string() == server::to_stratum_hash(test::block9_hash));
    BC_REQUIRE(params.at(2).as_string() == encode_base16(server::coinbase_prefix(10, "", 8)));
    BC_REQUIRE(params.at(3).as_string() == encode_base16(suffix));
    BC_REQUIRE(params.at(4).as_array().empty());
    BC_REQUIRE(params.at(5).as_string() == server::encode_word(version));
    BC_REQUIRE(params.at(6).as_string() == "1d00ffff");
    BC_REQUIRE_EQUAL(params.at(7).as_string().size(), 8u);
    BC_REQUIRE(params.at(8).as_bool());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_notify__first_job__minimum_difficulty_first)
{
    subscribe();
    authorize(address(0x01));
    const auto difficulty = receive();
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE(difficulty.at("id").is_null());
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), stratum_v1_minimum_difficulty);
}

BC_AUTO_TEST_CASE(stratum_v1__mining_notify__unauthorized__no_job)
{
    subscribe();
    const auto authorized = authorize("bogus");
    const auto subscribed = get(R"({"id":6,"method":"mining.extranonce.subscribe","params":[]})");
    BC_REQUIRE_EQUAL(authorized.at("error").as_array().at(0).as_int64(), unauthorized_worker.value());
    BC_REQUIRE(subscribed.at("result").as_bool());
}

// mining.submit

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__valid_share__true)
{
    const auto job = start_job();
    const auto response = submit(job, "00000000", "00000000");
    BC_REQUIRE(response.at("result").as_bool());
    BC_REQUIRE(response.at("error").is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__duplicate_share__duplicate_share)
{
    const auto job = start_job();
    const auto first = submit(job, "00000000", "00000000");
    const auto second = submit(job, "00000000", "00000000");
    BC_REQUIRE(first.at("result").as_bool());
    BC_REQUIRE_EQUAL(second.at("error").as_array().at(0).as_int64(), duplicate_share.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__distinct_shares__true)
{
    const auto job = start_job();
    BC_REQUIRE(submit_shares(job, 4));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__not_subscribed__not_subscribed)
{
    authorize(address(0x01));
    BC_REQUIRE_EQUAL(get_error(std::format(R"({{"id":3,"method":"mining.submit","params":["{}","00000000","00000000","00000000","00000000"]}})", worker())), not_subscribed.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__unauthorized_worker__unauthorized_worker)
{
    subscribe();
    BC_REQUIRE_EQUAL(get_error(R"({"id":3,"method":"mining.submit","params":["nobody","00000000","00000000","00000000","00000000"]})"), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__mining_unauthorized_worker__unauthorized_worker)
{
    start_job();
    BC_REQUIRE_EQUAL(get_error(R"({"id":3,"method":"mining.submit","params":["nobody","00000000","00000000","00000000","00000000"]})"), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__unknown_job__job_not_found)
{
    start_job();
    BC_REQUIRE_EQUAL(get_error(std::format(R"({{"id":3,"method":"mining.submit","params":["{}","00000009","00000000","00000000","00000000"]}})", worker())), job_not_found.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__job_not_word__unknown_dropped)
{
    start_job();
    const auto error = get_error(std::format(R"({{"id":3,"method":"mining.submit","params":["{}","job","00000000","00000000","00000000"]}})", worker()));
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(error, unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__anachronistic_ntime__unknown_dropped)
{
    const auto job = start_job();
    const auto response = get(std::format(R"({{"id":3,"method":"mining.submit","params":["{}","{}","00000000","00000001","00000000"]}})", worker(), job.at("params").as_array().at(0).as_string()));
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__futuristic_ntime__unknown_dropped)
{
    const auto job = start_job();
    const auto response = get(std::format(R"({{"id":3,"method":"mining.submit","params":["{}","{}","00000000","ffffffff","00000000"]}})", worker(), job.at("params").as_array().at(0).as_string()));
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__extranonce2_size__unknown_dropped)
{
    const auto job = start_job();
    const auto response = submit(job, "000000", "00000000");
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__nonce_not_word__unknown_dropped)
{
    const auto job = start_job();
    const auto response = submit(job, "00000000", "zz");
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__version_bits_without_mask__unknown_dropped)
{
    const auto job = start_job();
    const auto response = submit(job, "00000000", "00000000", "00002000");
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__version_bits_not_word__unknown_dropped)
{
    const auto job = start_job();
    const auto configured = get(R"({"id":4,"method":"mining.configure","params":[["version-rolling"],{}]})");
    const auto response = submit(job, "00000000", "00000000", "zz");
    const auto dropped = receive();
    BC_REQUIRE(configured.at("result").as_object().at("version-rolling").as_bool());
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__version_bits_within_mask__true)
{
    const auto job = start_job();
    const auto configured = get(R"({"id":4,"method":"mining.configure","params":[["version-rolling"],{}]})");
    const auto response = submit(job, "00000000", "00000000", "00002000");
    BC_REQUIRE(configured.at("result").as_object().at("version-rolling").as_bool());
    BC_REQUIRE(response.at("result").as_bool());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__version_bits_outside_mask__unknown_dropped)
{
    const auto job = start_job();
    const auto configured = get(R"({"id":4,"method":"mining.configure","params":[["version-rolling"],{}]})");
    const auto response = submit(job, "00000000", "00000000", "00000001");
    const auto dropped = receive();
    BC_REQUIRE(configured.at("result").as_object().at("version-rolling").as_bool());
    BC_REQUIRE_EQUAL(response.at("error").as_array().at(0).as_int64(), unknown.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_payout_tests, stratum_v1_payout_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__configured_payout__any_worker)
{
    BC_REQUIRE(authorize("anyone").at("result").as_bool());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_notify__configured_payout__paid_to_payout)
{
    subscribe();
    authorize("anyone");
    receive();
    const auto job = receive();
    const auto suffix = server::coinbase_suffix(10, subsidy, p2kh(0x42));
    BC_REQUIRE(job.at("params").as_array().at(3).as_string() == encode_base16(suffix));
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_authorize_timeout_tests, stratum_v1_authorize_timeout_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__authorize_timeout__no_messages__dropped)
{
    BC_REQUIRE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(stratum_v1__authorize_timeout__subscribed_unauthorized__dropped)
{
    subscribe();
    BC_REQUIRE(receive().at("dropped").as_bool());
}

BC_AUTO_TEST_CASE(stratum_v1__authorize_timeout__subscribed_authorized__retained)
{
    const auto job = start_job();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    BC_REQUIRE(submit(job, "00000000", "00000000").at("result").as_bool());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_difficult_tests, stratum_v1_difficult_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__below_share_target__low_difficulty_share)
{
    const auto job = start_job();
    BC_REQUIRE_EQUAL(submit(job, "00000000", "00000000").at("error").as_array().at(0).as_int64(), low_difficulty_share.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__below_share_target_repeated__low_difficulty_share)
{
    const auto job = start_job();
    const auto first = submit(job, "00000000", "00000000");
    const auto second = submit(job, "00000000", "00000000");
    BC_REQUIRE_EQUAL(first.at("error").as_array().at(0).as_int64(), low_difficulty_share.value());
    BC_REQUIRE_EQUAL(second.at("error").as_array().at(0).as_int64(), low_difficulty_share.value());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_vardiff_tests, stratum_v1_vardiff_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__fast_shares__difficulty_raised)
{
    const auto job = start_job();
    const auto accepted = submit_shares(job, 16);
    const auto difficulty = receive();
    const auto reissued = receive();
    BC_REQUIRE(accepted);
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), stratum_v1_minimum_difficulty * 4.0);
    BC_REQUIRE(reissued.at("method").as_string() == "mining.notify");
    BC_REQUIRE(!reissued.at("params").as_array().at(8).as_bool());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_decay_tests, stratum_v1_decay_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_notify__no_shares__difficulty_decayed)
{
    const auto suggested = get(R"({"id":5,"method":"mining.suggest_difficulty","params":[2.0]})");
    subscribe();
    authorize(address(0x01));
    const auto initial = receive();
    const auto job = receive();
    const auto difficulty = receive();
    const auto reissued = receive();
    BC_REQUIRE(suggested.at("result").as_bool());
    BC_REQUIRE_EQUAL(initial.at("params").as_array().at(0).as_double(), 2.0);
    BC_REQUIRE(job.at("method").as_string() == "mining.notify");
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE_EQUAL(difficulty.at("params").as_array().at(0).as_double(), 0.5);
    BC_REQUIRE(reissued.at("method").as_string() == "mining.notify");
    BC_REQUIRE(!reissued.at("params").as_array().at(8).as_bool());
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_credential_tests, stratum_v1_credential_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__tcp_credential_unsatisfied__subscriptions)
{
    BC_REQUIRE(subscribe().at("error").is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__tcp_wrong_password__unauthorized_worker)
{
    BC_REQUIRE_EQUAL(authorize(address(0x01) + ".rig", "wrong").at("error").as_array().at(0).as_int64(), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__tcp_other_account__unauthorized_worker)
{
    BC_REQUIRE_EQUAL(authorize(address(0x02) + ".rig", "pass").at("error").as_array().at(0).as_int64(), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__tcp_credential__job_notified)
{
    subscribe();
    const auto authorized = authorize(address(0x01) + ".rig", "pass");
    const auto difficulty = receive();
    const auto job = receive();
    BC_REQUIRE(authorized.at("result").as_bool());
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE(job.at("method").as_string() == "mining.notify");
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__tcp_credential_unsatisfied__unauthorized_dropped)
{
    subscribe();
    const auto error = get_error(R"({"id":3,"method":"mining.submit","params":["w","00000000","00000000","00000000","00000000"]})");
    const auto dropped = receive();
    BC_REQUIRE_EQUAL(error, code{ network::error::unauthorized }.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__websocket_upgrade__without_credential__accepted)
{
    BC_REQUIRE(!ws_upgrade());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_subscribe__websocket_credential_unsatisfied__subscriptions)
{
    ws_upgrade();
    const auto subscribed = ws_get(R"({"id":1,"method":"mining.subscribe","params":[]})");
    BC_REQUIRE(subscribed.at("error").is_null());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__websocket_wrong_password__unauthorized_worker)
{
    ws_upgrade();
    const auto authorized = ws_get(std::format(R"({{"id":2,"method":"mining.authorize","params":["{}.rig","wrong"]}})", address(0x01)));
    BC_REQUIRE_EQUAL(authorized.at("error").as_array().at(0).as_int64(), unauthorized_worker.value());
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__websocket_in_band_credential__job_notified)
{
    ws_upgrade();
    ws_get(R"({"id":1,"method":"mining.subscribe","params":[]})");
    const auto authorized = ws_get(std::format(R"({{"id":2,"method":"mining.authorize","params":["{}.rig","pass"]}})", address(0x01)));
    const auto difficulty = ws_receive();
    const auto job = ws_receive();
    BC_REQUIRE(authorized.at("result").as_bool());
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE(job.at("method").as_string() == "mining.notify");
}

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__websocket_credential_unsatisfied__unauthorized_dropped)
{
    ws_upgrade();
    ws_get(R"({"id":1,"method":"mining.subscribe","params":[]})");
    const auto submitted = ws_get(R"({"id":3,"method":"mining.submit","params":["w","00000000","00000000","00000000","00000000"]})");
    const auto dropped = ws_receive();
    BC_REQUIRE_EQUAL(submitted.at("error").as_array().at(0).as_int64(), code{ network::error::unauthorized }.value());
    BC_REQUIRE(dropped.as_object().contains("dropped"));
}

BC_AUTO_TEST_CASE(stratum_v1__mining_authorize__websocket_credential__job_notified)
{
    const auto ec = ws_upgrade(address(0x01), "pass");
    const auto subscribed = ws_get(R"({"id":1,"method":"mining.subscribe","params":[]})");
    const auto authorized = ws_get(std::format(R"({{"id":2,"method":"mining.authorize","params":["{}","x"]}})", address(0x01)));
    const auto difficulty = ws_receive();
    const auto job = ws_receive();
    BC_REQUIRE(!ec);
    BC_REQUIRE(subscribed.at("error").is_null());
    BC_REQUIRE(authorized.at("result").as_bool());
    BC_REQUIRE(difficulty.at("method").as_string() == "mining.set_difficulty");
    BC_REQUIRE(job.at("method").as_string() == "mining.notify");
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_broadcast_tests, stratum_v1_broadcast_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_notify__pooled_transaction__branch_of_one)
{
    const auto ec = submit(test::tx1c);
    const auto job = start_job();
    BC_REQUIRE(!ec);
    BC_REQUIRE_EQUAL(job.at("params").as_array().at(4).as_array().size(), 1u);
}

BC_AUTO_TEST_SUITE_END()

BC_FIXTURE_TEST_SUITE(stratum_v1_regtest_tests, stratum_v1_regtest_setup_fixture)

BC_AUTO_TEST_CASE(stratum_v1__mining_submit__block_target__block_confirmed_and_clean_job)
{
    const auto job = start_job();
    const auto found = mine(job, "00000001");
    const auto response = submit(job, "00000001", found.nonce);
    const auto top = confirmed(1);
    const auto hash = query_.get_top_confirmed_hash();
    const auto next = receive_clean();
    BC_REQUIRE(response.at("result").as_bool());
    BC_REQUIRE(top);
    BC_REQUIRE_EQUAL(hash, found.hash);
    BC_REQUIRE(next.at("params").as_array().at(1).as_string() == server::to_stratum_hash(found.hash));
}

BC_AUTO_TEST_SUITE_END()
