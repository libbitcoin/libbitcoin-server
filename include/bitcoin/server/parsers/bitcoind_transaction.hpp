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
#ifndef LIBBITCOIN_SERVER_PARSERS_BITCOIND_TRANSACTION_HPP
#define LIBBITCOIN_SERVER_PARSERS_BITCOIND_TRANSACTION_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Parse the createrawtransaction/createpsbt inputs and outputs.
BCS_API code parse_transaction(system::chain::transaction& out,
    const network::rpc::array_t& inputs,
    const network::rpc::value_t& outputs, double locktime, bool replaceable,
    double version, uint8_t p2kh, uint8_t p2sh, const std::string& witness,
    uint64_t maximum) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
