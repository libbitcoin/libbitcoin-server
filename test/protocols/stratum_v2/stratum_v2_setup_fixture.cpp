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
#include "stratum_v2_setup_fixture.hpp"

using namespace system;
using namespace network::messages::sv2;

// Bounds on waiting for a confirmation and on messages skipped.
constexpr size_t confirmation_polls = 400;
constexpr auto confirmation_poll = std::chrono::milliseconds(50);
constexpr size_t maximum_skipped = 16;

const hash_digest stratum_v2_setup_fixture::maximum_target = []() NOEXCEPT
{
    hash_digest out{};
    out.fill(0xff);
    return out;
}();

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

stratum_v2_setup_fixture::stratum_v2_setup_fixture(const initializer& setup,
    const configurator& configure)
  : rpc_setup_fixture(setup, [configure](configuration& config) NOEXCEPT
        {
            auto& stratum = config.server.stratum_v2;
            stratum.binds = { { STRATUM_V2_ENDPOINT } };
            stratum.connections = 1;
            stratum.inactivity_minutes = 1;
            stratum.minimum_difficulty = stratum_v2_minimum_difficulty;
            stratum.share_interval_seconds = 0;
            config.node.currency_window_minutes = 0;

            if (configure)
                configure(config);
        }, true, true)
{
    client_.connect(binding());
}

stratum_v2_setup_fixture::~stratum_v2_setup_fixture()
{
    client_.close();
}

BC_POP_WARNING()

std::string stratum_v2_setup_fixture::address(uint8_t fill,
    chain::selection network)
{
    const server::settings::wallet_settings wallet{ network };
    const chain::script script{ chain::script::to_pay_key_hash_pattern(
        short_hash{ fill }) };

    return server::to_address(script, wallet.p2kh_prefix, wallet.p2sh_prefix,
        wallet.witness_prefix);
}

// transport
// ----------------------------------------------------------------------------

network::code stratum_v2_setup_fixture::upgrade()
{
    return client_.upgrade();
}

network::code stratum_v2_setup_fixture::upgrade(const std::string& username,
    const std::string& password)
{
    return client_.upgrade(username, password);
}

network::http::status stratum_v2_setup_fixture::get_status()
{
    return client_.get("/").result();
}

void stratum_v2_setup_fixture::send(const data_chunk& frame)
{
    client_.write_binary(frame);
}

data_chunk stratum_v2_setup_fixture::receive()
{
    return client_.read_binary();
}

// setup
// ----------------------------------------------------------------------------

data_chunk stratum_v2_setup_fixture::setup_frame(uint16_t minimum,
    uint16_t maximum, uint32_t flags, uint8_t protocol)
{
    return to_frame(setup_connection
    {
        protocol, minimum, maximum, flags, "127.0.0.1", 8980_u16, "vendor",
        "hardware", "firmware", "device"
    });
}

setup_connection_error::cptr stratum_v2_setup_fixture::get_error(
    const data_chunk& frame)
{
    send(frame);
    return receive_message<setup_connection_error>();
}

bool stratum_v2_setup_fixture::setup()
{
    if (upgrade())
        return false;

    send(setup_frame());
    return !!receive_message<setup_connection_success>();
}

// channels
// ----------------------------------------------------------------------------

void stratum_v2_setup_fixture::open_extended(const std::string& user,
    float hash_rate, const hash_digest& max_target, uint16_t min_extranonce)
{
    send_message(open_extended_mining_channel
    {
        7, user, hash_rate, max_target, min_extranonce
    });
}

void stratum_v2_setup_fixture::open_standard(const std::string& user,
    float hash_rate, const hash_digest& max_target)
{
    send_message(open_standard_mining_channel
    {
        7, user, hash_rate, max_target
    });
}

stratum_v2_setup_fixture::extended stratum_v2_setup_fixture::start_extended()
{
    if (!setup())
        return {};

    open_extended(address(0x01) + ".rig");
    extended out{};
    out.channel = receive_message<extended_success>();
    out.job = receive_message<extended_job>();
    out.previous = receive_message<prev_hash>();
    return out;
}

stratum_v2_setup_fixture::standard stratum_v2_setup_fixture::start_standard()
{
    if (!setup())
        return {};

    open_standard(address(0x01) + ".rig");
    standard out{};
    out.channel = receive_message<standard_success>();
    out.job = receive_message<standard_job>();
    out.previous = receive_message<prev_hash>();
    return out;
}

// shares
// ----------------------------------------------------------------------------

stratum_v2_setup_fixture::extended_share stratum_v2_setup_fixture::to_share(
    const extended& started, uint32_t nonce, uint32_t sequence)
{
    if (!started.channel || !started.job || !started.previous)
        return {};

    const auto& channel = *started.channel;
    return
    {
        channel.channel_id, sequence, started.job->job_id, nonce,
        started.previous->ntime_start, started.job->version,
        data_chunk(channel.extranonce_size, 0x00)
    };
}

stratum_v2_setup_fixture::standard_share stratum_v2_setup_fixture::to_share(
    const standard& started, uint32_t nonce, uint32_t sequence)
{
    if (!started.channel || !started.job || !started.previous)
        return {};

    return
    {
        started.channel->channel_id, sequence, started.job->job_id, nonce,
        started.previous->ntime_start, started.job->version
    };
}

bool stratum_v2_setup_fixture::submit_shares(const extended& started,
    size_t count)
{
    for (uint32_t nonce{}; nonce < count; ++nonce)
    {
        send_message(to_share(started, nonce, nonce));
        if (!receive_message<submit_shares_success>())
            return false;
    }

    return true;
}

static std::pair<uint32_t, hash_digest> find_nonce(uint32_t version,
    const hash_digest& previous, const hash_digest& root, uint32_t ntime,
    uint32_t bits)
{
    const auto target = chain::compact::expand(bits);
    for (uint32_t nonce{}; nonce < max_uint32; ++nonce)
    {
        const chain::header header{ version, previous, root, ntime, bits,
            nonce };

        const auto hash = header.hash();
        if (to_uintx(hash) <= target)
            return { nonce, hash };
    }

    return {};
}

std::pair<uint32_t, hash_digest> stratum_v2_setup_fixture::mine(
    const extended& started)
{
    if (!started.channel || !started.job || !started.previous)
        return {};

    const auto& job = *started.job;
    const auto& previous = *started.previous;
    const auto& channel = *started.channel;
    const auto extranonce = splice(channel.extranonce_prefix,
        data_chunk(channel.extranonce_size, 0x00));
    const auto coinbase = splice(splice(job.coinbase_tx_prefix, extranonce),
        job.coinbase_tx_suffix);
    const auto root = server::coinbase_root(bitcoin_hash(coinbase),
        job.merkle_path);

    return find_nonce(job.version, previous.prev_hash, root,
        previous.ntime_start, previous.nbits);
}

std::pair<uint32_t, hash_digest> stratum_v2_setup_fixture::mine(
    const standard& started)
{
    if (!started.job || !started.previous)
        return {};

    const auto& previous = *started.previous;
    return find_nonce(started.job->version, previous.prev_hash,
        started.job->merkle_root, previous.ntime_start, previous.nbits);
}

stratum_v2_setup_fixture::prev_hash::cptr
stratum_v2_setup_fixture::receive_previous()
{
    for (size_t skipped{}; skipped < maximum_skipped; ++skipped)
    {
        const auto frame = receive();
        const auto head = heading::deserialize(frame);
        if (!head)
            return {};

        if (head->message_type != prev_hash::identifier)
            continue;

        const auto start = std::next(frame.begin(), heading::size());
        return prev_hash::deserialize(data_chunk{ start, frame.end() });
    }

    return {};
}

bool stratum_v2_setup_fixture::confirmed(size_t height)
{
    for (size_t poll{}; poll < confirmation_polls; ++poll)
    {
        if (query_.get_top_confirmed() >= height)
            return true;

        std::this_thread::sleep_for(confirmation_poll);
    }

    return false;
}
