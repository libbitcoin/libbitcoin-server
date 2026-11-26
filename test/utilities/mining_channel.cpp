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

BC_AUTO_TEST_SUITE(mining_channel_tests)

using namespace system;
using job = server::mining_channel::job;

static const server::extranonces::prefix prefix{ to_shared(data_chunk{ 0x2a, 0x00, 0x00, 0x00 }) };

// A job distinguished by its version.
static job::cptr make_job(uint32_t version) NOEXCEPT
{
    return to_shared(job{ .version = version });
}

// Issue jobs of versions [first, first + count), unclean.
static void issue_jobs(server::mining_channel& channel, uint32_t first,
    size_t count) NOEXCEPT
{
    for (size_t index{}; index < count; ++index)
        channel.issue(make_job(first + possible_narrow_cast<uint32_t>(index)),
            false);
}

static hash_digest hash_of(uint8_t value) NOEXCEPT
{
    hash_digest out{};
    out.front() = value;
    return out;
}

BC_AUTO_TEST_CASE(mining_channel__construct__difficulty__expected)
{
    const server::mining_channel channel{ prefix, 2.5 };
    BC_REQUIRE_EQUAL(channel.extranonce(), *prefix);
    BC_REQUIRE_EQUAL(channel.difficulty(), 2.5);
    BC_REQUIRE_EQUAL(channel.notified(), 0.0);
    BC_REQUIRE_EQUAL(channel.accepted(), 0u);
    BC_REQUIRE_EQUAL(channel.shares(), 0u);
    BC_REQUIRE(!channel.current());
    BC_REQUIRE(channel.payout().ops().empty());
}

BC_AUTO_TEST_CASE(mining_channel__issue__sequential__identifiers_and_current)
{
    server::mining_channel channel{ prefix, 1.0 };
    const auto first = channel.issue(make_job(10), false);
    const auto second = channel.issue(make_job(11), false);
    BC_REQUIRE_EQUAL(first, 0u);
    BC_REQUIRE_EQUAL(second, 1u);
    BC_REQUIRE_EQUAL(channel.current()->version, 11u);
    BC_REQUIRE_EQUAL(channel.find(0)->version, 10u);
    BC_REQUIRE_EQUAL(channel.find(1)->version, 11u);
    BC_REQUIRE(!channel.find(2));
}

BC_AUTO_TEST_CASE(mining_channel__issue__clean__prior_jobs_dropped)
{
    server::mining_channel channel{ prefix, 1.0 };
    channel.issue(make_job(10), false);
    BC_REQUIRE_EQUAL(channel.issue(make_job(11), true), 1u);
    BC_REQUIRE(!channel.find(0));
    BC_REQUIRE_EQUAL(channel.find(1)->version, 11u);
}

BC_AUTO_TEST_CASE(mining_channel__issue__above_maximum__oldest_dropped)
{
    server::mining_channel channel{ prefix, 1.0 };
    issue_jobs(channel, 100, add1(server::mining_channel::maximum_jobs));
    BC_REQUIRE(!channel.find(0));
    BC_REQUIRE_EQUAL(channel.find(1)->version, 101u);
    BC_REQUIRE_EQUAL(channel.current()->version, 116u);
}

BC_AUTO_TEST_CASE(mining_channel__accept__distinct__counted)
{
    server::mining_channel channel{ prefix, 1.0 };
    const auto first = channel.accept(hash_of(1));
    const auto second = channel.accept(hash_of(2));
    BC_REQUIRE(first);
    BC_REQUIRE(second);
    BC_REQUIRE_EQUAL(channel.accepted(), 2u);
    BC_REQUIRE_EQUAL(channel.shares(), 2u);
}

BC_AUTO_TEST_CASE(mining_channel__accept__repeated__false_not_counted)
{
    server::mining_channel channel{ prefix, 1.0 };
    const auto first = channel.accept(hash_of(1));
    const auto second = channel.accept(hash_of(1));
    BC_REQUIRE(first);
    BC_REQUIRE(!second);
    BC_REQUIRE_EQUAL(channel.accepted(), 1u);
    BC_REQUIRE_EQUAL(channel.shares(), 1u);
}

BC_AUTO_TEST_CASE(mining_channel__accept__repeated_across_reissue__false)
{
    server::mining_channel channel{ prefix, 1.0 };
    channel.issue(make_job(10), false);
    const auto first = channel.accept(hash_of(1));
    channel.issue(make_job(10), false);
    const auto second = channel.accept(hash_of(1));
    BC_REQUIRE(first);
    BC_REQUIRE(!second);
}

BC_AUTO_TEST_CASE(mining_channel__accept__repeated_after_clean__true)
{
    server::mining_channel channel{ prefix, 1.0 };
    channel.issue(make_job(10), false);
    const auto first = channel.accept(hash_of(1));
    channel.issue(make_job(11), true);
    const auto second = channel.accept(hash_of(1));
    BC_REQUIRE(first);
    BC_REQUIRE(second);
    BC_REQUIRE_EQUAL(channel.accepted(), 2u);
}

BC_AUTO_TEST_CASE(mining_channel__reset_window__shares_cleared_accepted_retained)
{
    server::mining_channel channel{ prefix, 1.0 };
    const auto accepted = channel.accept(hash_of(1));
    channel.reset_window();
    BC_REQUIRE(accepted);
    BC_REQUIRE_EQUAL(channel.shares(), 0u);
    BC_REQUIRE_EQUAL(channel.accepted(), 1u);
}

BC_AUTO_TEST_CASE(mining_channel__set_difficulty__set__expected)
{
    server::mining_channel channel{ prefix, 1.0 };
    channel.set_difficulty(4.0);
    channel.set_notified(2.0);
    BC_REQUIRE_EQUAL(channel.difficulty(), 4.0);
    BC_REQUIRE_EQUAL(channel.notified(), 2.0);
}

BC_AUTO_TEST_CASE(mining_channel__set_payout__set__expected)
{
    server::mining_channel channel{ prefix, 1.0 };
    const chain::script payout{ chain::script::to_pay_key_hash_pattern(short_hash{ 0x42 }) };
    channel.set_payout(payout);
    BC_REQUIRE(channel.payout() == payout);
}

BC_AUTO_TEST_SUITE_END()
