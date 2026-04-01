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
#include <bitcoin/server/parsers/electrum_version.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace electrum {

std::string_view version_to_string(version value) NOEXCEPT
{
    static const std::unordered_map<version, std::string_view> map
    {
        { version::v0_0,   "0.0" },
        { version::v0_6,   "0.6" },
        { version::v0_8,   "0.8" },
        { version::v0_9,   "0.9" },
        { version::v0_10,  "0.10" },
        { version::v1_0,   "1.0" },
        { version::v1_1,   "1.1" },
        { version::v1_2,   "1.2" },
        { version::v1_3,   "1.3" },
        { version::v1_4,   "1.4" },
        { version::v1_4_1, "1.4.1" },
        { version::v1_4_2, "1.4.2" },
        { version::v1_6,   "1.6" },
        { version::v1_7,   "1.7" }
    };

    const auto it = map.find(value);
    return it != map.end() ? it->second : "0.0";
}

version version_from_string( const std::string_view& value) NOEXCEPT
{
    static const std::unordered_map<std::string_view, version> map
    {
        { "0.0",   version::v0_0 },
        { "0.6",   version::v0_6 },
        { "0.8",   version::v0_8 },
        { "0.9",   version::v0_9 },
        { "0.10",  version::v0_10 },
        { "1.0",   version::v1_0 },
        { "1.1",   version::v1_1 },
        { "1.2",   version::v1_2 },
        { "1.3",   version::v1_3 },
        { "1.4",   version::v1_4 },
        { "1.4.1", version::v1_4_1 },
        { "1.4.2", version::v1_4_2 },
        { "1.6",   version::v1_6 },
        { "1.7",   version::v1_7 }
    };

    const auto it = map.find(value);
    return it != map.end() ? it->second : version::v0_0;
}

} // namespace electrum
} // namespace server
} // namespace libbitcoin
