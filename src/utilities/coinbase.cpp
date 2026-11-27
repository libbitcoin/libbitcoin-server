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
#include <bitcoin/server/utilities/coinbase.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace system::chain;

// The height push carries up to four bytes of height (bip34).
constexpr size_t height_push = add1(sizeof(uint32_t));

// The commitment script is op_return, push, head and commitment (bip141).
constexpr size_t commitment_script = two + sizeof(witness_head) + hash_size;

// Version, input count, prevout, script size, sequence, output count, two
// output values and script sizes, commitment script, locktime.
constexpr size_t fixed_size = sizeof(uint32_t) + one + point::serialized_size()
    + one + sizeof(uint32_t) + one + two * (sizeof(uint64_t) + one)
    + commitment_script + sizeof(uint32_t);

constexpr uint32_t coinbase_version = 2;
constexpr uint8_t witness_marker = 0x00;
constexpr uint8_t witness_flag = 0x01;

size_t coinbase_script_size(size_t tag, size_t extranonce) NOEXCEPT
{
    const auto tagged = is_zero(tag) ? zero : add1(tag);
    return height_push + tagged + add1(extranonce);
}

size_t coinbase_weight(size_t script, size_t payout) NOEXCEPT
{
    const auto base = fixed_size + script + payout;
    const auto total = base + coinbase_witness_size;
    return base_size_contribution * base + total_size_contribution * total;
}

// The height is a minimal push (bip34), so the script size varies with it.
data_chunk coinbase_prefix(size_t height, const std::string& tag,
    size_t extranonce) NOEXCEPT
{
    using namespace machine::number;
    const operation push{ chunk::from_integer(to_signed(height)), true };
    const auto tagged = !tag.empty();
    const auto tagging = tagged ? add1(tag.size()) : zero;
    const auto script = push.serialized_size() + tagging + add1(extranonce);

    data_chunk out{};
    write::bytes::data sink{ out };
    sink.write_4_bytes_little_endian(coinbase_version);
    sink.write_variable(one);
    sink.write_bytes(null_hash);
    sink.write_4_bytes_little_endian(point::null_index);
    sink.write_variable(script);
    push.to_data(sink);
    if (tagged)
    {
        sink.write_byte(narrow_cast<uint8_t>(tag.size()));
        sink.write_bytes(to_chunk(tag));
    }

    sink.write_byte(narrow_cast<uint8_t>(extranonce));
    sink.flush();
    return out;
}

// The sequence is not final and the locktime is the prior height (bip54).
data_chunk coinbase_suffix(size_t height, uint64_t value,
    const script& payout) NOEXCEPT
{
    const auto locktime = possible_narrow_cast<uint32_t>(sub1(height));

    data_chunk out{};
    write::bytes::data sink{ out };
    sink.write_4_bytes_little_endian(sub1(max_uint32));
    sink.write_variable(one);
    sink.write_8_bytes_little_endian(value);
    payout.to_data(sink, true);
    sink.write_4_bytes_little_endian(locktime);
    sink.flush();
    return out;
}

data_chunk coinbase_suffix(size_t height, uint64_t value,
    const script& payout, const hash_digest& commitment) NOEXCEPT
{
    const auto locktime = possible_narrow_cast<uint32_t>(sub1(height));

    data_chunk out{};
    write::bytes::data sink{ out };
    sink.write_4_bytes_little_endian(sub1(max_uint32));
    sink.write_variable(two);
    sink.write_8_bytes_little_endian(value);
    payout.to_data(sink, true);
    sink.write_8_bytes_little_endian(zero);
    sink.write_variable(commitment_script);
    sink.write_byte(static_cast<uint8_t>(opcode::op_return));
    sink.write_byte(static_cast<uint8_t>(opcode::push_size_36));
    sink.write_4_bytes_big_endian(witness_head);
    sink.write_bytes(commitment);
    sink.write_4_bytes_little_endian(locktime);
    sink.flush();
    return out;
}

data_chunk coinbase_unwitnessed(const data_chunk& prefix,
    const data_chunk& extranonce, const data_chunk& suffix) NOEXCEPT
{
    return splice(splice(prefix, extranonce), suffix);
}

// The version leads the prefix and the locktime ends the suffix.
data_chunk coinbase_witnessed(const data_chunk& prefix,
    const data_chunk& extranonce, const data_chunk& suffix) NOEXCEPT
{
    constexpr auto field = sizeof(uint32_t);
    BC_ASSERT(prefix.size() >= field && suffix.size() >= field);
    const auto version = std::next(prefix.begin(), field);
    const auto locktime = std::prev(suffix.end(), field);

    data_chunk out{};
    out.reserve(prefix.size() + extranonce.size() + suffix.size() +
        coinbase_witness_size);

    out.insert(out.end(), prefix.begin(), version);
    out.push_back(witness_marker);
    out.push_back(witness_flag);
    out.insert(out.end(), version, prefix.end());
    out.insert(out.end(), extranonce.begin(), extranonce.end());
    out.insert(out.end(), suffix.begin(), locktime);
    out.push_back(one);
    out.push_back(narrow_cast<uint8_t>(hash_size));
    out.insert(out.end(), null_hash.begin(), null_hash.end());
    out.insert(out.end(), locktime, suffix.end());
    return out;
}

// The coinbase is the leftmost leaf, so each branch hash is its right.
hash_digest coinbase_root(const hash_digest& txid,
    const hashes& branch) NOEXCEPT
{
    auto root = txid;
    for (const auto& hash: branch)
        root = sha256::double_hash(root, hash);

    return root;
}

} // namespace server
} // namespace libbitcoin
