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
#include <bitcoin/server/parsers/bitcoind_query.hpp>

#include <charconv>
#include <iterator>
#include <variant>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

template <typename Number>
static bool to_number(Number& out, const std::string_view& token) NOEXCEPT
{
    if (token.empty() || (token.size() > one && token.starts_with('0')))
        return false;

    const auto end = std::next(token.data(), token.size());
    const auto result = std::from_chars(token.data(), end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

bool bitcoind_query(rpc::request_t& out, const std::string& target) NOEXCEPT
{
    wallet::uri uri{};
    if (!uri.decode(target))
        return false;

    // Caller must have provided a request.params object.
    if (!out.params.has_value() ||
        !std::holds_alternative<rpc::object_t>(out.params.value()))
        return false;

    auto query = uri.decode_query();
    auto& params = std::get<rpc::object_t>(out.params.value());

    // Count is optional (defaulted by the target parser where applicable).
    if (const auto count = query.find("count"); count != query.end())
    {
        uint32_t value{};
        if (!to_number(value, count->second))
            return false;

        params["count"] = value;
    }

    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
