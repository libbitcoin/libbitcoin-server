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

BC_AUTO_TEST_SUITE(miners_tests)

using namespace system;
using miners = server::miners;

constexpr uint64_t start = 1'764'200'000'000;
constexpr uint64_t minute = 60'000;
constexpr double one_share_rate = 4'294'967'296.0 * 1'000.0 / 60'000.0;

static miners::device make_device(const std::string& id) NOEXCEPT
{
    return
    {
        .id = id,
        .service = "stratum_v1",
        .transport = "tcp",
        .address = "[127.0.0.1]:42",
        .client = "client",
        .workers = { "worker" },
        .connected = start,
        .difficulty = 1.0
    };
}

static miners::share make_share(uint64_t time, const std::string& result) NOEXCEPT
{
    return { time, 42, 1.0, result };
}

// Accepted shares of device "a", one each millisecond after the start.
static uint64_t add_shares(miners& registry, uint64_t count) NOEXCEPT
{
    for (uint64_t share = 1; share <= count; ++share)
        registry.add_share("a", make_share(start + share, "accepted"), true);

    return count;
}

// Blocks of device "a", of heights one to the count.
static size_t add_blocks(miners& registry, size_t count) NOEXCEPT
{
    for (size_t height = 1; height <= count; ++height)
        registry.add_block({ {}, height, start, "a" });

    return count;
}

// to_id

BC_AUTO_TEST_CASE(miners__to_id__connection_channel__expected)
{
    BC_REQUIRE_EQUAL(miners::to_id(0x0102, 3), "0000000000000102:3");
}

// to_transport

BC_AUTO_TEST_CASE(miners__to_transport__all__expected)
{
    BC_REQUIRE_EQUAL(miners::to_transport(true, true, true), "noise");
    BC_REQUIRE_EQUAL(miners::to_transport(false, true, true), "wss");
    BC_REQUIRE_EQUAL(miners::to_transport(false, true, false), "ws");
    BC_REQUIRE_EQUAL(miners::to_transport(false, false, true), "tls");
    BC_REQUIRE_EQUAL(miners::to_transport(false, false, false), "tcp");
}

// add/remove

BC_AUTO_TEST_CASE(miners__add__device__registered)
{
    miners registry{};
    registry.add(make_device("a"));

    const auto devices = registry.get_devices(start);
    BC_REQUIRE_EQUAL(devices.size(), 1u);
    BC_REQUIRE_EQUAL(devices.front().id, "a");
    BC_REQUIRE_EQUAL(devices.front().service, "stratum_v1");
    BC_REQUIRE_EQUAL(devices.front().workers.size(), 1u);
    BC_REQUIRE_EQUAL(devices.front().workers.front(), "worker");
    BC_REQUIRE_EQUAL(devices.front().accepted, 0u);
    BC_REQUIRE(!devices.front().last_share.has_value());
    BC_REQUIRE_EQUAL(registry.generation(), 1u);
}

BC_AUTO_TEST_CASE(miners__add__same_id__replaced)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add(make_device("a"));
    BC_REQUIRE_EQUAL(registry.get_devices(start).size(), 1u);
    BC_REQUIRE_EQUAL(registry.generation(), 2u);
}

BC_AUTO_TEST_CASE(miners__remove__registered__removed)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add(make_device("b"));
    registry.remove("a");

    const auto devices = registry.get_devices(start);
    BC_REQUIRE_EQUAL(devices.size(), 1u);
    BC_REQUIRE_EQUAL(devices.front().id, "b");
    BC_REQUIRE_EQUAL(registry.generation(), 3u);
}

BC_AUTO_TEST_CASE(miners__remove__unregistered__unchanged)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.remove("b");
    BC_REQUIRE_EQUAL(registry.get_devices(start).size(), 1u);
    BC_REQUIRE_EQUAL(registry.generation(), 1u);
}

// set_difficulty

BC_AUTO_TEST_CASE(miners__set_difficulty__registered__updated)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.set_difficulty("a", 42.0);
    registry.set_difficulty("b", 7.0);

    const auto devices = registry.get_devices(start);
    BC_REQUIRE_EQUAL(devices.size(), 1u);
    BC_REQUIRE_EQUAL(devices.front().difficulty, 42.0);
}

// add_share

BC_AUTO_TEST_CASE(miners__add_share__accepted__counted)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add_share("a", make_share(start + 1, "accepted"), true);

    const auto devices = registry.get_devices(start + 1);
    const auto& device = devices.front();
    BC_REQUIRE_EQUAL(device.accepted, 1u);
    BC_REQUIRE_EQUAL(device.rejected, 0u);
    BC_REQUIRE(device.rejections.empty());
    BC_REQUIRE_EQUAL(device.last_share.value(), start + 1);
    BC_REQUIRE_EQUAL(device.shares.size(), 1u);
    BC_REQUIRE_EQUAL(device.shares.front().result, "accepted");
    BC_REQUIRE_EQUAL(device.shares.front().height, 42u);
}

BC_AUTO_TEST_CASE(miners__add_share__rejected__counted_by_result)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add_share("a", make_share(start + 1, "stale"), false);
    registry.add_share("a", make_share(start + 2, "stale"), false);
    registry.add_share("a", make_share(start + 3, "duplicate"), false);

    const auto devices = registry.get_devices(start + 3);
    const auto& device = devices.front();
    BC_REQUIRE_EQUAL(device.accepted, 0u);
    BC_REQUIRE_EQUAL(device.rejected, 3u);
    BC_REQUIRE_EQUAL(device.rejections.at("stale"), 2u);
    BC_REQUIRE_EQUAL(device.rejections.at("duplicate"), 1u);
    BC_REQUIRE_EQUAL(device.shares.front().result, "duplicate");
}

BC_AUTO_TEST_CASE(miners__add_share__unregistered__ignored)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add_share("b", make_share(start + 1, "accepted"), true);
    BC_REQUIRE_EQUAL(registry.get_devices(start + 1).front().accepted, 0u);
}

BC_AUTO_TEST_CASE(miners__add_share__over_maximum__newest_retained)
{
    miners registry{};
    registry.add(make_device("a"));
    const auto count = add_shares(registry, add1<uint64_t>(miners::maximum_shares));

    const auto devices = registry.get_devices(start + count);
    const auto& device = devices.front();
    BC_REQUIRE_EQUAL(device.accepted, count);
    BC_REQUIRE_EQUAL(device.shares.size(), miners::maximum_shares);
    BC_REQUIRE_EQUAL(device.shares.front().time, start + count);
    BC_REQUIRE_EQUAL(device.shares.back().time, start + 2u);
}

// hashrate

BC_AUTO_TEST_CASE(miners__get_devices__minute_closed__rate_of_work)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add_share("a", make_share(start + 1, "accepted"), true);

    const auto devices = registry.get_devices(start + minute);
    const auto& device = devices.front();
    BC_REQUIRE_EQUAL(device.history.size(), 1u);
    BC_REQUIRE_EQUAL(device.history.front().time, start - (start % minute));
    BC_REQUIRE_EQUAL(device.history.front().rate, one_share_rate);
    BC_REQUIRE_EQUAL(device.hashrate, one_share_rate);
}

BC_AUTO_TEST_CASE(miners__get_devices__minute_open__no_history)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add_share("a", make_share(start + 1, "accepted"), true);

    const auto devices = registry.get_devices(start + 1);
    const auto& device = devices.front();
    BC_REQUIRE(device.history.empty());
    BC_REQUIRE_EQUAL(device.hashrate, 0.0);
}

BC_AUTO_TEST_CASE(miners__get_devices__idle_minutes__mean_rate)
{
    miners registry{};
    registry.add(make_device("a"));
    registry.add_share("a", make_share(start + 1, "accepted"), true);

    const auto devices = registry.get_devices(start + 2u * minute);
    const auto& device = devices.front();
    BC_REQUIRE_EQUAL(device.history.size(), 2u);
    BC_REQUIRE_EQUAL(device.history.back().rate, 0.0);
    BC_REQUIRE_EQUAL(device.hashrate, one_share_rate / 2.0);
}

BC_AUTO_TEST_CASE(miners__get_devices__beyond_history__oldest_dropped)
{
    miners registry{};
    registry.add(make_device("a"));
    const auto later = start + (miners::history_minutes + 5u) * minute;

    const auto devices = registry.get_devices(later);
    const auto& device = devices.front();
    BC_REQUIRE_EQUAL(device.history.size(), miners::history_minutes);
}

// add_block

BC_AUTO_TEST_CASE(miners__add_block__over_maximum__newest_retained)
{
    miners registry{};
    const auto count = add_blocks(registry, add1(miners::maximum_blocks));

    const auto blocks = registry.get_blocks();
    BC_REQUIRE_EQUAL(blocks.size(), miners::maximum_blocks);
    BC_REQUIRE_EQUAL(blocks.front().height, count);
    BC_REQUIRE_EQUAL(blocks.back().height, 2u);
    BC_REQUIRE_EQUAL(blocks.front().device, "a");
}

BC_AUTO_TEST_SUITE_END()
