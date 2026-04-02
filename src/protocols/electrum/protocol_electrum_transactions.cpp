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
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

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

    // TODO: requires tx pool in order to validate against unconfirmed txs.
    const code fault{ error::unconfirmable_transaction };

    ////const auto& query = archive();
    ////constexpr chain::context next_block_context{};
    ////fault = tx->check();
    ////fault = tx->guard_check();
    ////fault = tx->check(next_block_context);
    ////fault = tx->guard_check(next_block_context);
    ////query.populate_with_metadata(*tx);
    ////fault = tx->accept(next_block_context);
    ////fault = tx->guard_accept(next_block_context);
    ////fault = tx->confirm(next_block_context);
    ////fault = tx->connect(next_block_context);

    if (!fault)
    {
        // TODO: broadcast tx to p2p.
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

    size_t error_size{};
    ////const auto& query = archive();
    ////constexpr chain::context next_block_context{};
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

        // TODO: requires tx pool in order to validate against unconfirmed txs.
        const code fault{ error::unconfirmable_transaction };

        ////fault = tx->check();
        ////fault = tx->guard_check();
        ////fault = tx->check(next_block_context);
        ////fault = tx->guard_check(next_block_context);
        ////query.populate_with_metadata(*tx);
        ////fault = tx->accept(next_block_context);
        ////fault = tx->guard_accept(next_block_context);
        ////fault = tx->confirm(next_block_context);
        ////fault = tx->connect(next_block_context);

        if (fault)
        {
            const auto message = fault.message();
            error_size += message.size();
            errors.push_back(object_t
            {
                { "txid", encode_hash(tx->hash(false)) },
                { "error", message }
            });
        }
    }

    success = errors.empty();
    send_result(result, 42 + error_size, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_get(const code& ec,
    rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return;

    // TODO: changed in version 1.1: ignored height argument removed.
    // Requires additional same-name method implementation for v1.0.
    // This implies and override to channel_rpc<electrum>::dispatch().
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

    if (!verbose)
    {
        const auto tx = query.get_wire_tx(link, true);
        if (tx.empty())
        {
            send_code(error::server_error);
            return;
        }

        send_result(encode_base16(tx), two * tx.size(), BIND(complete, _1));
        return;
    }

    const auto tx = query.get_transaction(link, true);
    if (!tx)
    {
        send_code(error::server_error);
        return;
    }

    auto value = value_from(bitcoind(*tx));
    if (!value.is_object())
    {
        send_code(error::server_error);
        return;
    }

    if (const auto header = query.to_strong(link); !header.is_terminal())
    {
        using namespace system;
        const auto top = query.get_top_confirmed();
        const auto height = query.get_height(header);
        const auto block_hash = query.get_header_key(header);

        uint32_t timestamp{};
        if (height.is_terminal() || (block_hash == null_hash) ||
            !query.get_timestamp(timestamp, header))
        {
            send_code(error::server_error);
            return;
        }

        // Floor manages race between getting confirmed top and height.
        const auto confirms = add1(floored_subtract(top, height.value));

        auto& transaction = value.as_object();
        transaction["in_active_chain"] = true;
        transaction["blockhash"] = encode_hash(block_hash);
        transaction["confirmations"] = confirms;
        transaction["blocktime"] = timestamp;
        transaction["time"] = timestamp;
    }

    // Verbose means whatever bitcoind returns for getrawtransaction, lolz.
    const auto size = tx->serialized_size(true);
    send_result(std::move(value), two * size, BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_get_merkle(const code& ec,
    rpc_interface::blockchain_transaction_get_merkle, const std::string& tx_hash,
    double height) NOEXCEPT
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

    send_result(
    {
        object_t
        {
            { "merkle", std::move(branch) },
            { "block_height", block_height },
            { "pos", position }
        }
    }, two * hash_size * add1(branch.size()), BIND(complete, _1));
}

void protocol_electrum::handle_blockchain_transaction_id_from_pos(const code& ec,
    rpc_interface::blockchain_transaction_id_from_pos, double height,
    double tx_pos, bool merkle) NOEXCEPT
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

    send_result(
    {
        object_t
        {
            { "tx_hash", encode_hash(hash) },
            { "merkle", std::move(branch) }
        }
    }, two * hash_size * add1(branch.size()), BIND(complete, _1));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
