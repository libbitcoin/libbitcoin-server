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
#include <bitcoin/server/parsers/electrum_request.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Interface arity is otherwise enforced by the dispatcher, which drops the
// channel, so tolerating the excess requires that it is discarded here.
bool electrum_handshake_request(request_t& out, const request_t& in) NOEXCEPT
{
    using server_version = interface::electrum_handshake::server_version;

    if (in.method != server_version::name || !in.params)
        return false;

    const auto positional = std::get_if<array_t>(&(*in.params));
    if (!positional || positional->size() <= server_version::size)
        return false;

    out = in;
    std::get<array_t>(*out.params).resize(server_version::size);
    return true;
}

bool electrum_request(request_t& out, const request_t& in,
    electrum::version version) NOEXCEPT
{
    using transaction_get = interface::electrum::blockchain_transaction_get;

    if (version >= electrum::version::v1_1 ||
        in.method != transaction_get::name || !in.params)
        return false;

    // A third argument remains an arity error, as it was.
    const auto positional = std::get_if<array_t>(&(*in.params));
    if (!positional || positional->size() != two)
        return false;

    out = in;
    std::get<array_t>(*out.params).resize(one);
    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
