/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/interface/blockchain.hpp>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <utility>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::blockchain;
using namespace bc::system;
using namespace bc::system::chain;
using namespace bc::system::machine;
using namespace bc::system::wallet;

static constexpr size_t code_size = sizeof(uint32_t);
static constexpr size_t index_size = sizeof(uint32_t);
static constexpr size_t point_size = hash_size + sizeof(uint32_t);
static constexpr auto canonical = system::message::version::level::canonical;

void blockchain::fetch_history4(server_node& node, const message& request,
    send_handler handler)
{
    static constexpr size_t limit = 0;
    static constexpr size_t history_args_size = short_hash_size +
        sizeof(uint32_t);

    const auto& data = request.data();

    if (data.size() != history_args_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto script_hash = deserial.read_hash();
    const size_t from_height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_history(script_hash, limit, from_height,
        std::bind(&blockchain::history_fetched,
            _1, _2, request, handler));
}

void blockchain::history_fetched(const code& ec,
    const payment_record::list& payments, const message& request,
    send_handler handler)
{
    static const auto record_size = payment_record::satoshi_fixed_size(true);
    data_chunk result(code_size + record_size * payments.size());
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);

    for (const auto& record: payments)
        record.to_data(serial, true);

    handler(message(request, std::move(result)));
}

void blockchain::fetch_transaction(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto hash = deserial.read_hash();

    // The response is restricted to confirmed transactions.
    // This response excludes witness data so as not to break old parsers.
    node.chain().fetch_transaction(hash, true, false,
        std::bind(&blockchain::transaction_fetched,
            _1, _2, _3, _4, request, handler));
}

void blockchain::fetch_transaction2(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto hash = deserial.read_hash();

    // The response is restricted to confirmed transactions.
    // This response can include witness data (based on configuration)
    // so may break old parsers.
    const auto witness = script::is_enabled(
        node.blockchain_settings().enabled_forks(), rule_fork::bip141_rule);

    node.chain().fetch_transaction(hash, true, witness,
        std::bind(&blockchain::transaction_fetched,
            _1, _2, _3, _4, request, handler));
}

void blockchain::transaction_fetched(const code& ec, transaction_const_ptr tx,
    size_t, size_t, const message& request, send_handler handler)
{
    if (ec)
    {
        handler(message(request, ec));
        return;
    }

    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        tx->to_data(canonical)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_last_height(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (!data.empty())
    {
        handler(message(request, error::bad_stream));
        return;
    }

    node.chain().fetch_last_height(
        std::bind(&blockchain::last_height_fetched,
            _1, _2, request, handler));
}

void blockchain::last_height_fetched(const code& ec, size_t last_height,
    const message& request, send_handler handler)
{
    BITCOIN_ASSERT(last_height <= max_uint32);
    auto last_height32 = static_cast<uint32_t>(last_height);

    // [ code:4 ]
    // [ heigh:4 ]
    auto result = build_chunk(
    {
        message::to_bytes(ec),
        to_little_endian(last_height32)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_block(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.size() == hash_size)
        blockchain::fetch_block_by_hash(node, request, handler);
    else if (data.size() == sizeof(uint32_t))
        blockchain::fetch_block_by_height(node, request, handler);
    else
        handler(message(request, error::bad_stream));
}

void blockchain::fetch_block_by_hash(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == hash_size);

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();

    const auto witness = script::is_enabled(
        node.blockchain_settings().enabled_forks(), rule_fork::bip141_rule);

    node.chain().fetch_block(block_hash, witness,
        std::bind(&blockchain::block_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_block_by_height(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == sizeof(uint32_t));

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const uint64_t height = deserial.read_4_bytes_little_endian();

    const auto witness = script::is_enabled(
        node.blockchain_settings().enabled_forks(), rule_fork::bip141_rule);

    node.chain().fetch_block(height, witness,
        std::bind(&blockchain::block_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_block_header(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.size() == hash_size)
        blockchain::fetch_block_header_by_hash(node, request, handler);
    else if (data.size() == sizeof(uint32_t))
        blockchain::fetch_block_header_by_height(node, request, handler);
    else
        handler(message(request, error::bad_stream));
}

void blockchain::fetch_block_header_by_hash(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == hash_size);

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();

    node.chain().fetch_block_header(block_hash,
        std::bind(&blockchain::block_header_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_block_header_by_height(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == sizeof(uint32_t));

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const uint64_t height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_block_header(height,
        std::bind(&blockchain::block_header_fetched,
            _1, _2, request, handler));
}

void blockchain::block_fetched(const code& ec, block_const_ptr block,
    const message& request, send_handler handler)
{
    if (ec)
    {
        handler(message(request, ec));
        return;
    }

    // [ code:4 ]
    // [ block... ]
    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        block->to_data(canonical),
    });

    handler(message(request, std::move(result)));
}

void blockchain::block_header_fetched(const code& ec, header_const_ptr header,
    const message& request, send_handler handler)
{
    if (ec)
    {
        handler(message(request, ec));
        return;
    }

    // [ code:4 ]
    // [ block... ]
    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        header->to_data(canonical)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_block_transaction_hashes(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.size() == hash_size)
        fetch_block_transaction_hashes_by_hash(node, request, handler);
    else if (data.size() == sizeof(uint32_t))
        fetch_block_transaction_hashes_by_height(node, request, handler);
    else
        handler(message(request, error::bad_stream));
}

void blockchain::fetch_block_transaction_hashes_by_hash(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == hash_size);

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();
    node.chain().fetch_merkle_block(block_hash,
        std::bind(&blockchain::merkle_block_fetched,
            _1, _2, _3, request, handler));
}

void blockchain::fetch_block_transaction_hashes_by_height(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == sizeof(uint32_t));

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const size_t block_height = deserial.read_4_bytes_little_endian();
    node.chain().fetch_merkle_block(block_height,
        std::bind(&blockchain::merkle_block_fetched,
            _1, _2, _3, request, handler));
}

void blockchain::merkle_block_fetched(const code& ec, merkle_block_ptr block,
    size_t, const message& request, send_handler handler)
{
    // [ code:4 ]
    // [[ hash:32 ]...]
    data_chunk result(code_size + hash_size * block->hashes().size());
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);

    for (const auto& hash: block->hashes())
        serial.write_hash(hash);

    handler(message(request, std::move(result)));
}

void blockchain::fetch_transaction_index(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto hash = deserial.read_hash();

    // The response is restricted to confirmed transactions (backward compat).
    node.chain().fetch_transaction_position(hash, true,
        std::bind(&blockchain::transaction_index_fetched,
            _1, _2, _3, request, handler));
}

void blockchain::transaction_index_fetched(const code& ec,
    size_t tx_position, size_t block_height, const message& request,
    send_handler handler)
{
    BITCOIN_ASSERT(tx_position <= max_uint32);
    BITCOIN_ASSERT(block_height <= max_uint32);

    auto tx_position32 = static_cast<uint32_t>(tx_position);
    auto block_height32 = static_cast<uint32_t>(block_height);

    // [ code:4 ]
    // [ block_height:4 ]
    // [ tx_position:4 ]
    auto result = build_chunk(
    {
        message::to_bytes(ec),
        to_little_endian(block_height32),
        to_little_endian(tx_position32)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_spend(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != point_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    output_point outpoint;
    outpoint.from_data(data);

    node.chain().fetch_spend(outpoint,
        std::bind(&blockchain::spend_fetched,
            _1, _2, request, handler));
}

void blockchain::spend_fetched(const code& ec, const input_point& inpoint,
    const message& request, send_handler handler)
{
    // [ code:4 ]
    // [ hash:32 ]
    // [ index:4 ]
    auto result = build_chunk(
    {
        message::to_bytes(ec),
        inpoint.to_data()
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_block_height(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != hash_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();
    node.chain().fetch_block_height(block_hash,
        std::bind(&blockchain::block_height_fetched,
            _1, _2, request, handler));
}

void blockchain::block_height_fetched(const code& ec, size_t block_height,
    const message& request, send_handler handler)
{
    BITCOIN_ASSERT(block_height <= max_uint32);
    auto block_height32 = static_cast<uint32_t>(block_height);

    // [ code:4 ]
    // [ height:4 ]
    auto result = build_chunk(
    {
        message::to_bytes(ec),
        to_little_endian(block_height32)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_stealth2(server_node& node, const message& request,
    send_handler handler)
{
    const auto& data = request.data();

    if (data.empty())
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // [ prefix_bitsize:1 ]
    // [ prefix_blocks:1..4 ]
    // [ from_height:4 ]
    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto bits = deserial.read_byte();

    if (bits < stealth_address::min_filter_bits ||
        bits > stealth_address::max_filter_bits)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    const auto bytes = binary::blocks_size(bits);

    if (data.size() != sizeof(uint8_t) + bytes + sizeof(uint32_t))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    const auto blocks = deserial.read_bytes(bytes);
    const size_t from_height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_stealth(binary{ bits, blocks }, from_height,
        std::bind(&blockchain::stealth_fetched,
            _1, _2, request, handler));
}

void blockchain::stealth_fetched(const code& ec,
    const stealth_record::list& stealth, const message& request,
    send_handler handler)
{
    static const auto record_size = stealth_record::satoshi_fixed_size(true);

    // [ code:4 ]
    // [[ ephemeral_key_hash:32 ][ address_hash:20 ][ tx_hash:32 ]...]
    data_chunk result(code_size + record_size * stealth.size());
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);

    for (const auto& record: stealth)
        record.to_data(serial, true);

    handler(message(request, std::move(result)));
}

void blockchain::fetch_stealth_transaction_hashes(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.empty())
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // [ prefix_bitsize:1 ]
    // [ prefix_blocks:1..4 ]
    // [ from_height:4 ]
    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto bits = deserial.read_byte();

    if (bits < stealth_address::min_filter_bits ||
        bits > stealth_address::max_filter_bits)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    const auto bytes = binary::blocks_size(bits);

    if (data.size() != sizeof(uint8_t) + bytes + sizeof(uint32_t))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    const auto blocks = deserial.read_bytes(bytes);
    const size_t from_height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_stealth(binary{ bits, blocks }, from_height,
        std::bind(&blockchain::stealth_transaction_hashes_fetched,
            _1, _2, request, handler));
}

void blockchain::stealth_transaction_hashes_fetched(const code& ec,
    const stealth_record::list& stealth, const message& request,
    send_handler handler)
{
    // [ code:4 ]
    // [[ tx_hash:32 ]...]
    data_chunk result(code_size + hash_size * stealth.size());
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);

    for (const auto& record: stealth)
        serial.write_hash(record.transaction_hash());

    handler(message(request, std::move(result)));
}

// Save to blockchain and announce to all connected peers.
void blockchain::broadcast(server_node& /* node */, const message& request,
    send_handler handler)
{
    const auto block = std::make_shared<system::message::block>();

    if (!block->from_data(canonical, request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // TODO: implement.
    handler(message(request, error::not_implemented));

    ////// Organize into our chain.
    ////block->header().metadata.simulate = false;

    ////// This call is async but blocks on other organizations until started.
    ////// Subscribed channels will pick up and announce via inventory to peers.
    ////node.chain().organize(block,
    ////    std::bind(handle_broadcast, _1, request, handler));
}

void blockchain::handle_broadcast(const code& ec, const message& request,
    send_handler handler)
{
    // Returns validation error or error::success.
    handler(message(request, ec));
}

void blockchain::validate(server_node& /* node */, const message& request,
    send_handler handler)
{
    const auto block = std::make_shared<system::message::block>();

    if (!block->from_data(canonical, request.data()))
    {
        handler(message(request, error::bad_stream));
        return;
    }

    // TODO: implement.
    handler(message(request, error::not_implemented));

    ////// Simulate organization into our chain.
    ////block->header().metadata.simulate = true;

    ////// This call is async but blocks on other organizations until started.
    ////node.chain().organize(block,
    ////    std::bind(handle_validated, _1, request, handler));
}

void blockchain::handle_validated(const code& ec, const message& request,
    send_handler handler)
{
    // Returns validation error or error::success.
    handler(message(request, ec));
}

} // namespace server
} // namespace libbitcoin
