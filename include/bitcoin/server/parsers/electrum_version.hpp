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
#ifndef LIBBITCOIN_SERVER_PARSERS_ELECTRUM_VERSION_HPP
#define LIBBITCOIN_SERVER_PARSERS_ELECTRUM_VERSION_HPP

#include <unordered_map>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace electrum {

enum class version
{
    /// Invalid version.
    v0_0,

    /// 2011, initial protocol negotiation (usupported).
    v0_6,

    /// 2012, enhanced protocol negotiation (usupported).
    v0_8,

    /// 2012, added pruning limits and transport indicators (usupported).
    v0_9,

    /// 2013, baseline for core methods in official specification (usupported).
    v0_10,

    /// 2014, deprecations of utxo and block number methods (minimum).
    v1_0,

    /// 2015, updated version response and introduced scripthash methods.
    v1_1,

    /// 2017, added optional parameters for transactions and headers.
    v1_2,

    /// 2018, defaulted raw headers and introduced new block methods.
    v1_3,

    /// 2019, removed deserialized headers and added merkle proof features.
    v1_4,

    /// 2019, modifications for auxiliary proof-of-work handling.
    v1_4_1,

    /// 2020, added scripthash unsubscribe functionality.
    v1_4_2,

    /// There is no v1.5 release (skipped).
    //v1_5,

    /// 2022, updated response formats, added fee estimation modes (maximum).
    v1_6,

    /// Not valid, just defined for out of bounds testing.
    v1_7
};

std::string_view version_to_string(version value) NOEXCEPT;
version version_from_string(const std::string_view& value) NOEXCEPT;

} // namespace electrum
} // namespace server
} // namespace libbitcoin

#endif
