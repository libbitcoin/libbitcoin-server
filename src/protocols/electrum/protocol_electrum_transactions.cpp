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

#include <ranges>
#include <utility>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// BUGBUG: Electrum sends a single value param (invalid json-rpc).
// We don't support receiving invalid rpc, so requires array or object syntax.
void protocol_electrum::handle_blockchain_transaction_broadcast(const code& ec,
    rpc_interface::blockchain_transaction_broadcast,
    const std::string& raw_tx) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::wrong_version);
        return;
    }

    data_chunk tx_data{};
    if (!decode_base16(tx_data, raw_tx))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto tx = to_shared<chain::transaction>(tx_data, true);
    if (!tx->is_valid())
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto fault = broadcast_tx(tx);
    if (!fault)
    {
        send_result(encode_hash(tx->hash(false)), 42, BIND(complete, _1));
        return;
    }

    if (!at_least(electrum::version::v1_1))
    {
        send_result(fault.message(), 42, BIND(complete, _1));
        return;
    }

    send_code(fault);
}

void protocol_electrum::handle_blockchain_transaction_broadcast_package(
    const code& ec, rpc_interface::blockchain_transaction_broadcast_package,
    const interface::value_t& raw_txs, bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_6))
    {
        send_code(error::wrong_version);
        return;
    }

    // Electrum documentation: "Exact structure depends on bitcoind impl and
    // version, and should not be relied upon... should be considered
    // experimental and better-suited for debugging." - do not support this.
    if (verbose)
    {
        send_code(error::unsupported_argument);
        return;
    }

    if (!std::holds_alternative<array_t>(raw_txs.value()))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& txs_hex = std::get<array_t>(raw_txs.value());
    if (txs_hex.empty())
    {
        send_code(error::invalid_argument);
        return;
    }

    size_t size{};
    object_t result{ { "success", true }, { "errors", array_t{} } };
    auto& success = std::get<bool>(result["success"].value());
    auto& errors = std::get<array_t>(result["errors"].value());

    for (const auto& tx_hex: txs_hex)
    {
        if (!std::holds_alternative<string_t>(tx_hex.value()))
        {
            send_code(error::invalid_argument);
            return;
        }

        data_chunk tx_data{};
        if (!decode_base16(tx_data, std::get<string_t>(tx_hex.value())))
        {
            send_code(error::invalid_argument);
            return;
        }

        const auto tx = to_shared<chain::transaction>(tx_data, true);
        if (!tx->is_valid())
        {
            send_code(error::invalid_argument);
            return;
        }

        // TODO: this handles each transaction independently.
        const auto fault = broadcast_tx(tx);
        if (fault)
        {
            const auto message = fault.message();
            size += message.size();
            errors.push_back(object_t
            {
                { "txid", encode_hash(tx->hash(false)) },
                { "error", message }
            });
        }
    }

    success = errors.empty();
    send_result(result, 42 + size, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_get(const code& ec,
    rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return;

    // TODO: changed in version 1.1: ignored height argument removed.
    // This implies an override to channel_rpc<electrum>::dispatch() to strip
    // the height parameter in the case of negotiated v1.1.
    if ((!at_least(electrum::version::v1_0)) ||
        (!at_least(electrum::version::v1_2) && verbose))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto link = query.to_tx(hash);
    if (link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    size_t size{};
    boost::json::value value{};
    if (!verbose)
    {
        const auto tx = query.get_wire_tx(link, true);
        if (tx.empty())
        {
            send_code(error::server_error);
            return;
        }

        size = two * tx.size();
        value = encode_base16(tx);
    }
    else
    {
        const auto tx = query.get_transaction(link, true);
        if (!tx)
        {
            send_code(error::server_error);
            return;
        }

        // Verbose is whatever bitcoind returns for getrawtransaction, lolz.
        value = value_from(bitcoind(*tx));
        if (!value.is_object())
        {
            send_code(error::server_error);
            return;
        }

        size = two * tx->serialized_size(true);
        if (const auto block = query.find_strong(link); !block.is_terminal())
        {
            using namespace system;
            const auto top = query.get_top_confirmed();
            const auto height = query.get_height(block);
            const auto block_hash = query.get_header_key(block);

            uint32_t timestamp{};
            if (height.is_terminal() || (block_hash == null_hash) ||
                !query.get_timestamp(timestamp, block))
            {
                send_code(error::server_error);
                return;
            }

            // Floor manages race between getting confirmed top and height.
            const auto confirms = add1(floored_subtract(top, height.value));

            auto& object = value.as_object();
            object["in_active_chain"] = true;
            object["blockhash"] = encode_hash(block_hash);
            object["confirmations"] = confirms;
            object["blocktime"] = timestamp;
            object["time"] = timestamp;
        }
    }

    send_result(std::move(value), size, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_get_merkle(
    const code& ec, rpc_interface::blockchain_transaction_get_merkle,
    const std::string& tx_hash, double height) NOEXCEPT
{
    using namespace system;
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    hash_digest hash{};
    size_t block_height{};
    if (!to_integer(block_height, height) || !decode_hash(hash, tx_hash))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto block_link = query.to_confirmed(block_height);
    if (block_link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    auto hashes = query.get_tx_keys(block_link);
    if (hashes.empty())
    {
        send_code(error::server_error);
        return;
    }

    const auto index = find_position(hashes, hash);
    if (is_negative(index))
    {
        send_code(error::not_found);
        return;
    }

    using namespace chain;
    const auto position = to_unsigned(index);
    const auto proof = block::merkle_branch(index, std::move(hashes));

    array_t branch(proof.size());
    std::ranges::transform(proof, branch.begin(),
        [](const auto& hash) NOEXCEPT{ return encode_hash(hash); });

    send_result(object_t
    {
        { "merkle", std::move(branch) },
        { "block_height", block_height },
        { "pos", position }
    }, two * hash_size * add1(branch.size()), BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_id_from_position(
    const code& ec, rpc_interface::blockchain_transaction_id_from_position,
    double height, double tx_pos, bool merkle) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4))
    {
        send_code(error::wrong_version);
        return;
    }

    size_t position{};
    size_t block_height{};
    if (!to_integer(block_height, height) ||
        !to_integer(position, tx_pos))
    {
        send_code(error::invalid_argument);
        return;
    }

    const auto& query = archive();
    const auto block_link = query.to_confirmed(block_height);
    const auto tx_link = query.get_position_tx(block_link, position);
    if (tx_link.is_terminal())
    {
        send_code(error::not_found);
        return;
    }

    using namespace system;
    const auto hash = query.get_tx_key(tx_link);
    if (hash == null_hash)
    {
        send_code(error::server_error);
        return;
    }

    if (!merkle)
    {
        send_result(encode_hash(hash), two * hash_size, BIND(complete, _1));
        return;
    }

    auto hashes = query.get_tx_keys(block_link);
    if (hashes.empty())
    {
        send_code(error::server_error);
        return;
    }

    if (position >= hashes.size())
    {
        send_code(error::not_found);
        return;
    }

    using namespace chain;
    const auto proof = block::merkle_branch(position, std::move(hashes));

    array_t branch(proof.size());
    std::ranges::transform(proof, branch.begin(),
        [](const auto& hash) NOEXCEPT { return encode_hash(hash); });

    send_result(object_t
    {
        { "tx_hash", encode_hash(hash) },
        { "merkle", std::move(branch) }
    }, two * hash_size * add1(branch.size()), BIND(complete, _1));
}

// utility
// ----------------------------------------------------------------------------
// TODO: move this to node utility and pass through.

bool protocol_electrum::get_pool_context(chain::context& pool) const NOEXCEPT
{
    const auto& query = archive();
    const auto& settings = system_settings();
    const auto top = query.get_top_confirmed();
    const auto link = query.to_confirmed(top);
    const auto hash = query.get_header_key(link);
    const auto state = query.get_chain_state(settings, hash);
    if (!state) return false;
    pool = chain::chain_state(*state, settings).context();
    return true;
}

code protocol_electrum::validate_tx(
    const chain::transaction& tx) const NOEXCEPT
{
    chain::context ctx{};
    if (!get_pool_context(ctx))
        return error::server_error;

    code ec{};

    // Ensure tx does not violate tx consensus rules.
    if (!ec) ec = tx.check();
    if (!ec) ec = tx.check(ctx);
    if (!ec) archive().populate_with_metadata(tx, true);
    if (!ec) ec = tx.accept(ctx);
    if (!ec) ec = tx.confirm(ctx);
    if (!ec) ec = tx.connect(ctx);

    // Ensure tx does not violate presumed block consensus rules.
    // This is a DoS guard when validating a tx outside of a block.
    if (!ec) ec = tx.check_guard();
    if (!ec) ec = tx.check_guard(ctx);
    if (!ec) ec = tx.accept_guard(ctx);
    return ec;
}

code protocol_electrum::broadcast_tx(
    const chain::transaction::cptr& tx) NOEXCEPT
{
    if (const auto ec = validate_tx(*tx))
        return ec;

    BROADCAST(peer::transaction, to_shared<peer::transaction>(tx));
    return {};
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
