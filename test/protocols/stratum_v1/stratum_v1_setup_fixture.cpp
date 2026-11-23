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
#include "../../mocks/blocks.hpp"
#include "stratum_v1_setup_fixture.hpp"

#include <chrono>
#include <thread>

using namespace system;

// Bounds on waiting for a confirmation and on notifications skipped.
constexpr size_t confirmation_polls = 400;
constexpr auto confirmation_poll = std::chrono::milliseconds(50);
constexpr size_t maximum_skipped = 16;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

stratum_v1_setup_fixture::stratum_v1_setup_fixture(const initializer& setup,
    const configurator& configure)
  : rpc_setup_fixture(setup, [configure](configuration& config) NOEXCEPT
        {
            auto& stratum = config.server.stratum_v1;
            stratum.binds = { { STRATUM_V1_ENDPOINT } };
            stratum.connections = 1;
            stratum.inactivity_minutes = 1;
            stratum.minimum_difficulty = stratum_v1_minimum_difficulty;
            stratum.share_interval_seconds = 0;
            config.node.currency_window_minutes = 0;

            if (configure)
                configure(config);
        }, true, true)
{
    client_.connect(binding());
}

stratum_v1_setup_fixture::~stratum_v1_setup_fixture()
{
    client_.close();
}

BC_POP_WARNING()

std::string stratum_v1_setup_fixture::address(uint8_t fill,
    chain::selection network)
{
    const server::settings::wallet_settings wallet{ network };
    const chain::script script{ chain::script::to_pay_key_hash_pattern(
        short_hash{ fill }) };

    return server::to_address(script, wallet.p2kh_prefix, wallet.p2sh_prefix,
        wallet.witness_prefix);
}

// tcp
// ----------------------------------------------------------------------------

system::json::value stratum_v1_setup_fixture::get(const std::string& request)
{
    return client_.send(request + "\n");
}

system::json::value stratum_v1_setup_fixture::receive()
{
    return client_.receive();
}

// The code of a positional error, -1 if none.
int64_t stratum_v1_setup_fixture::get_error(const std::string& request)
{
    try
    {
        return get(request).at("error").as_array().at(0).as_int64();
    }
    catch (const std::exception&)
    {
        return -1;
    }
}

// http and websocket
// ----------------------------------------------------------------------------

system::json::value stratum_v1_setup_fixture::post(const std::string& request)
{
    return client_.post(request);
}

network::code stratum_v1_setup_fixture::ws_upgrade()
{
    return client_.upgrade();
}

system::json::value stratum_v1_setup_fixture::ws_get(
    const std::string& request)
{
    return client_.frame(request);
}

system::json::value stratum_v1_setup_fixture::ws_receive()
{
    return client_.read_frame();
}

// jobs
// ----------------------------------------------------------------------------

system::json::value stratum_v1_setup_fixture::subscribe()
{
    const auto response = get(
        R"({"id":1,"method":"mining.subscribe","params":["test"]})");

    try
    {
        extranonce1_ = response.at("result").as_array().at(1).as_string();
    }
    catch (const std::exception&)
    {
        extranonce1_.clear();
    }

    return response;
}

system::json::value stratum_v1_setup_fixture::authorize(
    const std::string& worker)
{
    worker_ = worker;
    constexpr auto request =
        R"({{"id":2,"method":"mining.authorize","params":["{}","x"]}})";

    return get(std::format(request, worker));
}

system::json::value stratum_v1_setup_fixture::start_job()
{
    subscribe();
    authorize(address(0x01) + ".rig");

    // The difficulty precedes the first job.
    receive();
    return receive();
}

system::json::value stratum_v1_setup_fixture::submit(
    const system::json::value& job, const std::string& extranonce2,
    const std::string& nonce, const std::string& version_bits)
{
    const auto& params = job.at("params").as_array();
    const std::string id{ params.at(0).as_string() };
    const std::string ntime{ params.at(7).as_string() };
    const auto bits = version_bits.empty() ? std::string{} :
        std::format(R"(,"{}")", version_bits);

    constexpr auto request = R"({{"id":3,"method":"mining.submit",)"
        R"("params":["{}","{}","{}","{}","{}"{}]}})";

    return get(std::format(request, worker_, id, extranonce2, ntime, nonce,
        bits));
}

bool stratum_v1_setup_fixture::submit_shares(const system::json::value& job,
    size_t count)
{
    for (uint32_t nonce{}; nonce < count; ++nonce)
    {
        const auto response = submit(job, "00000000",
            server::encode_word(nonce));

        if (!response.at("result").is_bool() ||
            !response.at("result").as_bool())
            return false;
    }

    return true;
}

// The prevhash words are reversed again to recover the header field.
stratum_v1_setup_fixture::mined stratum_v1_setup_fixture::mine(
    const system::json::value& job, const std::string& extranonce2) const
{
    const auto& params = job.at("params").as_array();
    const auto text = [&](size_t index) NOEXCEPT
    {
        return std::string{ params.at(index).as_string() };
    };

    const auto word = [&](size_t index) NOEXCEPT
    {
        uint32_t value{};
        server::decode_word(value, text(index));
        return value;
    };

    hash_digest swapped{};
    hash_digest previous{};
    decode_base16(swapped, text(1));
    decode_base16(previous, server::to_stratum_hash(swapped));

    hashes branch{};
    for (const auto& hash: params.at(4).as_array())
    {
        hash_digest digest{};
        decode_base16(digest, std::string{ hash.as_string() });
        branch.push_back(digest);
    }

    data_chunk coinbase{};
    decode_base16(coinbase, text(2) + extranonce1_ + extranonce2 + text(3));
    const auto root = server::coinbase_root(bitcoin_hash(coinbase), branch);
    const auto target = chain::compact::expand(word(6));

    for (uint32_t nonce{}; nonce < max_uint32; ++nonce)
    {
        const chain::header header{ word(5), previous, root, word(7),
            word(6), nonce };

        const auto hash = header.hash();
        if (to_uintx(hash) <= target)
            return { server::encode_word(nonce), hash };
    }

    return {};
}

system::json::value stratum_v1_setup_fixture::receive_clean()
{
    for (size_t skipped{}; skipped < maximum_skipped; ++skipped)
    {
        const auto message = receive();
        if (message.as_object().contains("method") &&
            message.at("method").as_string() == "mining.notify" &&
            message.at("params").as_array().at(8).as_bool())
            return message;
    }

    return {};
}

bool stratum_v1_setup_fixture::confirmed(size_t height)
{
    for (size_t poll{}; poll < confirmation_polls; ++poll)
    {
        if (query_.get_top_confirmed() >= height)
            return true;

        std::this_thread::sleep_for(confirmation_poll);
    }

    return false;
}

std::string stratum_v1_setup_fixture::worker() const
{
    return worker_;
}
