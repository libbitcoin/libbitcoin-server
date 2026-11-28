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
#include <bitcoin/server/protocols/protocol_stratum_v2.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_stratum_v2

using namespace system;
using namespace network::messages::sv2;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Share sums are whole units of share difficulty, saturating.
static uint64_t to_shares_sum(double difficulty) NOEXCEPT
{
    constexpr auto maximum = static_cast<double>(max_uint64);
    return difficulty >= maximum ? max_uint64 :
        static_cast<uint64_t>(difficulty);
}

// Target of floor difficulty may exceed maximum.
static hash_digest to_bounded_target(double difficulty,
    const hash_digest& maximum) NOEXCEPT
{
    return from_uintx(std::min(to_share_target(difficulty), to_uintx(maximum)));
}

// Start.
// ----------------------------------------------------------------------------

void protocol_stratum_v2::start() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (started())
        return;

    SUBSCRIBE_CHANNEL(open_standard_mining_channel,
        handle_receive_open_standard_mining_channel, _1, _2);
    SUBSCRIBE_CHANNEL(open_extended_mining_channel,
        handle_receive_open_extended_mining_channel, _1, _2);
    SUBSCRIBE_CHANNEL(update_channel, handle_receive_update_channel, _1, _2);
    SUBSCRIBE_CHANNEL(close_channel, handle_receive_close_channel, _1, _2);
    SUBSCRIBE_CHANNEL(submit_shares_standard,
        handle_receive_submit_shares_standard, _1, _2);
    SUBSCRIBE_CHANNEL(submit_shares_extended,
        handle_receive_submit_shares_extended, _1, _2);

    if (!is_zero(options_.authorize_timeout_seconds))
        authorize_timer_->start(BIND(handle_timeout, _1));

    mining_t::start();
}

void protocol_stratum_v2::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    authorize_timer_->stop();
    mining_t::stopping(ec);
}

void protocol_stratum_v2::handle_timeout(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped() || ec == network::error::operation_canceled)
        return;

    stop(ec ? ec : code{ network::error::channel_timeout });
}

// Channels.
// ----------------------------------------------------------------------------

bool protocol_stratum_v2::handle_receive_open_standard_mining_channel(
    const code& ec, const open_standard_mining_channel::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    open_channel(message->request_id, message->user_identity,
        message->nominal_hash_rate, message->max_target, true);
    return true;
}

bool protocol_stratum_v2::handle_receive_open_extended_mining_channel(
    const code& ec, const open_extended_mining_channel::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    if (message->min_extranonce_size > options_.extranonce_size)
    {
        open_failed(message->request_id, "min-extranonce-size-too-large");
        return true;
    }

    open_channel(message->request_id, message->user_identity,
        message->nominal_hash_rate, message->max_target, false);
    return true;
}

// Miner maximum target floors channel difficulty and bounds channel target.
bool protocol_stratum_v2::handle_receive_update_channel(const code& ec,
    const update_channel::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    const auto id = message->channel_id;
    const auto channel = find(id);
    if (!channel)
    {
        const update_channel_error error{ id, "invalid-channel-id" };
        SEND(error, handle_send, _1);
        return true;
    }

    if (message->max_target == null_hash)
    {
        const update_channel_error error{ id, "max-target-out-of-range" };
        SEND(error, handle_send, _1);
        return true;
    }

    const auto rate = to_rate_difficulty(message->nominal_hash_rate,
        options_.share_interval_seconds);
    const auto automatic = !(options_.start_difficulty > 0.0);
    const auto difficulty = automatic && rate > 0.0 ? rate :
        channel->difficulty();

    channel->set_max_target(message->max_target);
    suggest(*channel, difficulty);

    // Maximum may lower target at unchanged difficulty.
    const auto bounded = to_bounded_target(channel->difficulty(),
        channel->max_target);
    if (bounded != channel->notified_target)
        refresh(*channel);

    return true;
}

bool protocol_stratum_v2::handle_receive_close_channel(const code& ec,
    const close_channel::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    remove_channel(message->channel_id);
    return true;
}

// The identity up to any dot is the payout address, unless one is configured.
void protocol_stratum_v2::open_channel(uint32_t request_id,
    const std::string& identity, float hash_rate,
    const hash_digest& max_target, bool standard) NOEXCEPT
{
    BC_ASSERT(stranded());
    chain::script payout{};
    if (!to_payout(payout, identity))
    {
        open_failed(request_id, "unknown-user");
        return;
    }

    if (max_target == null_hash)
    {
        open_failed(request_id, "max-target-out-of-range");
        return;
    }

    if (mining_channel_count() >= options_.maximum_channels)
    {
        open_failed(request_id, "too-many-channels");
        return;
    }

    const auto prefix = allocate_prefix(standard ?
        options_.coinbase_extranonce_size() : options_.extranonce_prefix_size);
    if (!prefix)
    {
        stop(network::error::oversubscribed);
        return;
    }

    do { ++next_; } while (is_zero(next_) || find_channel(next_));

    const auto id = next_;
    const auto floor = to_hash_difficulty(max_target);
    const auto difficulty = std::max(floor, start_difficulty(hash_rate));
    const auto channel = emplace_shared<channel_state>(prefix, difficulty, id,
        standard);

    channel->set_max_target(max_target);
    channel->set_payout(payout);
    add_channel(id, channel,
    {
        .service = "stratum_v2",
        .transport = miners::to_transport(channel_->noised(),
            channel_->websocket(), channel_->secure()),
        .client = channel_->client(),
        .workers = { identity }
    });
    authorize_timer_->stop();

    const auto target = to_bounded_target(difficulty, max_target);
    channel->notified_target = target;
    if (standard)
    {
        const open_standard_mining_channel_success success
        {
            request_id, id, target, *prefix, {}
        };

        SEND(success, handle_send, _1);
    }
    else
    {
        const open_extended_mining_channel_success success
        {
            request_id, id, target, options_.extranonce_size, *prefix, {}
        };

        SEND(success, handle_send, _1);
    }

    start_jobs();
}

void protocol_stratum_v2::open_failed(uint32_t request_id,
    const std::string& reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    const open_mining_channel_error error{ request_id, reason };
    SEND(error, handle_send, _1);
}

bool protocol_stratum_v2::to_payout(chain::script& out,
    const std::string& identity) const NOEXCEPT
{
    const auto& address = options_.payout_address;
    const auto name = address.empty() ?
        identity.substr(zero, identity.find('.')) : address;

    return !output_script(out, name, p2kh_, p2sh_, witness_) &&
        out.serialized_size(false) <= max_address_script;
}

protocol_stratum_v2::channel_state::ptr protocol_stratum_v2::find(
    uint32_t id) const NOEXCEPT
{
    return std::static_pointer_cast<channel_state>(find_channel(id));
}

// Shares.
// ----------------------------------------------------------------------------

bool protocol_stratum_v2::handle_receive_submit_shares_standard(
    const code& ec, const submit_shares_standard::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    static const data_chunk none{};
    submit_share(
    {
        message->channel_id,
        message->sequence_number,
        message->job_id,
        message->nonce,
        message->ntime,
        message->version,
        none
    }, true);

    return true;
}

bool protocol_stratum_v2::handle_receive_submit_shares_extended(
    const code& ec, const submit_shares_extended::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return false;

    submit_share(
    {
        message->channel_id,
        message->sequence_number,
        message->job_id,
        message->nonce,
        message->ntime,
        message->version,
        message->extranonce
    }, false);

    return true;
}

// A miner that sends a share that cannot be valid is dropped.
void protocol_stratum_v2::submit_share(const share& share,
    bool standard) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto channel = find(share.channel_id);
    if (!channel || channel->standard != standard)
    {
        share_failed(share, "invalid-channel-id");
        return;
    }

    const auto job = channel->find(share.job_id);
    if (!job)
    {
        const auto stale = channel->issued(share.job_id);
        reject_share(*channel, stale ? "stale" : "job_not_found");
        share_failed(share, stale ? "stale-share" : "invalid-job-id");
        return;
    }

    const size_t rolled = standard ? zero : options_.extranonce_size;
    if (share.extranonce.size() != rolled)
    {
        share_dropped(share, "invalid-extranonce-size",
            error::invalid_argument);
        return;
    }

    // Version bits are rolled only within the general purpose bits (bip320).
    const auto bits = bit_xor(share.version, job->version);
    if (!is_zero(bit_and(bits, bit_not(version_rolling_mask))))
    {
        share_dropped(share, "invalid-version", error::invalid_argument);
        return;
    }

    code failed{};
    chain::header header{};
    switch (check_share(*channel, *job, share.version, share.ntime,
        share.nonce, share.extranonce, header, failed))
    {
        case share_outcome::invalid:
        {
            share_dropped(share, "invalid-share", failed);
            return;
        }
        case share_outcome::low_difficulty:
        {
            share_failed(share, "difficulty-too-low");
            return;
        }
        case share_outcome::duplicate:
        {
            share_failed(share, "duplicate-share");
            return;
        }
        case share_outcome::accepted:
        {
            // A share is valid work, whatever the outcome of a block it mines.
            const submit_shares_success success
            {
                share.channel_id, share.sequence_number, 1,
                to_shares_sum(job->difficulty)
            };

            SEND(success, handle_send, _1);
            accept_share(*channel, *job, header, share.extranonce);
            return;
        }
    }
}

void protocol_stratum_v2::share_failed(const share& share,
    const std::string& reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    const submit_shares_error error
    {
        share.channel_id, share.sequence_number, reason
    };

    SEND(error, handle_send, _1);
}

void protocol_stratum_v2::share_dropped(const share& share,
    const std::string& reason, const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    const submit_shares_error error
    {
        share.channel_id, share.sequence_number, reason
    };

    SEND(error, handle_dropped, _1, ec);
}

void protocol_stratum_v2::handle_dropped(const code& ec,
    const code& reason) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped(ec))
        return;

    stop(reason);
}

// Jobs.
// ----------------------------------------------------------------------------

size_t protocol_stratum_v2::extranonce_size() const NOEXCEPT
{
    return options_.coinbase_extranonce_size();
}

// A clean job is a future job, activated by its previous block.
void protocol_stratum_v2::send_job(mining_channel& channel,
    const job::cptr& job, bool clean) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& state = static_cast<channel_state&>(channel);
    const auto bounded = to_bounded_target(job->difficulty, state.max_target);
    if (bounded != state.notified_target)
    {
        state.notified_target = bounded;
        const set_target target{ state.id, bounded };
        SEND(target, handle_send, _1);
    }

    const auto id = channel.issue(job, clean);
    const auto start = clean ? std::optional<uint32_t>{} :
        std::optional<uint32_t>{ job->timestamp };

    if (state.standard)
    {
        const auto coinbase = coinbase_unwitnessed(job->prefix,
            channel.extranonce(), job->suffix);

        const new_mining_job standard
        {
            state.id, id, start, job->version,
            coinbase_root(bitcoin_hash(coinbase), job->held->branch)
        };

        SEND(standard, handle_send, _1);
    }
    else
    {
        const new_extended_mining_job extended
        {
            state.id, id, start, job->version, true, job->held->branch,
            job->prefix, job->suffix
        };

        SEND(extended, handle_send, _1);
    }

    if (clean)
    {
        const set_new_prev_hash previous
        {
            state.id, id, job->previous, job->timestamp, job->bits
        };

        SEND(previous, handle_send, _1);
    }
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef CLASS

} // namespace server
} // namespace libbitcoin
