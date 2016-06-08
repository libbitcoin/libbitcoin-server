/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/interface/blockchain.hpp>

#include <cstdint>
#include <cstddef>
#include <functional>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/messages/incoming.hpp>
#include <bitcoin/server/messages/outgoing.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/utility/fetch_helpers.hpp>

namespace libbitcoin {
namespace server {

using namespace std::placeholders;
using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::wallet;

void blockchain::fetch_history(server_node& node,
    const incoming& request, send_handler handler)
{
    static constexpr uint64_t limit = 0;
    uint32_t from_height;
    payment_address address;

    if (!unwrap_fetch_history_args(address, from_height, request))
        return;

    log::debug(LOG_SERVER)
        << "blockchain.fetch_history(" << address.encoded()
        << ", from_height=" << from_height << ")";

    node.chain().fetch_history(address, limit, from_height,
        std::bind(send_history_result,
            _1, _2, request, handler));
}

void blockchain::fetch_transaction(server_node& node,
    const incoming& request, send_handler handler)
{
    hash_digest tx_hash;

    if (!unwrap_fetch_transaction_args(tx_hash, request))
        return;

    log::debug(LOG_SERVER)
        << "blockchain.fetch_transaction(" << encode_hash(tx_hash) << ")";

    node.chain().fetch_transaction(tx_hash,
        std::bind(transaction_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_last_height(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;

    if (!request.data.empty())
    {
        log::error(LOG_SERVER)
            << "Incorrect data size for blockchain.fetch_last_height";
        return;
    }

    node.chain().fetch_last_height(
        std::bind(&blockchain::last_height_fetched,
            _1, _2, request, handler));
}

void blockchain::last_height_fetched(const code& ec, size_t last_height,
    const incoming& request, send_handler handler)
{
    BITCOIN_ASSERT(last_height <= bc::max_uint32);
    auto last_height32 = static_cast<uint32_t>(last_height);

    data_chunk result(code_size + sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    serial.write_4_bytes_little_endian(last_height32);
    BITCOIN_ASSERT(serial.iterator() == result.end());

    handler(outgoing(request, result));
}

void blockchain::fetch_block_header(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;

    if (data.size() == hash_size)
        blockchain::fetch_block_header_by_hash(node, request, handler);
    else if (data.size() == sizeof(uint32_t))
        blockchain::fetch_block_header_by_height(node, request, handler);
    else
        log::error(LOG_SERVER)
            << "Incorrect data size for blockchain.fetch_block_header";
}

void blockchain::fetch_block_header_by_hash(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;
    BITCOIN_ASSERT(data.size() == hash_size);

    auto deserial = make_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();

    node.chain().fetch_block_header(block_hash,
        std::bind(&blockchain::block_header_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_block_header_by_height(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;
    BITCOIN_ASSERT(data.size() == sizeof(uint32_t));

    auto deserial = make_deserializer(data.begin(), data.end());
    const uint64_t height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_block_header(height,
        std::bind(&blockchain::block_header_fetched,
            _1, _2, request, handler));
}

void blockchain::block_header_fetched(const code& ec,
    const chain::header& block, const incoming& request, send_handler handler)
{
    const auto block_size64 = block.serialized_size(false);
    BITCOIN_ASSERT_MSG(block_size64 <= max_size_t, "Clearly Bitcoin is dead.");
    const auto block_size = static_cast<size_t>(block_size64);

    data_chunk result(code_size + block_size);
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    data_chunk block_data = block.to_data(false);
    serial.write_data(block_data);
    BITCOIN_ASSERT(serial.iterator() == result.end());

    handler(outgoing(request, result));
}

void blockchain::fetch_block_transaction_hashes(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;

    if (data.size() == hash_size)
        fetch_block_transaction_hashes_by_hash(node, request, handler);
    else if (data.size() == sizeof(uint32_t))
        fetch_block_transaction_hashes_by_height(node, request, handler);
    else
    {
        log::error(LOG_SERVER)
            << "Incorrect data size for "
            "blockchain.fetch_block_transaction_hashes_by_height";
        return;
    }
}

void blockchain::fetch_block_transaction_hashes_by_hash(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;
    BITCOIN_ASSERT(data.size() == hash_size);

    auto deserial = make_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();
    node.chain().fetch_block_transaction_hashes(block_hash,
        std::bind(&blockchain::block_transaction_hashes_fetched,
            _1, _2, request, handler));
}

void blockchain::fetch_block_transaction_hashes_by_height(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;
    BITCOIN_ASSERT(data.size() == sizeof(uint32_t));

    auto deserial = make_deserializer(data.begin(), data.end());
    const size_t block_height = deserial.read_4_bytes_little_endian();
    node.chain().fetch_block_transaction_hashes(block_height,
        std::bind(&blockchain::block_transaction_hashes_fetched,
            _1, _2, request, handler));
}

void blockchain::block_transaction_hashes_fetched(const code& ec,
    const hash_list& hashes, const incoming& request,
    send_handler handler)
{
    data_chunk result(code_size + hash_size * hashes.size());
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    for (const auto& tx_hash: hashes)
        serial.write_hash(tx_hash);

    BITCOIN_ASSERT(serial.iterator() == result.end());

    handler(outgoing(request, result));
}

void blockchain::fetch_transaction_index(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;

    if (data.size() != hash_size)
    {
        log::error(LOG_SERVER)
            << "Incorrect data size for blockchain.fetch_transaction_index";
        return;
    }

    auto deserial = make_deserializer(data.begin(), data.end());
    const auto tx_hash = deserial.read_hash();

    node.chain().fetch_transaction_index(tx_hash,
        std::bind(&blockchain::transaction_index_fetched,
            _1, _2, _3, request, handler));
}

void blockchain::transaction_index_fetched(const code& ec, size_t block_height,
    size_t index, const incoming& request, send_handler handler)
{
    BITCOIN_ASSERT(index <= max_uint32);
    auto index32 = static_cast<uint32_t>(index);

    BITCOIN_ASSERT(block_height <= max_uint32);
    auto block_height32 = static_cast<uint32_t>(block_height);

    // error_code (4), block_height (4), index (4)
    data_chunk result(code_size + sizeof(uint32_t) + sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    serial.write_4_bytes_little_endian(block_height32);
    serial.write_4_bytes_little_endian(index32);

    handler(outgoing(request, result));
}

void blockchain::fetch_spend(server_node& node, const incoming& request,
    send_handler handler)
{
    const auto& data = request.data;

    if (data.size() != point_size)
    {
        log::error(LOG_SERVER)
            << "Incorrect data size for blockchain.fetch_spend";
        return;
    }

    using namespace boost::iostreams;
    static const auto fail_bit = stream<byte_source<data_chunk>>::failbit;
    stream<byte_source<data_chunk>> istream(data);
    istream.exceptions(fail_bit);
    chain::output_point outpoint;
    outpoint.from_data(istream);

    node.chain().fetch_spend(outpoint,
        std::bind(&blockchain::spend_fetched,
            _1, _2, request, handler));
}

void blockchain::spend_fetched(const code& ec, const chain::input_point& inpoint,
    const incoming& request, send_handler handler)
{
    // error_code (4), hash (32), index (4)
    const auto inpoint_size64 = inpoint.serialized_size();
    BITCOIN_ASSERT(inpoint_size64 <= max_size_t);
    const auto inpoint_size = static_cast<size_t>(inpoint_size64);

    data_chunk result(code_size + inpoint_size);
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    auto raw_inpoint = inpoint.to_data();
    serial.write_data(raw_inpoint);

    handler(outgoing(request, result));
}

void blockchain::fetch_block_height(server_node& node,
    const incoming& request, send_handler handler)
{
    const auto& data = request.data;

    if (data.size() != hash_size)
    {
        log::error(LOG_SERVER)
            << "Incorrect data size for blockchain.fetch_block_height";
        return;
    }

    auto deserial = make_deserializer(data.begin(), data.end());
    const auto block_hash = deserial.read_hash();
    node.chain().fetch_block_height(block_hash,
        std::bind(&blockchain::block_height_fetched,
            _1, _2, request, handler));
}

void blockchain::block_height_fetched(const code& ec, size_t block_height,
    const incoming& request, send_handler handler)
{
    BITCOIN_ASSERT(block_height <= max_uint32);
    auto block_height32 = static_cast<uint32_t>(block_height);

    // error_code (4), height (4)
    data_chunk result(code_size + sizeof(uint32_t));
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);

    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);
    serial.write_4_bytes_little_endian(block_height32);

    handler(outgoing(request, result));
}

void blockchain::fetch_stealth(server_node& node, const incoming& request,
    send_handler handler)
{
    const auto& data = request.data;

    if (data.empty())
    {
        log::error(LOG_SERVER)
            << "Incorrect data size (empty) for blockchain.fetch_stealth";
        return;
    }

    auto deserial = make_deserializer(data.begin(), data.end());

    // number_bits
    const auto bitsize = deserial.read_byte();

    if (data.size() != sizeof(uint8_t) + binary::blocks_size(bitsize) +
        sizeof(uint32_t))
    {
        log::error(LOG_SERVER)
            << "Incorrect data size (" << data.size()
            << ") for blockchain.fetch_stealth";
        return;
    }

    // actual bitfield data
    const auto blocks = deserial.read_data(binary::blocks_size(bitsize));
    const binary prefix(bitsize, blocks);

    // from_height
    const uint64_t from_height = deserial.read_4_bytes_little_endian();

    node.chain().fetch_stealth(prefix, from_height,
        std::bind(&blockchain::stealth_fetched,
            _1, _2, request, handler));
}

void blockchain::stealth_fetched(const code& ec,
    const stealth_compact::list& stealth_results, const incoming& request,
    send_handler handler)
{
    // [ ephemeral_key_hash:32 ]
    // [ address_hash:20 ]
    // [ tx_hash:32 ]
    static constexpr size_t row_size = hash_size + short_hash_size + hash_size;
    data_chunk result(code_size + row_size * stealth_results.size());
    auto serial = make_serializer(result.begin());
    serial.write_error_code(ec);
    BITCOIN_ASSERT(serial.iterator() == result.begin() + code_size);

    for (const auto& row: stealth_results)
    {
        serial.write_hash(row.ephemeral_public_key_hash);
        serial.write_short_hash(row.public_key_hash);
        serial.write_hash(row.transaction_hash);
    }

    handler(outgoing(request, result));
}

} // namespace server
} // namespace libbitcoin
