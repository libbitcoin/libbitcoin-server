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
#ifndef LIBBITCOIN_SERVER_PARSERS_ELECTRUM_REQUEST_HPP
#define LIBBITCOIN_SERVER_PARSERS_ELECTRUM_REQUEST_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/electrum.hpp>

namespace libbitcoin {
namespace server {

/// Discard extraneous server.version arguments, which the protocol requires
/// to be tolerated and ignored. False if the request is unmodified.
BCS_API bool electrum_handshake_request(network::rpc::request_t& out,
    const network::rpc::request_t& in) NOEXCEPT;

/// Discard the ignored blockchain.transaction.get height argument below 1.1,
/// which occupies the slot of the 1.2 verbose argument. False if unmodified.
BCS_API bool electrum_request(network::rpc::request_t& out,
    const network::rpc::request_t& in, electrum::version version) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
