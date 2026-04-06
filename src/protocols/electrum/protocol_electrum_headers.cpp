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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <atomic>
#include <ranges>
#include <variant>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_electrum::handle_blockchain_number_of_blocks_subscribe(
    const code& ec, rpc_interface::blockchain_number_of_blocks_subscribe) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    subscribed_height_.store(true, std::memory_order_relaxed);
    const auto top_height = archive().get_top_confirmed();
    send_result(top_height, 42, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_block_get_chunk(const code& ec,
    rpc_interface::blockchain_block_get_chunk, double index) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t position{};
    if (!to_integer(position, index))
    {
        send_code(error::invalid_argument);
        return;
    }

    // Get zero-based index of 2016 confirmed header "chunks".
    constexpr size_t chunk{ 2016 };
    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto height = ceilinged_multiply(position, chunk);
    const auto end = limit(ceilinged_add(height, chunk), add1(top));
    const auto count = floored_subtract(end, height);
    const auto size = count * two * chain::header::serialized_size();
    const auto links = query.get_confirmed_headers(height, count);

    std::string headers(size, '\0');
    stream::out::fast sink{ headers };
    write::base16::fast writer{ sink };
    for (const auto& link: links)
    {
        if (!query.get_wire_header(writer, link))
        {
            send_code(error::server_error);
            return;
        }
    }

    send_result(std::move(headers), size + 42u, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_block_get_header(const code& ec,
    rpc_interface::blockchain_block_get_header, double height) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t target{};
    if (!to_integer(target, height))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto link = query.to_confirmed(target);
    if (link.is_terminal())
    {
        ////send_code(error::not_found);
        send_result(null_t{}, 42, BIND(complete, _1));
        return;
    }

    constexpr auto size = two * chain::header::serialized_size();
    std::string header(size, '\0');
    stream::out::fast sink{ header };
    write::base16::fast writer{ sink };
    if (!query.get_wire_header(writer, link))
    {
        send_code(error::server_error);
        return;
    }

    send_result(std::move(header), size + 42u, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_block_header(const code& ec,
    rpc_interface::blockchain_block_header, double height,
    double cp_height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_3))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t starting{};
    size_t waypoint{};
    if (!to_integer(starting, height) ||
        !to_integer(waypoint, cp_height))
    {
        send_code(error::invalid_argument);
        return;
    }

    blockchain_block_headers(starting, one, waypoint, false);
}

void protocol_electrum::handle_blockchain_block_headers(const code& ec,
    rpc_interface::blockchain_block_headers, double start_height, double count,
    double cp_height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_2))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t quantity{};
    size_t waypoint{};
    size_t starting{};
    if (!to_integer(quantity, count) ||
        !to_integer(waypoint, cp_height) ||
        !to_integer(starting, start_height))
    {
        send_code(error::invalid_argument);
        return;
    }

    if (!is_zero(cp_height) && !at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    blockchain_block_headers(starting, quantity, waypoint, true);
}

// Common implementation for blockchain_block_header/s.
void protocol_electrum::blockchain_block_headers(size_t starting,
    size_t quantity, size_t waypoint, bool multiplicity) NOEXCEPT
{
    const auto prove = !is_zero(quantity) && !is_zero(waypoint);
    const auto target = starting + sub1(quantity);
    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    using namespace system;

    // The documented requirement: `start_height + (count - 1) <= cp_height` is
    // ambiguous at count = 0 so guard must be applied to both args and prover.
    if (is_add_overflow(starting, quantity))
    {
        send_code(error::argument_overflow);
        return;
    }
    else if ((starting > top) || (prove && waypoint > top))
    {
        send_code(error::not_found);
        return;
    }
    else if (prove && target > waypoint)
    {
        send_code(error::target_overflow);
        return;
    }

    // Recommended to be at least one difficulty retarget period, e.g. 2016.
    // The maximum number of headers the server will return in single request.
    const auto maximum_headers = server_settings().electrum.maximum_headers;

    // Returned headers are assured to be contiguous despite intervening reorg.
    // No headers may be returned, which implies start > confirmed top block.
    const auto count = limit(quantity, maximum_headers);
    const auto links = query.get_confirmed_headers(starting, count);
    auto size = two * chain::header::serialized_size() * links.size();

    value_t value{ object_t{} };
    auto& result = std::get<object_t>(value.value());
    if (multiplicity)
    {
        result["max"] = maximum_headers;
        result["count"] = links.size();
    }
    else if (links.empty())
    {
        send_code(error::server_error);
        return;
    }

    if (at_least(electrum::version::v1_6))
    {
        array_t headers{};
        headers.reserve(links.size());
        for (const auto& link: links)
        {
            const auto header = query.get_wire_header(link);
            if (header.empty())
            {
                send_code(error::server_error);
                return;
            }

            headers.push_back(encode_base16(header));
        };

        if (multiplicity)
            result["headers"] = std::move(headers);
        else
            result["header"] = std::move(headers.front());
    }
    else
    {
        std::string headers(size, '\0');
        stream::out::fast sink{ headers };
        write::base16::fast writer{ sink };
        for (const auto& link: links)
        {
            if (!query.get_wire_header(writer, link))
            {
                send_code(error::server_error);
                return;
            }
        };

        result["hex"] = std::move(headers);
    }

    // There is a very slim chance of inconsistency given an intervening reorg
    // because of get_merkle_root_and_proof() use of height-based calculations.
    // This is acceptable as it must be verified by caller in any case.
    if (prove)
    {
        hashes proof{};
        hash_digest root{};
        if (const auto code = query.get_merkle_root_and_proof(root, proof,
            target, waypoint))
        {
            send_code(code);
            return;
        }

        array_t branch(proof.size());
        std::ranges::transform(proof, branch.begin(),
            [](const auto& hash) NOEXCEPT { return encode_hash(hash); });

        result["branch"] = std::move(branch);
        result["root"] = encode_hash(root);
        size += two * hash_size * add1(proof.size());
    }

    send_result(std::move(value), size + 42u, BIND(complete, _1));
}

// TODO: implement support for v1.3 explicit false.
// TODO: This implies an override to channel_rpc<electrum>::dispatch() with
// TODO: use of an injected nullable<bool> to indicate presence.
// v1.2 undocumented add optional parameter 'raw' (default false).
// v1.3 undocumented change optional parameter 'raw' (default to true).
// v1.4 undocumented remove parameter 'raw' (always true).
void protocol_electrum::handle_blockchain_headers_subscribe(const code& ec,
    rpc_interface::blockchain_headers_subscribe, bool raw) NOEXCEPT
{
    if (stopped(ec))
        return;

    // HACK: assumes raw true implies defined.
    // HACK: precludes explicity setting raw false for v1.3.
    const auto raw_defined = raw;
    if (at_least(electrum::version::v1_3))
        raw = true;

    if ((!at_least(electrum::version::v1_0)) ||
        (!at_least(electrum::version::v1_2) && raw_defined) ||
        ( at_least(electrum::version::v1_4) && raw_defined))
    {
        send_code(error::wrong_version);
        return;
    }

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);

    // This is unlikely but possible due to a race condition during reorg.
    if (link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    const auto header = query.get_wire_header(link);
    if (header.empty())
    {
        send_code(error::server_error);
        return;
    }

    // TODO: determine intended encoding.
    if (!raw)
    {
        send_code(error::not_implemented);
        return;
    }

    subscribed_header_.store(true, std::memory_order_relaxed);
    send_result(object_t
    {
        { "height", top },
        { "hex", encode_base16(header) }
    }, 256, BIND(complete, _1));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
