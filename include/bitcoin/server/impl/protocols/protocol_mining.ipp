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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_MINING_IPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_MINING_IPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define TEMPLATE template <typename Base>
#define CLASS protocol_mining<Base>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Stop.
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    if (active_.exchange(false))
        this->demand_template(false);

    decay_timer_->stop();
    base::stopping(ec);
}

// Channels.
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::add_channel(uint32_t id,
    const mining_channel::ptr& channel) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    channels_.emplace(id, channel);
}

TEMPLATE
void CLASS::remove_channel(uint32_t id) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    channels_.erase(id);
}

TEMPLATE
mining_channel::ptr CLASS::find_channel(uint32_t id) const NOEXCEPT
{
    BC_ASSERT(this->stranded());
    const auto it = channels_.find(id);
    return it == channels_.end() ? mining_channel::ptr{} : it->second;
}

TEMPLATE
size_t CLASS::mining_channel_count() const NOEXCEPT
{
    BC_ASSERT(this->stranded());
    return channels_.size();
}

// Events.
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::handle_chase(const code& ec, node::event_value value) NOEXCEPT
{
    using namespace std::placeholders;

    // Do not pass ec to stopped, it is not a call status.
    if (this->stopped())
        return false;

    if (node::to_chase(value) == node::chase::template_ &&
        active_.load(std::memory_order_relaxed))
        this->get_template(this->template bind<CLASS>(&CLASS::handle_template,
            _1, _2));

    return base::handle_chase(ec, value);
}

// Jobs.
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::start_jobs() NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace std::placeholders;

    if (!active_.exchange(true))
    {
        this->demand_template(true);
        start_decay();
    }

    this->get_template(this->template bind<CLASS>(&CLASS::handle_template,
        _1, _2));
}

TEMPLATE
void CLASS::handle_template(const code& ec,
    const node::block_template::cptr& held) NOEXCEPT
{
    if (this->stopped())
        return;

    POST(do_template, ec, held);
}

TEMPLATE
void CLASS::do_template(const code& ec,
    const node::block_template::cptr& held) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace system;
    using namespace system::chain;
    using namespace std::placeholders;

    if (this->stopped() || ec)
        return;

    if (held->previous != this->archive().get_top_confirmed_hash())
    {
        this->get_template(this->template bind<CLASS>(&CLASS::handle_template,
            _1, _2));
        return;
    }

    const auto& ctx = held->context;
    const auto height = ctx.height;
    const auto minimum = add1(ctx.median_time_past);
    const auto timestamp = std::max(network::unix_time(), minimum);

    const auto& bitcoin = this->system_settings();
    const auto subsidy = block::subsidy(height,
        bitcoin.subsidy_interval_blocks,
        bitcoin.initial_subsidy(),
        bitcoin.forks.bip42);

    const auto value = ceilinged_add(subsidy, held->fees);
    const auto prefix = coinbase_prefix(height, options_.coinbase_tag,
        extranonce_size());

    for (const auto& entry: channels_)
    {
        auto& channel = *entry.second;
        const auto current = channel.current();
        if (current && current->held == held)
            continue;

        const auto clean = !current || current->previous != held->previous;
        send_job(channel, to_shared(job
        {
            held,
            prefix,
            coinbase_suffix(height, value, channel.payout(), held->commitment),
            held->previous,
            chain_state::signal_version(bitcoin),
            ctx.work_required,
            timestamp,
            channel.difficulty()
        }), clean);
    }
}

// The current job is issued again at the channel difficulty and a later time.
TEMPLATE
void CLASS::refresh(mining_channel& channel) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    const auto current = channel.current();
    if (!current)
        return;

    auto copy = *current;
    copy.difficulty = channel.difficulty();
    copy.timestamp = std::max(network::unix_time(), add1(current->timestamp));
    send_job(channel, system::to_shared(std::move(copy)), false);
}

// Shares.
// ----------------------------------------------------------------------------

// The miner sets only header fields, so a valid header is a valid block.
TEMPLATE
share_outcome CLASS::check_share(mining_channel& channel, const job& job,
    uint32_t version, uint32_t timestamp, uint32_t nonce,
    const system::data_chunk& rolled, system::chain::header& out,
    code& ec) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace system;

    const auto extranonce = splice(channel.extranonce(), rolled);
    const auto coinbase = splice(splice(job.prefix, extranonce), job.suffix);
    out = chain::header
    {
        version,
        job.previous,
        coinbase_root(bitcoin_hash(coinbase), job.held->branch),
        timestamp,
        job.bits,
        nonce
    };

    const auto& bitcoin = this->system_settings();
    const auto limit = add<uint64_t>(network::unix_time(),
        bitcoin.timestamp_limit_seconds);

    ec = out.accept(job.held->context, bitcoin.retargeting_interval());
    if (ec)
        return share_outcome::invalid;

    if (timestamp > limit)
    {
        ec = system::error::futuristic_timestamp;
        return share_outcome::invalid;
    }

    const auto hash = out.hash();
    if (to_uintx(hash) > to_share_target(job.difficulty))
        return share_outcome::low_difficulty;

    if (!channel.accept(hash))
        return share_outcome::duplicate;

    return share_outcome::accepted;
}

TEMPLATE
void CLASS::accept_share(mining_channel& channel, const job& job,
    const system::chain::header& header,
    const system::data_chunk& rolled) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace system;

    if (to_uintx(header.hash()) <= chain::compact::expand(job.bits))
        submit_mined(header, job, splice(channel.extranonce(), rolled));

    retarget(channel);
}

// Difficulty.
// ----------------------------------------------------------------------------

TEMPLATE
double CLASS::start_difficulty(double hash_rate) const NOEXCEPT
{
    const auto start = options_.start_difficulty > 0.0 ?
        options_.start_difficulty :
        to_rate_difficulty(hash_rate, options_.share_interval_seconds);

    return std::max(options_.minimum_difficulty, start);
}

TEMPLATE
double CLASS::floor(const mining_channel& channel) const NOEXCEPT
{
    return std::max(options_.minimum_difficulty, channel.floor());
}

// The difficulty moves toward the configured share interval (vardiff).
TEMPLATE
void CLASS::retarget(mining_channel& channel) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace system;

    const auto interval = options_.share_interval_seconds;
    if (is_zero(interval))
        return;

    const auto count = channel.shares();
    const auto elapsed = channel.elapsed();
    const auto target = to_floating(interval);
    if (count < retarget_shares &&
        elapsed < target * to_floating(retarget_shares))
        return;

    channel.reset_window();
    const auto observed = elapsed / to_floating(count);
    const auto ratio = observed > 0.0 ?
        std::clamp(target / observed, minimum_ratio, maximum_ratio) :
        maximum_ratio;

    if (ratio > lower_dead_band && ratio < upper_dead_band)
        return;

    const auto difficulty = std::max(floor(channel),
        channel.difficulty() * ratio);

    if (difficulty == channel.difficulty())
        return;

    channel.set_difficulty(difficulty);
    refresh(channel);
}

// Without shares the difficulty cannot retarget, so it decays by the timer.
TEMPLATE
void CLASS::start_decay() NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace std::placeholders;

    const auto interval = options_.share_interval_seconds;
    if (this->stopped() || is_zero(interval))
        return;

    const auto span = network::seconds(interval * decay_intervals);
    decay_timer_->start(this->template bind<CLASS>(&CLASS::handle_decay, _1),
        span);
}

TEMPLATE
void CLASS::handle_decay(const code& ec) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    if (this->stopped() || ec == network::error::operation_canceled)
        return;

    if (ec)
    {
        this->stop(ec);
        return;
    }

    for (const auto& entry: channels_)
    {
        auto& channel = *entry.second;
        if (!is_zero(channel.shares()))
            continue;

        const auto difficulty = std::max(floor(channel),
            channel.difficulty() * minimum_ratio);

        if (difficulty != channel.difficulty())
        {
            channel.set_difficulty(difficulty);
            channel.reset_window();
            refresh(channel);
        }
    }

    start_decay();
}

TEMPLATE
void CLASS::suggest(mining_channel& channel, double difficulty) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    const auto floored = std::max(floor(channel), difficulty);
    if (floored == channel.difficulty())
        return;

    channel.set_difficulty(floored);
    channel.reset_window();
    refresh(channel);
}

// Blocks.
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::submit_mined(const system::chain::header& header, const job& job,
    const system::data_chunk& extranonce) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace system;

    const auto& query = this->archive();
    const auto& links = job.held->links;
    const auto hash = header.hash();
    mined_.emplace(hash, job.held->context.height);

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
            handle_submitted(hash, submission::failed,
                database::error::integrity);
            return;
        }

        sink.write_bytes(tx);
    }

    sink.flush();
    const auto block = to_shared<chain::view::block>(std::move(data), true);
    if (!block->is_valid())
    {
        handle_submitted(hash, submission::failed, database::error::integrity);
        return;
    }

    this->submit_block(to_shared<chain::header>(header), block, this->gate());
}

// The block is the template with the miner's valid header, so a block that
// is not valid is a store integrity fault. One not confirmed is not a fault.
TEMPLATE
void CLASS::handle_submitted(const system::hash_digest& hash,
    submission outcome, const code& ec) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    const auto it = mined_.find(hash);
    const auto height = it == mined_.end() ? zero : it->second;
    if (it != mined_.end())
        mined_.erase(it);

    const auto block = "[" + system::encode_hash(hash) + ":" +
        std::to_string(height) + "]";

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
            this->fault(database::error::integrity);
            break;
        }
    }
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef CLASS
#undef TEMPLATE

} // namespace server
} // namespace libbitcoin

#endif
