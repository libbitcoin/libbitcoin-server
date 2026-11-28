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
#include <bitcoin/server/protocols/protocol_stratum_v1_handshake.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_stratum_v1_handshake

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/finished (handshake).
// ----------------------------------------------------------------------------

// Session resumes channel following return from start().
void protocol_stratum_v1_handshake::shake(
    network::result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
    {
        handler(network::error::operation_failed);
        return;
    }

    if (!mining_)
    {
        handler(network::error::oversubscribed);
        return;
    }

    handler_ = move_shared<network::result_handler>(std::move(handler));
    channel_->set_mining(mining_);

    SUBSCRIBE_RPC(handle_mining_subscribe, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_RPC(handle_mining_authorize, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_mining_configure, _1, _2, _3, _4);
    SUBSCRIBE_RPC(handle_mining_suggest_difficulty, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_suggest_target, _1, _2, _3);
    SUBSCRIBE_RPC(handle_mining_extranonce_subscribe, _1, _2);

    if (!is_zero(options_.authorize_timeout_seconds))
        authorize_timer_->start(BIND(handle_timeout, _1));

    protocol_rpc<rpc_interface>::start();
}

void protocol_stratum_v1_handshake::finished(const code& ec,
    const code& shake) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    callback(shake);
}

// Allow channel stop to terminate handshake.
void protocol_stratum_v1_handshake::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    authorize_timer_->stop();
    callback(ec);
    protocol_rpc<rpc_interface>::stopping(ec);
}

// private
// Idempotent on strand, first caller gets handler.
void protocol_stratum_v1_handshake::callback(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!handler_)
        return;

    // Invoke handshake completion, error will result in stopped channel.
    (*handler_)(ec);
    handler_.reset();
}

// Dispatch.
// ----------------------------------------------------------------------------

bool protocol_stratum_v1_handshake::claims(
    const std::string& method) const NOEXCEPT
{
    const auto suggestion =
        method == rpc_interface::mining_suggest_difficulty::name ||
        method == rpc_interface::mining_suggest_target::name;

    if (suggestion && !handler_)
        return false;

    return protocol_rpc<rpc_interface>::claims(method);
}

bool protocol_stratum_v1_handshake::in_band() const NOEXCEPT
{
    return true;
}

void protocol_stratum_v1_handshake::handle_unclaimed(
    const request_t& request) NOEXCEPT
{
    using namespace error::stratum_v1;
    using mining = network::rpc::dispatcher<interface::stratum_v1>;
    BC_ASSERT(stranded());

    if (!handler_)
        return;

    if (!authorized())
    {
        send_code(network::error::unauthorized, network::error::unauthorized);
        return;
    }

    if (!mining::contains(request.method))
    {
        send_error(
        {
            .code = reserved::method_not_found,
            .message = "Method not found"
        });
        return;
    }

    send_code(channel_->subscribed() ? unauthorized_worker : not_subscribed);
}

// Handlers.
// ----------------------------------------------------------------------------

// Miners may send the connected host and port, which are not used.
void protocol_stratum_v1_handshake::handle_mining_subscribe(const code& ec,
    rpc_interface::mining_subscribe, const std::string& user_agent,
    const std::string&, const std::string&, const value_t&) NOEXCEPT
{
    if (stopped(ec))
        return;

    channel_->set_client(user_agent);
    channel_->set_subscribed();
    const auto id = encode_base16(to_big_endian(channel_->identifier()));
    send_result(array_t
    {
        array_t
        {
            array_t{ string_t{ "mining.set_difficulty" }, id },
            array_t{ string_t{ "mining.notify" }, id }
        },
        string_t{ encode_base16(mining_->extranonce()) },
        options_.extranonce2_size
    });

    complete();
}

// The worker name up to any dot is the account, the payout address unless one
// is configured. Absent basic authorization (tcp or ws), a configured
// credential is satisfied here by the account and password.
void protocol_stratum_v1_handshake::handle_mining_authorize(const code& ec,
    rpc_interface::mining_authorize, const std::string& username,
    const std::string& password) NOEXCEPT
{
    using namespace error::stratum_v1;
    using namespace network::config;
    if (stopped(ec))
        return;

    const auto account = username.substr(zero, username.find('.'));
    if (!authorized())
    {
        set_authorized(credential::to_digest(account, password));
        if (!authorized())
        {
            send_code(unauthorized_worker);
            return;
        }
    }

    chain::script payout{};
    const auto& address = options_.payout_address;
    const auto name = address.empty() ? account : address;

    if (output_script(payout, name, p2kh_, p2sh_, witness_) ||
        payout.serialized_size(false) > max_address_script ||
        (channel_->has_payout() && payout != mining_->payout()))
    {
        send_code(unauthorized_worker);
        return;
    }

    // The payout of the channel is set by its first authorized worker.
    if (!channel_->has_payout())
        mining_->set_payout(payout);

    channel_->authorize(username);
    send_result(true);
    complete();
}

// Version rolling (bip310) is the only extension, others are declined.
void protocol_stratum_v1_handshake::handle_mining_configure(const code& ec,
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

void protocol_stratum_v1_handshake::handle_mining_suggest_difficulty(
    const code& ec, rpc_interface::mining_suggest_difficulty,
    double difficulty) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(true);
    mining_->suggest(difficulty, options_.minimum_difficulty);
}

void protocol_stratum_v1_handshake::handle_mining_suggest_target(
    const code& ec, rpc_interface::mining_suggest_target,
    const std::string& target) NOEXCEPT
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
    mining_->suggest(to_share_difficulty(to_uintx(hash)),
        options_.minimum_difficulty);
}

// The extranonce of a channel does not change.
void protocol_stratum_v1_handshake::handle_mining_extranonce_subscribe(
    const code& ec, rpc_interface::mining_extranonce_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    send_result(true);
}

// Completion.
// ----------------------------------------------------------------------------

// private
mining_channel::ptr protocol_stratum_v1_handshake::to_mining(
    const extranonces::prefix& prefix, double difficulty) NOEXCEPT
{
    return prefix ? emplace_shared<mining_channel>(prefix, difficulty) :
        mining_channel::ptr{};
}

// private
// A subscribed and authorized channel completes the handshake (once).
void protocol_stratum_v1_handshake::complete() NOEXCEPT
{
    BC_ASSERT(stranded());
    if (!handler_ || !channel_->subscribed() || !channel_->has_payout())
        return;

    authorize_timer_->stop();
    pause();
    finished(network::error::success, network::error::success);
}

// private
void protocol_stratum_v1_handshake::handle_timeout(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (stopped() || ec == network::error::operation_canceled)
        return;

    stop(ec ? ec : code{ network::error::channel_timeout });
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef CLASS

} // namespace server
} // namespace libbitcoin
