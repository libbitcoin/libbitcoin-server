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
#include <bitcoin/server/protocols/protocol_stratum_v1.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_stratum_v1

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_stratum_v1::start() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (started())
        return;

    SUBSCRIBE_RPC(handle_mining_submit, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_RPC(handle_mining_suggest_difficulty, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_suggest_target, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_get_transactions, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_multi_version, _1, _2, _3);

    mining_t::start();
    add_channel(zero, mining_,
    {
        .service = "stratum_v1",
        .transport = miners::to_transport(false, channel_->websocket(),
            channel_->secure()),
        .client = channel_->client(),
        .workers = channel_->workers()
    });
    start_jobs();
}

// No attached protocol subscribes the method (terminal responder).
void protocol_stratum_v1::handle_unclaimed(const request_t&) NOEXCEPT
{
    BC_ASSERT(stranded());
    send_error(
    {
        .code = reserved::method_not_found,
        .message = "Method not found"
    });
}

// Handlers.
// ----------------------------------------------------------------------------

void protocol_stratum_v1::handle_mining_submit(const code& ec,
    rpc_interface::mining_submit, const std::string& worker_name,
    const std::string& job_id, const std::string& extranonce2,
    const std::string& ntime, const std::string& nonce,
    const std::string& version_bits) NOEXCEPT
{
    using namespace error::stratum_v1;
    if (stopped(ec))
        return;

    if (!channel_->authorized_worker(worker_name))
    {
        send_code(unauthorized_worker);
        return;
    }

    uint32_t id{};
    if (!decode_word(id, job_id))
    {
        send_code(unknown, error::invalid_argument);
        return;
    }

    const auto job = mining_->find(id);
    if (!job)
    {
        reject_share(*mining_, "job_not_found");
        send_code(job_not_found);
        return;
    }

    data_chunk roll{};
    uint32_t timestamp{}, solution{};
    const auto size = options_.extranonce2_size;
    if (!decode_base16(roll, extranonce2) || roll.size() != size ||
        !decode_word(timestamp, ntime) ||
        !decode_word(solution, nonce))
    {
        send_code(unknown, error::invalid_argument);
        return;
    }

    // Version bits are rolled only within the configured mask (bip310).
    auto version = job->version;
    if (!version_bits.empty())
    {
        uint32_t bits{};
        const auto mask = channel_->mask();
        if (!decode_word(bits, version_bits) ||
            !is_zero(bit_and(bits, bit_not(mask))))
        {
            send_code(unknown, error::invalid_argument);
            return;
        }

        version = bit_or(bit_and(version, bit_not(mask)), bits);
    }

    // A miner that invalidates the header is dropped.
    code failed{};
    chain::header header{};
    switch (check_share(*mining_, *job, version, timestamp, solution, roll,
        header, failed))
    {
        case share_outcome::invalid:
        {
            send_code(unknown, failed);
            return;
        }
        case share_outcome::low_difficulty:
        {
            send_code(low_difficulty_share);
            return;
        }
        case share_outcome::duplicate:
        {
            send_code(duplicate_share);
            return;
        }
        case share_outcome::accepted:
        {
            // A share is valid work, whatever the outcome of a block it mines.
            send_result(true);
            accept_share(*mining_, *job, header, roll);
            return;
        }
    }
}

void protocol_stratum_v1::handle_mining_suggest_difficulty(const code& ec,
    rpc_interface::mining_suggest_difficulty, double difficulty) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(true);
    suggest(*mining_, difficulty);
}

void protocol_stratum_v1::handle_mining_suggest_target(const code& ec,
    rpc_interface::mining_suggest_target, const std::string& target) NOEXCEPT
{
    using namespace error::stratum_v1;
    if (stopped(ec))
        return;

    hash_digest hash{};
    if (!decode_hash(hash, target))
    {
        send_code(unknown);
        return;
    }

    send_result(true);
    suggest(*mining_, to_share_difficulty(to_uintx(hash)));
}

// Unserved methods answer the stratum code, with the reason as message.
void protocol_stratum_v1::handle_mining_get_transactions(const code& ec,
    rpc_interface::mining_get_transactions, const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_error(
    {
        .code = error::stratum_v1::unknown,
        .message = code{ error::not_implemented }.message()
    });
}

void protocol_stratum_v1::handle_mining_multi_version(const code& ec,
    rpc_interface::mining_multi_version, double) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_error(
    {
        .code = error::stratum_v1::unknown,
        .message = code{ error::not_implemented }.message()
    });
}

// Jobs.
// ----------------------------------------------------------------------------

size_t protocol_stratum_v1::extranonce_size() const NOEXCEPT
{
    return add<size_t>(options_.extranonce1_size, options_.extranonce2_size);
}

// The difficulty of a job is notified before the job, as it applies to jobs
// that follow it.
void protocol_stratum_v1::send_job(mining_channel& channel,
    const job::cptr& job, bool clean) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!is_duplex())
        return;

    const auto difficulty = job->difficulty;
    if (difficulty != channel.notified())
    {
        channel.set_notified(difficulty);
        send_notification(version::v1, "mining.set_difficulty", array_t
        {
            difficulty
        });
    }

    const auto id = channel.issue(job, clean);

    array_t branch{};
    for (const auto& hash: job->held->branch)
        branch.emplace_back(encode_base16(hash));

    send_notification(version::v1, "mining.notify", array_t
    {
        encode_word(id),
        to_stratum_hash(job->previous),
        encode_base16(job->prefix),
        encode_base16(job->suffix),
        std::move(branch),
        encode_word(job->version),
        encode_word(job->bits),
        encode_word(job->timestamp),
        clean
    });
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
