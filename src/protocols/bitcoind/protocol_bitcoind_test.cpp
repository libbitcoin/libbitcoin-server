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
#include <bitcoin/server/protocols/protocol_bitcoind_test.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

// Isolate the subgroup dispatch metaprogramming to this translation unit.
template class network::rpc::dispatcher<
    server::interface::bitcoind_test>;

namespace server {

template class protocol_bitcoind_dispatch<interface::bitcoind_test>;

#define CLASS protocol_bitcoind_test
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

void protocol_bitcoind_test::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_add_connection, _1, _2);
    SUBSCRIBE_BITCOIND(handle_add_peer_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_echo, _1, _2);
    SUBSCRIBE_BITCOIND(handle_echo_ipc, _1, _2);
    SUBSCRIBE_BITCOIND(handle_echo_json, _1, _2);
    SUBSCRIBE_BITCOIND(handle_estimate_raw_fee, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate_to_address, _1, _2);
    SUBSCRIBE_BITCOIND(handle_generate_to_descriptor, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_mempool_fee_rate_diagram, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_orphan_txs, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_raw_addrman, _1, _2);
    SUBSCRIBE_BITCOIND(handle_invalidate_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_mock_scheduler, _1, _2);
    SUBSCRIBE_BITCOIND(handle_reconsider_block, _1, _2);
    SUBSCRIBE_BITCOIND(handle_send_msg_to_peer, _1, _2);
    SUBSCRIBE_BITCOIND(handle_set_mock_time, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sync_with_validation_interface_queue, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Test methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_test::handle_add_connection(const code& ec,
    rpc_interface::add_connection) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_add_peer_address(const code& ec,
    rpc_interface::add_peer_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_echo(const code& ec,
    rpc_interface::echo) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_echo_ipc(const code& ec,
    rpc_interface::echo_ipc) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_echo_json(const code& ec,
    rpc_interface::echo_json) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_estimate_raw_fee(const code& ec,
    rpc_interface::estimate_raw_fee) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_generate(const code& ec,
    rpc_interface::generate) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_generate_block(const code& ec,
    rpc_interface::generate_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_generate_to_address(const code& ec,
    rpc_interface::generate_to_address) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_generate_to_descriptor(const code& ec,
    rpc_interface::generate_to_descriptor) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_get_mempool_fee_rate_diagram(const code& ec,
    rpc_interface::get_mempool_fee_rate_diagram) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_get_orphan_txs(const code& ec,
    rpc_interface::get_orphan_txs) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_get_raw_addrman(const code& ec,
    rpc_interface::get_raw_addrman) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_invalidate_block(const code& ec,
    rpc_interface::invalidate_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_mock_scheduler(const code& ec,
    rpc_interface::mock_scheduler) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_reconsider_block(const code& ec,
    rpc_interface::reconsider_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_send_msg_to_peer(const code& ec,
    rpc_interface::send_msg_to_peer) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_set_mock_time(const code& ec,
    rpc_interface::set_mock_time) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_test::handle_sync_with_validation_interface_queue(const code& ec,
    rpc_interface::sync_with_validation_interface_queue) NOEXCEPT
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
