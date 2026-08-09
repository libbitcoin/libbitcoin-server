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
#include "../test.hpp"

BOOST_AUTO_TEST_SUITE(partial_merkle_tests)

using namespace system;

// Distinct leaf hashes (value in the last byte), for a block of the given size.
static hashes make_txids(size_t count) NOEXCEPT
{
    hashes out(count);
    for (size_t index{}; index < count; ++index)
        out.at(index)[0] = possible_narrow_cast<uint8_t>(add1(index));

    return out;
}

// The real merkle root of the leaves, independent of the partial tree.
static hash_digest full_root(const hashes& txids) NOEXCEPT
{
    return sha256::merkle_root(hashes{ txids });
}

// A partial tree built for the given matches extracts to the real root, and
// recovers exactly the matched txids and their positions.
static void verify_round_trip(size_t count,
    const std::vector<size_t>& matches) NOEXCEPT
{
    const auto txids = make_txids(count);
    std::vector<bool> match(count);
    for (const auto position: matches)
        match.at(position) = true;

    data_chunk flags{};
    hashes branch{};
    server::build_partial_merkle(flags, branch, txids, match);

    hash_digest root{};
    hashes matched{};
    std::vector<size_t> positions{};
    BOOST_REQUIRE(server::extract_partial_merkle(root, matched, positions, count, flags, branch));

    BOOST_REQUIRE_EQUAL(root, full_root(txids));
    BOOST_REQUIRE_EQUAL(positions.size(), matches.size());

    for (size_t index{}; index < matches.size(); ++index)
    {
        BOOST_REQUIRE_EQUAL(positions.at(index), matches.at(index));
        BOOST_REQUIRE_EQUAL(matched.at(index), txids.at(matches.at(index)));
    }
}

// round trip
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(partial_merkle__single_tx__matched__root)
{
    verify_round_trip(1, { 0 });
}

BOOST_AUTO_TEST_CASE(partial_merkle__seven_tx__two_matched__root_and_positions)
{
    verify_round_trip(7, { 1, 4 });
}

BOOST_AUTO_TEST_CASE(partial_merkle__eight_tx__interior_matched__root)
{
    verify_round_trip(8, { 0, 3, 7 });
}

BOOST_AUTO_TEST_CASE(partial_merkle__odd_tx__last_matched__root)
{
    verify_round_trip(5, { 4 });
}

BOOST_AUTO_TEST_CASE(partial_merkle__large__scattered__root)
{
    verify_round_trip(100, { 0, 1, 50, 98, 99 });
}

// A branch smaller than a single-tx tree extracts the leaf as the root.
BOOST_AUTO_TEST_CASE(partial_merkle__single_tx__is_leaf_root)
{
    const auto txids = make_txids(1);
    data_chunk flags{};
    hashes branch{};
    server::build_partial_merkle(flags, branch, txids, { true });
    BOOST_REQUIRE_EQUAL(branch.size(), 1u);
    BOOST_REQUIRE_EQUAL(branch.front(), txids.front());
}

// malformed
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(partial_merkle__zero_tx__false)
{
    hash_digest root{};
    hashes matched{};
    std::vector<size_t> positions{};
    BOOST_REQUIRE(!server::extract_partial_merkle(root, matched, positions, 0, { 0x00 }, { one_hash }));
}

// More branch hashes than transactions is malformed.
BOOST_AUTO_TEST_CASE(partial_merkle__excess_hashes__false)
{
    hash_digest root{};
    hashes matched{};
    std::vector<size_t> positions{};
    BOOST_REQUIRE(!server::extract_partial_merkle(root, matched, positions, 1, { 0x00 }, { one_hash, one_hash }));
}

BOOST_AUTO_TEST_SUITE_END()
