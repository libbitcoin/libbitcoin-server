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
#include <bitcoin/server/parsers/btcd_filter.hpp>

#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parsers/bitcoind_script.hpp>

namespace libbitcoin {
namespace server {
namespace btcd {

using namespace system;
using namespace system::chain;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

code filter_keys(hashes& out, const value_t& addresses, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    if (!std::holds_alternative<array_t>(addresses.value()))
        return error::invalid_argument;

    script script{};
    for (const auto& item: std::get<array_t>(addresses.value()))
    {
        if (!std::holds_alternative<string_t>(item.value()))
            return error::invalid_argument;

        const auto value = std::get<string_t>(item.value());
        if (const auto ec = output_script(script, value, p2kh, p2sh, witness))
            return ec;

        out.push_back(script.hash());
    }

    return error::success;
}

code filter_points(points& out, const value_t& outpoints) NOEXCEPT
{
    if (!std::holds_alternative<array_t>(outpoints.value()))
        return error::invalid_argument;

    uint32_t index{};
    hash_digest hash{};
    for (const auto& item: std::get<array_t>(outpoints.value()))
    {
        if (!std::holds_alternative<object_t>(item.value()))
            return error::invalid_argument;

        const auto& fields = std::get<object_t>(item.value());
        const auto hash_it = fields.find("hash");
        const auto index_it = fields.find("index");
        if (hash_it == fields.end() || index_it == fields.end() ||
            !std::holds_alternative<string_t>(hash_it->second.value()) ||
            !std::holds_alternative<number_t>(index_it->second.value()))
            return error::invalid_argument;

        if (!decode_hash(hash, std::get<string_t>(hash_it->second.value())) ||
            !to_integer(index, std::get<number_t>(index_it->second.value())))
            return error::invalid_argument;

        out.emplace_back(hash, index);
    }

    return error::success;
}

code search_keys(hashes& out, const std::string& address, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    data_chunk point{};
    if (decode_base16(point, address) && is_public_key(point))
    {
        const auto hash = bitcoin_short_hash(point);
        out.push_back(script{ script::to_pay_public_key_pattern(
            point) }.hash());
        out.push_back(script{ script::to_pay_key_hash_pattern(
            hash) }.hash());
        return error::success;
    }

    script parsed{};
    if (const auto ec = output_script(parsed, address, p2kh, p2sh, witness))
        return ec;

    out.push_back(parsed.hash());
    return error::success;
}

BC_POP_WARNING()

} // namespace btcd
} // namespace server
} // namespace libbitcoin
