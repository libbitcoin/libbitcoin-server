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
#include <bitcoin/server/utilities/electrum_version.hpp>

#include <algorithm>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {
namespace electrum {

using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Defined versions with their numeric forms, in ascending order.
static const auto& versions() NOEXCEPT
{
    static const std::array<std::pair<version, system::config::version>, 14>
    map
    {{
        { version::v0_0,   { 0, 0 } },
        { version::v0_6,   { 0, 6 } },
        { version::v0_8,   { 0, 8 } },
        { version::v0_9,   { 0, 9 } },
        { version::v0_10,  { 0, 10 } },
        { version::v1_0,   { 1, 0 } },
        { version::v1_1,   { 1, 1 } },
        { version::v1_2,   { 1, 2 } },
        { version::v1_3,   { 1, 3 } },
        { version::v1_4,   { 1, 4 } },
        { version::v1_4_1, { 1, 4, 1 } },
        { version::v1_4_2, { 1, 4, 2 } },
        { version::v1_6,   { 1, 6 } },
        { version::v1_7,   { 1, 7 } }
    }};

    return map;
}

system::config::version version_to_number(version value) NOEXCEPT
{
    const auto& map = versions();
    const auto it = std::find_if(map.begin(), map.end(),
        [=](const auto& entry) NOEXCEPT
        {
            return entry.first == value;
        });

    return it != map.end() ? it->second : system::config::version{};
}

std::string version_to_string(version value) NOEXCEPT
{
    return version_to_number(value).to_string();
}

bool version_from_string(system::config::version& out,
    const std::string_view& value) NOEXCEPT
{
    // Stream extraction tolerates sign, whitespace and trailing garbage, and
    // split elides empty segments, so require well-formed dotted digits.
    auto result = !value.empty()
        && value.front() != '.'
        && value.back() != '.'
        && value.find("..") == std::string_view::npos
        && std::all_of(value.begin(), value.end(), [](char character) NOEXCEPT
        {
            return system::is_ascii_number(character) || character == '.';
        });

    if (result)
    {
        try
        {
            out = system::config::version{ std::string{ value } };
        }
        catch (const std::exception&)
        {
            result = false;
        }
    }

    return result;
}

version version_floor(const system::config::version& value) NOEXCEPT
{
    const auto& map = versions();
    const auto it = std::find_if(map.rbegin(), map.rend(),
        [&](const auto& entry) NOEXCEPT
        {
            return entry.second <= value;
        });

    return it != map.rend() ? it->first : version::v0_0;
}

bool version_range(system::config::version& min,
    system::config::version& max, const value_t& version) NOEXCEPT
{
    const auto& value = version.value();

    if (std::holds_alternative<string_t>(value))
    {
        if (!version_from_string(min, std::get<string_t>(value)))
            return false;

        max = min;
        return true;
    }

    if (std::holds_alternative<array_t>(value))
    {
        const auto& versions = std::get<array_t>(value);
        if (versions.size() != two)
            return false;

        const auto& low = versions.at(0).value();
        const auto& high = versions.at(1).value();
        if (!std::holds_alternative<string_t>(low) ||
            !std::holds_alternative<string_t>(high))
            return false;

        return version_from_string(min, std::get<string_t>(low)) &&
            version_from_string(max, std::get<string_t>(high));
    }

    return false;
}

// Clients may specify undefined (e.g. future) versions, negotiation is
// numeric and settles on the greatest defined version in the overlap.
version negotiate(const value_t& value, const system::config::version& minimum,
    const system::config::version& maximum) NOEXCEPT
{
    system::config::version min{};
    system::config::version max{};
    if (!version_range(min, max, value))
        return version::v0_0;

    const auto lower = std::max(min, minimum);
    const auto upper = std::min(max, maximum);
    const auto floor = version_floor(upper);
    return version_to_number(floor) < lower ? version::v0_0 : floor;
}

std::string escape_client(const std::string& in) NOEXCEPT
{
    std::string out(in.size(), '*');
    std::transform(in.begin(), in.end(), out.begin(), [](char c) NOEXCEPT
    {
        return system::is_ascii_character(c) &&
            !system::is_ascii_whitespace(c) ? c : '*';
    });

    return out;
}

// The 1.0 result is the server software string alone.
value_t version_result(version negotiated,
    const std::string& server_name) NOEXCEPT
{
    if (negotiated < version::v1_1)
        return { server_name };

    return array_t{ server_name, version_to_string(negotiated) };
}

BC_POP_WARNING()

} // namespace electrum
} // namespace server
} // namespace libbitcoin
