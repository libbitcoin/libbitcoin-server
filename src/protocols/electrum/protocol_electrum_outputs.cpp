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

#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void protocol_electrum::handle_blockchain_utxo_get_address(const code& ec,
    rpc_interface::blockchain_utxo_get_address, const std::string& tx_hash,
    double index) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (at_least(electrum::version::v1_1))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t offset{};
    hash_digest hash{};
    if (!to_integer(offset, index) ||
        !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto tx = query.to_tx(hash);
    if (tx.is_terminal())
    {
        ////send_code(error::not_found);
        send_result(null_t{}, 42, BIND(complete, _1));
        return;
    }

    const auto output = query.to_output(tx, offset);
    const auto script = query.get_output_script(output);
    if (!script)
    {
        send_code(error::server_error);
        return;
    }

    const auto address = extract_address(*script);
    if (!address)
    {
        send_result(null_t{}, 42, BIND(complete, _1));
        return;
    }

    send_result(address.encoded(), 56, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_outpoint_get_status(const code& ec,
    rpc_interface::blockchain_outpoint_get_status, const std::string& tx_hash,
    double txout_idx, const std::string& spk_hint) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    send_get_status(tx_hash, txout_idx, spk_hint);
}

void protocol_electrum::handle_blockchain_outpoint_subscribe(const code& ec,
    rpc_interface::blockchain_outpoint_subscribe, const std::string& tx_hash,
    double txout_idx, const std::string& spk_hint) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    if (!send_get_status(tx_hash, txout_idx, spk_hint))
        return;

    // TODO: collect the outpoint into a limited notification set.
    subscribed_outpoint_.store(false, std::memory_order_relaxed);
}

void protocol_electrum::handle_blockchain_outpoint_unsubscribe(const code& ec,
    rpc_interface::blockchain_outpoint_unsubscribe, const std::string& tx_hash,
    double txout_idx) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::wrong_version);
        return;
    }

    uint32_t index{};
    hash_digest hash{};
    if (!to_integer(index, txout_idx) ||
        !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    // TODO: remove outpoint subscription from notification set.
    chain::point prevout{ hash, index };
    const auto previous = subscribed_scriptpubkey_.load(
        std::memory_order_relaxed);

    send_result(previous, 16, BIND(complete, _1));
}

// utility.
// ----------------------------------------------------------------------------

bool protocol_electrum::send_get_status(const std::string& tx_hash,
    double txout_idx, const std::string& spk_hint) NOEXCEPT
{
    uint32_t index{};
    hash_digest hash{};
    if (!to_integer(index, txout_idx) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return false;
    }

    // This is parsed for correctness but is not used.
    // Script is advisory, and should match output script.
    if (!spk_hint.empty())
    {
        data_chunk bytes{};
        if (!decode_base16(bytes, spk_hint))
        {
            send_code(error::invalid_argument);
            return false;
        }

        chain::script script{ std::move(bytes), false };
        if (!script.is_valid())
        {
            send_code(error::invalid_argument);
            return false;
        }
    }

    const auto& query = archive();
    const auto tx = query.to_tx(hash);
    const auto output = query.to_output(tx, index);
    if (output.is_terminal())
    {
        send_code(error::not_found);
        return false;
    }

    // TODO: database query.///////////////////////////////////////////////////
    size_t height{ database::history::rooted_height };
    if (const auto block = query.find_confirmed_block(tx); block.is_terminal())
    {
        if (!query.is_confirmed_all_prevouts(tx))
            height = database::history::unrooted_height;
    }
    else if (!query.get_height(height, block))
    {
        send_code(error::server_error);
        return false;
    }
    ///////////////////////////////////////////////////////////////////////////

    // TODO: query tx spenders sorted history./////////////////////////////////
    const database::histories spenders{};
    ///////////////////////////////////////////////////////////////////////////

    auto result = object_t{ { "height", to_unsigned(height) } };
    if (!spenders.empty())
    {
        const auto& spender = spenders.front().tx;
        result["spender_txhash"] = encode_hash(spender.hash());
        result["spender_height"] = to_unsigned(spender.height());
    }

    send_result(std::move(result), 16, BIND(complete, _1));
    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
