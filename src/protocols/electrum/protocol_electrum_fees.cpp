/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

using mode_t = node::estimator::mode;
mode_t mode_from_string(const std::string& mode) NOEXCEPT
{
    if (mode.empty()) return mode_t::basic;
    if (mode == "basic") return mode_t::basic;
    if (mode == "geometric") return mode_t::geometric;
    if (mode == "economical") return mode_t::economical;
    if (mode == "conservative") return mode_t::conservative;
    return mode_t::unknown;
}

void protocol_electrum::handle_blockchain_estimate_fee(const code& ec,
    rpc_interface::blockchain_estimate_fee, double number,
    const std::string& mode) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t target{};
    if (!to_integer(target, number))
    {
        send_code(error::invalid_argument);
        return;
    }

    if (!mode.empty() && !at_least(electrum::version::v1_6))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto mode_ = mode_from_string(mode);
    if (mode_ == mode_t::unknown)
    {
        send_code(error::invalid_argument);
        return;
    }

    estimate(target, mode_, BIND(handle_estimate_fee, _1, _2));
}

void protocol_electrum::handle_estimate_fee(const code& ec,
    uint64_t fee) NOEXCEPT
{
    POST(complete_estimate_fee, ec, fee);
}

void protocol_electrum::complete_estimate_fee(const code& ec,
    uint64_t fee) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    const auto disabled =
        ec == node::error::estimate_false ||
        ec == node::error::estimate_disabled ||
        ec == node::error::estimate_premature;

    if (!disabled && ec)
    {
        // node::error::estimates_failed, implies store fault.
        send_code(error::server_error);
        return;
    }

    // If not enough information to make an estimate, -1 is returned.
    send_result(disabled ? -1 : possible_narrow_sign_cast<int64_t>(fee), 42);
}

void protocol_electrum::handle_blockchain_relay_fee(const code& ec,
    rpc_interface::blockchain_relay_fee) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0) ||
         at_least(electrum::version::v1_6))
    {
        send_code(error::wrong_version);
        return;
    }

    send_result(node_settings().minimum_fee_rate, 42);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
