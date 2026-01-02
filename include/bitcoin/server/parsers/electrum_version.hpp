/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

enum class electrum_version
{
    /// Invalid version.
    v0_0,

    /// 2011, initial protocol negotiation.
    v0_6,

    /// 2012, enhanced protocol negotiation.
    v0_8,

    /// 2012, added pruning limits and transport indicators.
    v0_9,

    /// 2013, baseline for core methods in the official specification.
    v0_10,

    /// 2014, 1.x series, deprecations of utxo and block number methods.
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

    /// 2022, updated response formats and added fee estimation modes.
    v1_6
};

} // namespace server
} // namespace libbitcoin

#endif
