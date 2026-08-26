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
#include <bitcoin/server/protocols/protocol_bitcoind_mining.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

namespace server {

#define CLASS protocol_bitcoind_mining
#define SUBSCRIBE_BITCOIND(method, ...) \
    subscribe<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_mining::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_network_hash_ps, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mining_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_submit_header, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_template, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_prioritised_transactions, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prioritise_transaction, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Mining methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_mining::handle_get_network_hash_ps(const code& ec,
    rpc_interface::get_network_hash_ps, double nblocks,
    double height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();

    // A negative height selects the confirmed top (as bitcoind).
    size_t target{};
    if (height < 0)
    {
        target = top;
    }
    else if (!to_integer(target, height))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }
    else
    {
        target = std::min(target, top);
    }

    // A non-positive window selects the span since the last retarget.
    size_t window{};
    if (nblocks <= 0)
    {
        window = add1(target % system_settings().retargeting_interval());
    }
    else if (!to_integer(window, nblocks))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    window = std::min(window, target);
    if (is_zero(window))
    {
        send_result(zero, 20);
        return true;
    }

    // The window timespan is bounded by its observed timestamps.
    const auto first = target - window;
    auto minimum = max_uint32;
    auto maximum = min_uint32;
    for (auto index = first; index <= target; ++index)
    {
        const auto header = query.get_header(query.to_confirmed(index));
        if (!header)
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }

        minimum = std::min(minimum, header->timestamp());
        maximum = std::max(maximum, header->timestamp());
    }

    if (minimum == maximum)
    {
        send_result(zero, 20);
        return true;
    }

    uint256_t start_work{};
    uint256_t end_work{};
    if (!query.get_branch_work(start_work, query.to_confirmed(first)) ||
        !query.get_branch_work(end_work, query.to_confirmed(target)))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    const auto work = (end_work - start_work).convert_to<double>();
    send_result(work / (maximum - minimum), 20);
    return true;
}

// currentblockweight/currentblocktx are omitted (bitcoind omits them until a
// block is assembled, and there is no assembler). The tx pool is empty, so
// pooledtx is zero, and no packages are selected, so blockmintxfee is the
// maximum.
bool protocol_bitcoind_mining::handle_get_mining_info(const code& ec,
    rpc_interface::get_mining_info) NOEXCEPT
{
    using namespace chain;

    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto height = query.get_top_confirmed();
    const auto link = query.to_confirmed(height);
    const auto top = query.get_header(link);
    if (!top)
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    // The pool state over the top block carries the next work required.
    const auto& bitcoin = system_settings();
    const auto key = query.get_header_key(link);
    const auto state = query.get_chain_state(bitcoin, key);
    if (!state)
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    const chain_state pool{ *state, bitcoin };
    const header header{ 0, {}, {}, 0, pool.work_required(), 0 };
    object_t next_block
    {
        { "height", add1(height) },
        { "bits", encode_base16(to_big_endian(header.bits())) },
        { "difficulty", header.difficulty() },
        { "target", encode_hash(from_uintx(compact::expand(header.bits()))) }
    };

    // TODO: change to min inclusion fee when mining enabled.
    const auto max_money = to_floating(bitcoin.max_money());
    const auto span = to_floating(power2<uint64_t>(32u));
    const auto period = bitcoin.block_spacing_seconds;

    // bitcoind OB1 error ("blocks" wants height).
    send_result(object_t
    {
        { "blocks", height },
        { "bits", encode_base16(to_big_endian(top->bits())) },
        { "difficulty", top->difficulty() },
        { "target", encode_hash(from_uintx(compact::expand(top->bits()))) },
        { "networkhashps", top->difficulty() * span / period },
        { "pooledtx", zero },
        { "blockmintxfee", max_money / satoshi_per_bitcoin },
        { "chain", chain_name(query) },
        { "next", std::move(next_block) },
        { "warnings", array_t{} }
    }, 512);
    return true;
}

// The response defers to organize completion (see dispatch).
bool protocol_bitcoind_mining::handle_submit_block(const code& ec,
    rpc_interface::submit_block, const std::string& hexdata,
    const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexdata))
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    constexpr auto witness = true;
    const auto block = to_shared<chain::block>(data, witness);
    if (!block->is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    // bitcoind reports an already-stored block as a duplicate result.
    if (!archive().to_header(block->hash()).is_terminal())
    {
        send_result(std::string{ "duplicate" }, 32);
        return true;
    }

    organize(block, BIND(handle_organize_block, _1, _2));
    return true;
}

bool protocol_bitcoind_mining::handle_submit_header(const code& ec,
    rpc_interface::submit_header, const std::string& hexdata) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexdata))
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    const auto header = to_shared<chain::header>(data);
    if (!header->is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    if (!archive().to_header(header->hash()).is_terminal())
    {
        send_result(null_t{}, 8);
        return true;
    }

    organize(header, BIND(handle_organize_header, _1, _2));
    return true;
}

void protocol_bitcoind_mining::handle_organize_block(const code& ec,
    size_t) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_submit_block, ec);
}

void protocol_bitcoind_mining::handle_organize_header(const code& ec,
    size_t) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_submit_header, ec);
}

// bitcoind returns null on acceptance and a reject token on rejection.
void protocol_bitcoind_mining::do_submit_block(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (ec)
        send_result(error::bitcoind::reject(ec), 64);
    else
        send_result(null_t{}, 8);
}

void protocol_bitcoind_mining::do_submit_header(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace error::bitcoind;
    if (ec)
        send_error(translate(ec, verify_error));
    else
        send_result(null_t{}, 8);
}

bool protocol_bitcoind_mining::handle_get_block_template(const code& ec,
    rpc_interface::get_block_template) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_mining::handle_get_prioritised_transactions(const code& ec,
    rpc_interface::get_prioritised_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_mining::handle_prioritise_transaction(const code& ec,
    rpc_interface::prioritise_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
