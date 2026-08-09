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

// Isolate the subgroup dispatch metaprogramming to this translation unit.
template class network::rpc::dispatcher<
    server::interface::bitcoind_mining>;

namespace server {

template class protocol_bitcoind_dispatch<interface::bitcoind_mining>;

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
    SUBSCRIBE_BITCOIND(handle_submit_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_header, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_block_template, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_prioritised_transactions, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prioritise_transaction, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Mining methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_mining::handle_get_network_hash_ps(const code& ec,
    rpc_interface::get_network_hash_ps, uint32_t, int32_t height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto target = is_negative(height) ? top :
        std::min(sign_cast<size_t>(height), top);

    const auto header = query.get_header(query.to_confirmed(target));
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    const auto period = system_settings().block_spacing_seconds;
    const auto span = to_floating(power2<uint64_t>(32u));
    send_result(header->difficulty() * span / period, 20);
    return true;
}

// currentblockweight/currentblocktx are omitted (bitcoind omits them until a
// block is assembled, and there is no assembler). The tx pool is empty and no
// packages are selected, so pooledtx and blockmintxfee are zero.
bool protocol_bitcoind_mining::handle_get_mining_info(const code& ec,
    rpc_interface::get_mining_info) NOEXCEPT
{
    using namespace chain;

    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);
    const auto tip = query.get_header(link);
    if (!tip)
    {
        send_error(database::error::integrity);
        return true;
    }

    // The pool state over the top block carries the next work required.
    const auto state = query.get_chain_state(system_settings(),
        query.get_header_key(link));
    if (!state)
    {
        send_error(database::error::integrity);
        return true;
    }

    const chain_state pool{ *state, system_settings() };
    const header next{ 0, {}, {}, 0, pool.work_required(), 0 };
    const auto period = system_settings().block_spacing_seconds;
    const auto span = to_floating(power2<uint64_t>(32u));

    send_result(object_t
    {
        { "blocks", top },
        { "bits", encode_base16(to_big_endian(tip->bits())) },
        { "difficulty", tip->difficulty() },
        { "target", encode_hash(from_uintx(compact::expand(tip->bits()))) },
        { "networkhashps", tip->difficulty() * span / period },
        { "pooledtx", zero },
        { "blockmintxfee", 0.0 },
        { "chain", chain_name(query) },
        { "next", object_t
            {
                { "height", add1(top) },
                { "bits", encode_base16(to_big_endian(next.bits())) },
                { "difficulty", next.difficulty() },
                { "target",
                    encode_hash(from_uintx(compact::expand(next.bits()))) }
            } },
        { "warnings", array_t{} }
    }, 512);
    return true;
}

bool protocol_bitcoind_mining::handle_submit_block(const code& ec,
    rpc_interface::submit_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_mining::handle_submit_header(const code& ec,
    rpc_interface::submit_header) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_mining::handle_get_block_template(const code& ec,
    rpc_interface::get_block_template) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_mining::handle_get_prioritised_transactions(const code& ec,
    rpc_interface::get_prioritised_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_mining::handle_prioritise_transaction(const code& ec,
    rpc_interface::prioritise_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
