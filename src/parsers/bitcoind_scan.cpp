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
#include <bitcoin/server/parsers/bitcoind_scan.hpp>

#include <iterator>
#include <variant>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A scan object is a descriptor string or { "desc", "range" } object.
bool expand_scan_object(chain::scripts& out, const value_t& item) NOEXCEPT
{
    wallet::descriptor::signing::list signings{};
    if (!expand_scan_signings(signings, item))
        return false;

    out.reserve(out.size() + signings.size());
    for (auto& derived: signings)
        out.push_back(std::move(derived.script));

    return true;
}

bool expand_scan_signings(wallet::descriptor::signing::list& out,
    const value_t& item) NOEXCEPT
{
    std::string expression{};
    uint32_t begin{};
    uint32_t end{};

    // bitcoind's default and maximum ranges for ranged descriptors.
    constexpr uint32_t default_range = 1'000;
    constexpr uint32_t maximum_range = 1'000'000;

    if (std::holds_alternative<string_t>(item.value()))
    {
        expression = std::get<string_t>(item.value());
        end = default_range;
    }
    else if (std::holds_alternative<object_t>(item.value()))
    {
        const auto& fields = std::get<object_t>(item.value());
        const auto desc = fields.find("desc");
        if (desc == fields.end() ||
            !std::holds_alternative<string_t>(desc->second.value()))
            return false;

        expression = std::get<string_t>(desc->second.value());
        end = default_range;
        const auto range = fields.find("range");
        if (range != fields.end() &&
            !parse_scan_range(begin, end, range->second))
            return false;
    }
    else
    {
        return false;
    }

    const wallet::descriptor parsed{ expression };
    if (!parsed || to_bool(shift_right(end, 31u)) ||
        floored_subtract(end, begin) >= maximum_range)
        return false;

    if (!parsed.ranged())
        end = begin;

    for (auto index = begin; index <= end; ++index)
    {
        auto derived = parsed.signings(index);
        if (derived.empty())
            return false;

        out.insert(out.end(), std::make_move_iterator(derived.begin()),
            std::make_move_iterator(derived.end()));
    }

    return true;
}

bool parse_scan_range(uint32_t& begin, uint32_t& end,
    const value_t& range) NOEXCEPT
{
    const auto& value = range.value();
    if (std::holds_alternative<number_t>(value))
        return to_integer(end, std::get<number_t>(value));

    if (!std::holds_alternative<array_t>(value))
        return false;

    const auto& pair = std::get<array_t>(value);
    return pair.size() == two &&
        std::holds_alternative<number_t>(pair.front().value()) &&
        std::holds_alternative<number_t>(pair.back().value()) &&
        to_integer(begin, std::get<number_t>(pair.front().value())) &&
        to_integer(end, std::get<number_t>(pair.back().value())) &&
        end >= begin;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
