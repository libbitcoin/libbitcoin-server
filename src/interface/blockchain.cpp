/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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

#include <utility>
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

// TODO: create interface doc for unordered list, unconfirmeds and key change.
void blockchain::fetch_history4(server_node& node, const message& request,
    send_handler handler)
{
    static constexpr size_t default_limit = 0;
    static constexpr size_t history_args_size = hash_size + sizeof(uint32_t);

    const auto& data = request.data();

    if (data.size() != history_args_size)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto key = deserial.read_reverse<hash_digest>();
    const size_t from_height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_history(key, default_limit, from_height,
        std::bind(&blockchain::history_fetched,
            _1, _2, request, handler));
}

/*
void blockchain::history_fetched(const code& ec,
    const payment_record::list& payments, const message& request,
    send_handler handler)
{
    static const auto record_size = payment_record::satoshi_fixed_size(true);
    data_chunk result(code_size + record_size * payments.size());
    auto serial = make_unsafe_serializer(result.begin());
    serial.write_error_code(ec);

    // Unconfirmed transactions have height sentinal of max_uint32.
    for (const auto& record: payments)
        record.to_data(serial, true);

    handler(message(request, std::move(result)));
}
*/

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
    const auto require_confirmed = true;
    const auto witness = false;

    node.chain().fetch_transaction(hash, require_confirmed, witness,
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
    const auto require_confirmed = true;
    const auto witness = script::is_enabled(
        node.blockchain_settings().enabled_forks(), rule_fork::bip141_rule);

    node.chain().fetch_transaction(hash, require_confirmed, witness,
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

/*
void blockchain::fetch_compact_filter(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.size() == hash_size + 1u)
        blockchain::fetch_compact_filter_by_hash(node, request, handler);
    else if (data.size() == sizeof(uint32_t) + 1u)
        blockchain::fetch_compact_filter_by_height(node, request, handler);
    else
        handler(message(request, error::bad_stream));
}

void blockchain::fetch_compact_filter_by_hash(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == 1u + hash_size);

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto filter_type = deserial.read_byte();
    const auto block_hash = deserial.read_hash();

    node.chain().fetch_compact_filter(filter_type, block_hash,
        std::bind(&blockchain::compact_filter_fetched,
            _1, _2, _3, request, handler));
}

void blockchain::fetch_compact_filter_by_height(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == 1u + sizeof(uint32_t));

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto filter_type = deserial.read_byte();
    const uint64_t height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_compact_filter(filter_type, height,
        std::bind(&blockchain::compact_filter_fetched,
            _1, _2, _3, request, handler));
}

void blockchain::compact_filter_fetched(const code& ec,
    system::compact_filter_ptr response, size_t, const message& request,
    send_handler handler)
{
    if (ec)
    {
        handler(message(request, ec));
        return;
    }

    // [ code:4 ]
    // [ compact filter... ]
    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        response->to_data(canonical)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_compact_filter_headers(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.size() == 1u + sizeof(uint32_t) + hash_size)
        blockchain::fetch_compact_filter_headers_by_hash(node, request, handler);
    else if (data.size() == 1u + sizeof(uint32_t) + sizeof(uint32_t))
        blockchain::fetch_compact_filter_headers_by_height(node, request, handler);
    else
        handler(message(request, error::bad_stream));
}

void blockchain::fetch_compact_filter_headers_by_hash(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == 1u + sizeof(uint32_t) + hash_size);

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto filter_type = deserial.read_byte();
    const auto start_height = deserial.read_4_bytes_little_endian();
    const auto stop_hash = deserial.read_hash();

    node.chain().fetch_compact_filter_headers(filter_type, start_height,
        stop_hash, std::bind(&blockchain::compact_filter_headers_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_compact_filter_headers_by_height(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();
    BITCOIN_ASSERT(data.size() == 1u + sizeof(uint32_t) + sizeof(uint32_t));

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto filter_type = deserial.read_byte();
    const auto start_height = deserial.read_4_bytes_little_endian();
    const auto stop_height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_compact_filter_headers(filter_type, start_height,
        stop_height, std::bind(&blockchain::compact_filter_headers_fetched,
            _1, _2, request, handler));
}

void blockchain::compact_filter_headers_fetched(const code& ec,
    system::compact_filter_headers_ptr response,
    const message& request, send_handler handler)
{
    if (ec)
    {
        handler(message(request, ec));
        return;
    }

    // [ code:4 ]
    // [ compact filter headers... ]
    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        response->to_data(canonical)
    });

    handler(message(request, std::move(result)));
}

void blockchain::fetch_compact_filter_checkpoint(server_node& node,
    const message& request, send_handler handler)
{
    const auto& data = request.data();

    if (data.size() != hash_size + 1u)
    {
        handler(message(request, error::bad_stream));
        return;
    }

    auto deserial = make_safe_deserializer(data.begin(), data.end());
    const auto filter_type = deserial.read_byte();
    const auto stop_hash = deserial.read_hash();

    node.chain().fetch_compact_filter_checkpoint(filter_type, stop_hash,
        std::bind(&blockchain::compact_filter_checkpoint_fetched,
            _1, _2, request, handler));
}

void blockchain::compact_filter_checkpoint_fetched(const code& ec,
    compact_filter_checkpoint_ptr checkpoint, const message& request,
    send_handler handler)
{
    if (ec)
    {
        handler(message(request, ec));
        return;
    }

    // [ code:4 ]
    // [ compact filter checkpoint... ]
    auto result = build_chunk(
    {
        message::to_bytes(error::success),
        checkpoint->to_data(canonical),
    });

    handler(message(request, std::move(result)));
}
*/

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

/*
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
*/

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
    const auto require_confirmed = true;

    node.chain().fetch_transaction_position(hash, require_confirmed,
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

    // Failure to parse will result in a not found output.
    /* bool */ outpoint.from_data(data);

    node.chain().fetch_spend(outpoint,
        std::bind(&blockchain::spend_fetched,
            _1, _2, request, handler));
}

void blockchain::spend_fetched(const code& ec, const system::chain::input::cptr inpoint,
    const message& request, send_handler handler)
{
    // [ code:4 ]
    // [ hash:32 ]
    // [ index:4 ]
    auto result = build_chunk(
    {
        message::to_bytes(ec),
        inpoint->to_data()
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
