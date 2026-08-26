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
#include <bitcoin/server/parsers/bitcoind_merkle.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

// The number of nodes at the given height (leaves are at height zero).
static size_t tree_width(size_t txs, size_t height) NOEXCEPT
{
    return ceilinged_divide(txs, power2(height));
}

// The merkle node hash of a pair (double sha256 of the concatenation). The
// two-element merkle_root is a single bitcoin merkle level.
static hash_digest node_hash(const hash_digest& left,
    const hash_digest& right) NOEXCEPT
{
    return sha256::merkle_root(hashes{ left, right });
}

// The hash of the subtree rooted at (height, pos) over the block's txids.
static hash_digest calc_hash(size_t height, size_t pos,
    const hashes& txids) NOEXCEPT
{
    if (is_zero(height))
        return txids.at(pos);

    const auto left = calc_hash(sub1(height), pos * two, txids);
    const auto right =
        (pos * two + one) < tree_width(txids.size(), sub1(height)) ?
            calc_hash(sub1(height), pos * two + one, txids) : left;

    return node_hash(left, right);
}

// Depth-first build of the flag bits and branch hashes (bip37).
static void traverse_build(size_t height, size_t pos, const hashes& txids,
    const std::vector<bool>& match, std::vector<bool>& bits,
    hashes& branch) NOEXCEPT
{
    // This node is a parent of a match if any covered leaf is matched.
    auto parent_of_match = false;
    const auto width = power2(height);
    const auto first = pos * width;
    const auto past = std::min((pos + one) * width, txids.size());
    for (auto leaf = first; leaf < past; ++leaf)
        parent_of_match = parent_of_match || match.at(leaf);

    bits.push_back(parent_of_match);
    if (is_zero(height) || !parent_of_match)
    {
        // Leaf, or a subtree with no match: store its hash and stop.
        branch.push_back(calc_hash(height, pos, txids));
        return;
    }

    // Descend into both subtrees (the right may be a duplicate of the left).
    traverse_build(sub1(height), pos * two, txids, match, bits, branch);
    if (pos * two + one < tree_width(txids.size(), sub1(height)))
        traverse_build(sub1(height), pos * two + one, txids, match, bits,
            branch);
}

// Depth-first extract of the root and matched leaves (bip37).
static hash_digest traverse_extract(size_t height, size_t pos, size_t txs,
    const std::vector<bool>& bits, const hashes& branch, size_t& bit,
    size_t& used, hashes& matched, std::vector<size_t>& positions,
    bool& bad) NOEXCEPT
{
    if (bit >= bits.size())
    {
        bad = true;
        return {};
    }

    const auto parent_of_match = bits.at(bit++);
    if (is_zero(height) || !parent_of_match)
    {
        if (used >= branch.size())
        {
            bad = true;
            return {};
        }

        const auto hash = branch.at(used++);

        // A set flag at a leaf is a matched txid.
        if (is_zero(height) && parent_of_match)
        {
            matched.push_back(hash);
            positions.push_back(pos);
        }

        return hash;
    }

    const auto left = traverse_extract(sub1(height), pos * two, txs, bits,
        branch, bit, used, matched, positions, bad);

    hash_digest right{};
    if (pos * two + one < tree_width(txs, sub1(height)))
    {
        right = traverse_extract(sub1(height), pos * two + one, txs, bits,
            branch, bit, used, matched, positions, bad);

        // Sibling subtrees cover disjoint txids, so cannot be identical.
        if (right == left)
            bad = true;
    }
    else
    {
        right = left;
    }

    return node_hash(left, right);
}

static std::vector<bool> bytes_to_bits(const data_chunk& bytes) NOEXCEPT
{
    std::vector<bool> bits(bytes.size() * byte_bits);
    for (size_t bit{}; bit < bits.size(); ++bit)
        bits.at(bit) = get_right(bytes.at(to_floored_bytes(bit)),
            bit % byte_bits);

    return bits;
}

static data_chunk bits_to_bytes(const std::vector<bool>& bits) NOEXCEPT
{
    data_chunk bytes(to_ceilinged_bytes(bits.size()));
    for (size_t bit{}; bit < bits.size(); ++bit)
        set_right_into(bytes.at(to_floored_bytes(bit)), bit % byte_bits,
            bits.at(bit));

    return bytes;
}

// The tree height that reduces the leaves to a single root.
static size_t tree_height(size_t txs) NOEXCEPT
{
    size_t height{};
    while (tree_width(txs, height) > one)
        ++height;

    return height;
}

void build_partial_merkle(data_chunk& flags, hashes& branch,
    const hashes& txids, const std::vector<bool>& match) NOEXCEPT
{
    branch.clear();
    std::vector<bool> bits{};
    traverse_build(tree_height(txids.size()), zero, txids, match, bits, branch);
    flags = bits_to_bytes(bits);
}

bool extract_partial_merkle(hash_digest& root, hashes& matched,
    std::vector<size_t>& positions, size_t txs, const data_chunk& flags,
    const hashes& branch) NOEXCEPT
{
    matched.clear();
    positions.clear();

    // An empty tree, or more hashes than leaves, is malformed. There must be
    // at least one flag bit per branch hash.
    const auto bits = bytes_to_bits(flags);
    if (is_zero(txs) || branch.size() > txs || bits.size() < branch.size())
        return false;

    auto bad = false;
    size_t bit{}, used{};
    root = traverse_extract(tree_height(txs), zero, txs, bits, branch, bit,
        used, matched, positions, bad);

    // All hashes must be consumed, and all bits but the final byte padding.
    return !bad && used == branch.size() &&
        to_ceilinged_bytes(bit) == to_ceilinged_bytes(bits.size());
}

} // namespace server
} // namespace libbitcoin
