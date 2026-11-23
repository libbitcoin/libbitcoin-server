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

#include <algorithm>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_stratum_v1

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;
constexpr auto relaxed = std::memory_order_relaxed;

// Shares counted (or the share interval multiplied) before a retarget.
constexpr size_t retarget_shares = 16;

// Share intervals without a share before the difficulty decays.
constexpr size_t decay_intervals = 4;

// Retarget bounds on the ratio of target to observed share interval.
constexpr double minimum_ratio = 0.25;
constexpr double maximum_ratio = 4.0;
constexpr double lower_dead_band = 0.8;
constexpr double upper_dead_band = 1.25;

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

    SUBSCRIBE_RPC(handle_mining_subscribe, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_RPC(handle_mining_authorize, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_mining_submit, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_RPC(handle_mining_configure, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_mining_suggest_difficulty, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_suggest_target, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_get_transactions, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_multi_version, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_extranonce_subscribe, _1, _2);
    submit_t::start();
}

void protocol_stratum_v1::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (active_.exchange(false))
        demand_template(false);

    decay_timer_->stop();
    submit_t::stopping(ec);
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

// Events.
// ----------------------------------------------------------------------------

bool protocol_stratum_v1::handle_chase(const code& ec,
    node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (stopped())
        return false;

    if (node::to_chase(value) == node::chase::template_ &&
        active_.load(relaxed))
        get_template(BIND(handle_template, _1, _2));

    return submit_t::handle_chase(ec, value);
}

// Handlers.
// ----------------------------------------------------------------------------

// Miners may send the connected host and port, which are not used.
void protocol_stratum_v1::handle_mining_subscribe(const code& ec,
    rpc_interface::mining_subscribe, const std::string&,
    const std::string&, const std::string&, const value_t&) NOEXCEPT
{
    if (stopped(ec))
        return;

    channel_->set_subscribed();
    const auto id = encode_base16(to_big_endian(channel_->identifier()));
    send_result(array_t
    {
        array_t
        {
            array_t{ string_t{ "mining.set_difficulty" }, id },
            array_t{ string_t{ "mining.notify" }, id }
        },
        string_t{ encode_base16(channel_->extranonce1()) },
        options_.extranonce2_size
    });

    start_jobs();
}

// The configured address governs, otherwise the worker name up to any dot.
void protocol_stratum_v1::handle_mining_authorize(const code& ec,
    rpc_interface::mining_authorize, const std::string& username,
    const std::string&) NOEXCEPT
{
    using namespace error::stratum_v1;
    if (stopped(ec))
        return;

    chain::script payout{};
    const auto& address = options_.payout_address;
    const auto name = address.empty() ?
        username.substr(zero, username.find('.')) : address;

    if (output_script(payout, name, p2kh_, p2sh_, witness_) ||
        (channel_->has_payout() && payout != channel_->payout()))
    {
        send_code(unauthorized_worker);
        return;
    }

    channel_->authorize(username, payout);
    send_result(true);
    start_jobs();
}

void protocol_stratum_v1::handle_mining_submit(const code& ec,
    rpc_interface::mining_submit, const std::string& worker_name,
    const std::string& job_id, const std::string& extranonce2,
    const std::string& ntime, const std::string& nonce,
    const std::string& version_bits) NOEXCEPT
{
    using namespace error::stratum_v1;
    if (stopped(ec))
        return;

    if (!channel_->subscribed())
    {
        send_code(not_subscribed);
        return;
    }

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

    const auto job = channel_->find(id);
    if (!job)
    {
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

    const auto extranonce = splice(channel_->extranonce1(), roll);
    const auto coinbase = splice(splice(job->prefix, extranonce), job->suffix);
    const chain::header header
    {
        version,
        job->previous,
        coinbase_root(bitcoin_hash(coinbase), job->held->branch),
        timestamp,
        job->bits,
        solution
    };

    // The miner sets only header fields, so a valid header is a valid block,
    // and a miner that invalidates the header is dropped.
    const auto& bitcoin = system_settings();
    const auto limit = add<uint64_t>(network::unix_time(),
        bitcoin.timestamp_limit_seconds);

    if (const auto failed = header.accept(job->held->context,
        bitcoin.retargeting_interval()))
    {
        send_code(unknown, failed);
        return;
    }

    if (timestamp > limit)
    {
        send_code(unknown, system::error::futuristic_timestamp);
        return;
    }

    const auto hash = header.hash();
    if (!job->shares.insert(hash).second)
    {
        send_code(duplicate_share);
        return;
    }

    const auto value = to_uintx(hash);
    if (value > to_share_target(job->difficulty))
    {
        send_code(low_difficulty_share);
        return;
    }

    // A share is valid work, whatever the outcome of a block it mines.
    send_result(true);
    if (value <= chain::compact::expand(job->bits))
        submit_mined(header, *job, extranonce);

    retarget();
}

// Version rolling (bip310) is the only extension, others are declined.
void protocol_stratum_v1::handle_mining_configure(const code& ec,
    rpc_interface::mining_configure, const interface::array_t& extensions,
    const interface::object_t& parameters) NOEXCEPT
{
    if (stopped(ec))
        return;

    object_t result{};
    for (const auto& extension: extensions)
    {
        const auto name = std::get_if<string_t>(&extension.value());
        if (is_null(name))
            continue;

        if (*name != "version-rolling")
        {
            result.emplace(*name, false);
            continue;
        }

        auto mask = max_uint32;
        const auto it = parameters.find("version-rolling.mask");
        if (it != parameters.end())
        {
            const auto text = std::get_if<string_t>(&it->second.value());
            if (is_null(text) || !decode_word(mask, *text))
                mask = zero;
        }

        mask &= version_rolling_mask;
        channel_->set_mask(mask);
        result.emplace(*name, true);
        result.emplace("version-rolling.mask", encode_word(mask));
    }

    send_result(std::move(result));
}

void protocol_stratum_v1::handle_mining_suggest_difficulty(const code& ec,
    rpc_interface::mining_suggest_difficulty, double difficulty) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(true);
    suggest(difficulty);
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
    suggest(to_share_difficulty(to_uintx(hash)));
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

// The extranonce of a channel does not change.
void protocol_stratum_v1::handle_mining_extranonce_subscribe(const code& ec,
    rpc_interface::mining_extranonce_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(true);
}

// Jobs.
// ----------------------------------------------------------------------------

// A subscribed and authorized channel demands templates and is issued jobs.
void protocol_stratum_v1::start_jobs() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!channel_->subscribed() || !channel_->has_payout() ||
        active_.exchange(true))
        return;

    demand_template(true);
    get_template(BIND(handle_template, _1, _2));
    start_decay();
}

void protocol_stratum_v1::handle_template(const code& ec,
    const node::block_template::cptr& held) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_template, ec, held);
}

void protocol_stratum_v1::do_template(const code& ec,
    const node::block_template::cptr& held) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace chain;
    if (stopped() || ec)
        return;

    const auto current = channel_->current();
    if (current && current->held == held)
        return;

    if (held->previous != archive().get_top_confirmed_hash())
    {
        get_template(BIND(handle_template, _1, _2));
        return;
    }

    const auto& ctx = held->context;
    const auto height = ctx.height;
    const auto minimum = add1(ctx.median_time_past);
    const auto timestamp = std::max(network::unix_time(), minimum);

    const auto& bitcoin = system_settings();
    const auto subsidy = block::subsidy(height,
        bitcoin.subsidy_interval_blocks,
        bitcoin.initial_subsidy(),
        bitcoin.forks.bip42);

    const auto value = ceilinged_add(subsidy, held->fees);
    const auto extranonce = add<size_t>(options_.extranonce1_size,
        options_.extranonce2_size);

    const auto clean = !current || current->previous != held->previous;
    send_job(emplace_shared<job>(job
    {
        held,
        coinbase_prefix(height, options_.coinbase_tag, extranonce),
        coinbase_suffix(height, value, channel_->payout(), held->commitment),
        held->previous,
        chain_state::signal_version(bitcoin),
        ctx.work_required,
        timestamp,
        channel_->difficulty(),
        {}
    }), clean);
}

// The difficulty of a job is notified before the job, as it applies to jobs
// that follow it.
void protocol_stratum_v1::send_job(const job::ptr& job, bool clean) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!is_duplex())
        return;

    const auto difficulty = job->difficulty;
    if (difficulty != channel_->notified())
    {
        channel_->set_notified(difficulty);
        send_notification(version::v1, "mining.set_difficulty", array_t
        {
            difficulty
        });
    }

    const auto id = channel_->issue(job, clean);

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

// The current job is issued again at the channel difficulty.
void protocol_stratum_v1::refresh() NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto current = channel_->current();
    if (!current)
        return;

    const auto copy = emplace_shared<job>(*current);
    copy->shares.clear();
    copy->difficulty = channel_->difficulty();
    send_job(copy, false);
}

// The difficulty moves toward the configured share interval (vardiff).
void protocol_stratum_v1::retarget() NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto interval = options_.share_interval_seconds;
    if (is_zero(interval))
        return;

    const auto count = channel_->count_share();
    const auto elapsed = channel_->elapsed();
    const auto target = to_floating(interval);
    if (count < retarget_shares &&
        elapsed < target * to_floating(retarget_shares))
        return;

    channel_->reset_window();
    const auto observed = elapsed / to_floating(count);
    const auto ratio = observed > 0.0 ?
        std::clamp(target / observed, minimum_ratio, maximum_ratio) :
        maximum_ratio;

    if (ratio > lower_dead_band && ratio < upper_dead_band)
        return;

    const auto difficulty = std::max(options_.minimum_difficulty,
        channel_->difficulty() * ratio);

    if (difficulty == channel_->difficulty())
        return;

    channel_->set_difficulty(difficulty);
    refresh();
}

// Without shares the difficulty cannot retarget, so it decays by the timer.
void protocol_stratum_v1::start_decay() NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto interval = options_.share_interval_seconds;
    if (stopped() || is_zero(interval))
        return;

    const auto span = network::seconds(interval * decay_intervals);
    decay_timer_->start(BIND(handle_decay, _1), span);
}

void protocol_stratum_v1::handle_decay(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        stop(ec);
        return;
    }

    if (is_zero(channel_->shares()))
    {
        const auto difficulty = std::max(options_.minimum_difficulty,
            channel_->difficulty() * minimum_ratio);

        if (difficulty != channel_->difficulty())
        {
            channel_->set_difficulty(difficulty);
            channel_->reset_window();
            refresh();
        }
    }

    start_decay();
}

// A suggested difficulty is floored at the configured minimum.
void protocol_stratum_v1::suggest(double difficulty) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto floored = std::max(options_.minimum_difficulty, difficulty);
    if (floored == channel_->difficulty())
        return;

    channel_->set_difficulty(floored);
    channel_->reset_window();
    refresh();
}

// Blocks.
// ----------------------------------------------------------------------------

// The share was answered, so the block is not gated on its outcome, and each
// outcome is reported to handle_submitted.
void protocol_stratum_v1::submit_mined(const chain::header& header,
    const job& job, const data_chunk& extranonce) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    const auto& links = job.held->links;
    mined_ = header.hash();
    height_ = job.held->context.height;

    data_chunk data{};
    write::bytes::data sink{ data };
    header.to_data(sink);
    sink.write_variable(add1(links.size()));
    sink.write_bytes(coinbase_witnessed(job.prefix, extranonce, job.suffix));

    for (const auto& link: links)
    {
        const auto tx = query.get_wire_tx(link, true);
        if (tx.empty())
        {
            handle_submitted(submission::failed, database::error::integrity);
            return;
        }

        sink.write_bytes(tx);
    }

    sink.flush();
    const auto block = to_shared<chain::view::block>(std::move(data), true);
    if (!block->is_valid())
    {
        handle_submitted(submission::failed, database::error::integrity);
        return;
    }

    submit_block(to_shared<chain::header>(header), block, {});
}

// The block is the template with the miner's valid header, so a block that
// is not valid is a store integrity fault. One not confirmed is not a fault.
void protocol_stratum_v1::handle_submitted(submission outcome,
    const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto block = "[" + encode_hash(mined_) + ":" +
        std::to_string(height_) + "]";

    switch (outcome)
    {
        case submission::accepted:
        case submission::duplicate:
        {
            LOGN("Stratum block accepted " << block);
            break;
        }
        case submission::inconclusive:
        case submission::duplicate_inconclusive:
        {
            LOGN("Stratum block inconclusive " << block);
            break;
        }
        case submission::duplicate_invalid:
        case submission::bad_parent:
        case submission::rejected:
        case submission::failed:
        {
            LOGF("Stratum block invalid " << block << ", " << ec.message());
            fault(database::error::integrity);
            break;
        }
    }
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
