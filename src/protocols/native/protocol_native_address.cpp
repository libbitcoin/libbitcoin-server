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
#include <bitcoin/server/protocols/protocol_native.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_native

using namespace system;

BC_PUSH_WARNING(NO_INCOMPLETE_SWITCH)

// handle_get_address
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address(const code& ec, interface::address,
    uint8_t, uint8_t media, const hash_cptr& hash, bool turbo) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_address, media, turbo, hash);
    return true;
}

// private
void protocol_native::do_get_address(uint8_t media, bool turbo,
    const hash_cptr& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    database::outpoints set{};
    const auto& query = archive();
    const auto ec = query.get_address_outpoints(stopping_, set, *hash, turbo);
    POST(complete_get_address, ec, media, std::move(set));
}

// This is shared by the three get_address... methods.
void protocol_native::complete_get_address(const code& ec, uint8_t media,
    const database::outpoints& set) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Stop monitoring socket.
    monitor(false);

    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    if (set.empty())
    {
        send_not_found();
        return;
    }

    const auto size = set.size() * chain::outpoint::serialized_size();
    switch (media)
    {
        case data:
            send_chunk(to_bin_array(set, size));
            return;
        case text:
            send_text(to_hex_array(set, size));
            return;
        case json:
            send_json(value_from(set), two * size);
            return;
    }

    send_not_found();
}

// handle_get_address_confirmed
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address_confirmed(const code& ec,
    interface::address_confirmed, uint8_t, uint8_t media,
    const hash_cptr& hash, bool turbo) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    if (!archive().address_enabled())
    {
        send_not_implemented();
        return true;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_address_confirmed, media, turbo, hash);
    return true;
}

// private
void protocol_native::do_get_address_confirmed(uint8_t media, bool turbo,
    const hash_cptr& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    database::outpoints set{};
    const auto& query = archive();
    auto ec = query.get_confirmed_unspent_outpoints(stopping_, set, *hash,
        turbo);
    POST(complete_get_address, ec, media, std::move(set));
}

// handle_get_address_unconfirmed
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address_unconfirmed(const code& ec,
    interface::address_unconfirmed, uint8_t, uint8_t,
    const hash_cptr&, bool) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // TODO: there are currently no unconfirmed txs.
    send_not_implemented();
    return true;
}

// handle_get_address_balance
// ----------------------------------------------------------------------------

bool protocol_native::handle_get_address_balance(const code& ec,
    interface::address_balance, uint8_t, uint8_t media,
    const hash_cptr& hash, bool turbo) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    const auto& query = archive();
    if (!query.address_enabled())
    {
        send_not_implemented();
        return true;
    }

    // Monitor socket for close.
    monitor(true);

    PARALLEL(do_get_address_balance, media, turbo, hash);
    return true;
}

void protocol_native::do_get_address_balance(uint8_t media, bool turbo,
    const hash_cptr& hash) NOEXCEPT
{
    BC_ASSERT(!stranded());

    uint64_t balance{};
    const auto& query = archive();
    auto ec = query.get_confirmed_balance(stopping_, balance, *hash, turbo);
    POST(complete_get_address_balance, ec, media, balance);
}

void protocol_native::complete_get_address_balance(const code& ec,
    uint8_t media, uint64_t balance) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Stop monitoring socket.
    monitor(false);

    // Suppresses cancelation error response.
    if (stopped())
        return;

    if (ec)
    {
        send_internal_server_error(ec);
        return;
    }

    switch (media)
    {
        case data:
            send_chunk(to_little_endian_size(balance));
            return;
        case text:
            send_text(encode_base16(to_little_endian_size(balance)));
            return;
        case json:
            send_json(balance, two * sizeof(balance));
            return;
    }

    send_not_found();
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
