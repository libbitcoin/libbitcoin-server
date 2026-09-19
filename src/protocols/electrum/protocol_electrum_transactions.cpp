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
#include <bitcoin/server/protocols/protocol_electrum.hpp>

#include <ranges>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum

using namespace system;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The specification requires that at least 25 transactions are accepted.
constexpr size_t maximum_test_txs = 25;

// Electrum sends a single value param (invalid json-rpc). This is enabled via
// the lax json-rpc body value !strict option, mapping the singleton to array.
void protocol_electrum::handle_blockchain_transaction_broadcast(const code& ec,
    rpc_interface::blockchain_transaction_broadcast,
    const std::string& raw_tx) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_0))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    read::base16::copy hexer{ raw_tx };
    const auto tx = to_shared<chain::transaction>(hexer, true);
    if (!tx->is_valid() || !hexer.is_exhausted())
    {
        send_code(error::electrum::bad_request);
        return;
    }

    // A single tx is the minimal package.
    constexpr auto test = false;
    submit(to_shared(chain::transaction_cptrs{ tx }),
        test, BIND(handle_submit_tx, _1, _2, tx));
}

void protocol_electrum::handle_submit_tx(const code& ec, size_t,
    const chain::transaction::cptr& tx) NOEXCEPT
{
    POST(complete_submit_tx, ec, tx);
}

void protocol_electrum::complete_submit_tx(const code& ec,
    const chain::transaction::cptr& tx) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (!ec)
    {
        send_result(encode_hash(tx->hash(false)));
        return;
    }

    if (!at_least(electrum::version::v1_1))
    {
        send_result(ec.message());
        return;
    }

    using namespace error::electrum;
    send_code(translate(ec, daemon_error));
}

void protocol_electrum::handle_blockchain_transaction_broadcast_package(
    const code& ec, rpc_interface::blockchain_transaction_broadcast_package,
    const interface::value_t& raw_txs, bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_6))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    // Electrum documentation: "Exact structure depends on bitcoind impl and
    // version, and should not be relied upon... should be considered
    // experimental and better-suited for debugging." - do not support this.
    if (verbose)
    {
        send_code(error::electrum::bad_request);
        return;
    }

    if (!std::holds_alternative<array_t>(raw_txs.value()))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    const auto& txs_hex = std::get<array_t>(raw_txs.value());
    if (txs_hex.empty())
    {
        send_code(error::electrum::bad_request);
        return;
    }

    chain::transaction_cptrs txs{};
    txs.reserve(txs_hex.size());

    for (const auto& tx_hex: txs_hex)
    {
        if (!std::holds_alternative<string_t>(tx_hex.value()))
        {
            send_code(error::electrum::bad_request);
            return;
        }

        read::base16::copy hexer{ std::get<string_t>(tx_hex.value()) };
        const auto tx = to_shared<chain::transaction>(hexer, true);
        if (!tx->is_valid() || !hexer.is_exhausted())
        {
            send_code(error::electrum::bad_request);
            return;
        }

        txs.push_back(tx);
    }

    constexpr auto test = false;
    const auto package = to_shared<chain::transaction_cptrs>(std::move(txs));
    submit(package, test, BIND(handle_submit_package, _1, _2, package));
}

void protocol_electrum::handle_blockchain_transaction_testmempoolaccept(
    const code& ec, rpc_interface::blockchain_transaction_testmempoolaccept,
    const interface::value_t& raw_txs) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_7))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    if (!std::holds_alternative<array_t>(raw_txs.value()))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    const auto& txs_hex = std::get<array_t>(raw_txs.value());
    if (txs_hex.empty() || txs_hex.size() > maximum_test_txs)
    {
        send_code(error::electrum::bad_request);
        return;
    }

    chain::transaction_cptrs txs{};
    txs.reserve(txs_hex.size());

    for (const auto& tx_hex: txs_hex)
    {
        if (!std::holds_alternative<string_t>(tx_hex.value()))
        {
            send_code(error::electrum::bad_request);
            return;
        }

        read::base16::copy hexer{ std::get<string_t>(tx_hex.value()) };
        const auto tx = to_shared<chain::transaction>(hexer, true);
        if (!tx->is_valid() || !hexer.is_exhausted())
        {
            send_code(error::electrum::bad_request);
            return;
        }

        txs.push_back(tx);
    }

    constexpr auto test = true;
    const auto package = to_shared<chain::transaction_cptrs>(std::move(txs));
    submit(package, test, BIND(handle_test_package, _1, _2, package));
}

void protocol_electrum::handle_test_package(const code& ec, size_t,
    const chain::transactions_cptr& txs) NOEXCEPT
{
    POST(complete_test_package, ec, txs);
}

// The package is accepted as a whole, so its code applies to each of its txs.
void protocol_electrum::complete_test_package(const code& ec,
    const chain::transactions_cptr& txs) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    array_t out{};
    out.reserve(txs->size());

    for (const auto& tx: *txs)
    {
        object_t value
        {
            { "txid", encode_hash(tx->hash(false)) },
            { "wtxid", encode_hash(tx->hash(true)) },
            { "allowed", !ec }
        };

        if (ec)
            value.emplace("reason", ec.message());

        out.emplace_back(std::move(value));
    }

    send_result(std::move(out));
}

void protocol_electrum::handle_blockchain_transaction_get(const code& ec,
    rpc_interface::blockchain_transaction_get, const std::string& tx_hash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return;

    if ((!at_least(electrum::version::v1_0)) ||
        (!at_least(electrum::version::v1_2) && verbose))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    hash_digest hash{};
    if (!decode_hash(hash, tx_hash))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    const auto& query = archive();
    const auto link = query.to_tx(hash);
    if (link.is_terminal())
    {
        // This client may have broadcast it (there is no tx pool).
        if (const auto tx = retained_tx(hash))
        {
            send_retained_tx(*tx, verbose);
            return;
        }

        // electrumx passes tx lookup to its daemon, failing as daemon error.
        send_code(error::electrum::daemon_error);
        return;
    }

    size_t size{};
    boost::json::value value{};
    if (!verbose)
    {
        const auto tx = query.get_wire_tx(link, true);
        if (tx.empty())
        {
            send_code(error::electrum::daemon_error);
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
            send_code(error::electrum::daemon_error);
            return;
        }

        // Verbose is whatever bitcoind returns for getrawtransaction, lolz.
        value = value_from(bitcoind(*tx, flags_));
        if (!value.is_object())
        {
            send_code(error::electrum::daemon_error);
            return;
        }

        inject_tx_scripts(value.as_object(), *tx, p2kh_, p2sh_, witness_);

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
                send_code(error::electrum::daemon_error);
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

    send_result(std::move(value));
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
        send_code(error::electrum::bad_request);
        return;
    }

    hash_digest hash{};
    size_t block_height{};
    if (!to_integer(block_height, height) || !decode_hash(hash, tx_hash))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    const auto& query = archive();
    const auto block_link = query.to_confirmed(block_height);
    if (block_link.is_terminal())
    {
        send_code(error::electrum::bad_request);
        return;
    }

    auto hashes = query.get_tx_keys(block_link);
    if (hashes.empty())
    {
        send_code(error::electrum::daemon_error);
        return;
    }

    const auto index = find_position(hashes, hash);
    if (is_negative(index))
    {
        send_code(error::electrum::bad_request);
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
    });
}

void protocol_electrum::handle_blockchain_transaction_id_from_position(
    const code& ec, rpc_interface::blockchain_transaction_id_from_position,
    double height, double tx_pos, bool merkle) NOEXCEPT
{
    if (stopped(ec))
        return;

    if (!at_least(electrum::version::v1_4))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    size_t position{};
    size_t block_height{};
    if (!to_integer(block_height, height) ||
        !to_integer(position, tx_pos))
    {
        send_code(error::electrum::bad_request);
        return;
    }

    const auto& query = archive();
    const auto block_link = query.to_confirmed(block_height);
    const auto tx_link = query.get_position_tx(block_link, position);
    if (tx_link.is_terminal())
    {
        send_code(error::electrum::bad_request);
        return;
    }

    using namespace system;
    const auto hash = query.get_tx_key(tx_link);
    if (hash == null_hash)
    {
        send_code(error::electrum::daemon_error);
        return;
    }

    if (!merkle)
    {
        send_result(encode_hash(hash));
        return;
    }

    auto hashes = query.get_tx_keys(block_link);
    if (hashes.empty())
    {
        send_code(error::electrum::daemon_error);
        return;
    }

    if (position >= hashes.size())
    {
        send_code(error::electrum::bad_request);
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
    });
}

// utility
// ----------------------------------------------------------------------------

void protocol_electrum::handle_submit_package(const code& ec, size_t index,
    const chain::transactions_cptr& txs) NOEXCEPT
{
    POST(complete_submit_package, ec, index, txs);
}

// The package is accepted as a whole, so a failure is reported against the one
// tx that caused it, and the others are neither accepted nor in error.
void protocol_electrum::complete_submit_package(const code& ec, size_t index,
    const chain::transactions_cptr& txs) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    array_t errors{};
    size_t size{};

    if (ec)
    {
        auto message = ec.message();
        size = message.size();
        errors.push_back(object_t
        {
            { "txid", encode_hash(txs->at(index)->hash(false)) },
            { "error", std::move(message) }
        });
    }

    send_result(object_t
    {
        { "success", !ec },
        { "errors", std::move(errors) }
    });
}

// A retained tx is unconfirmed, so carries no block context.
void protocol_electrum::send_retained_tx(const chain::transaction& tx,
    bool verbose) NOEXCEPT
{
    if (!verbose)
    {
        const auto data = tx.to_data(true);
        send_result(encode_base16(data));
        return;
    }

    auto value = value_from(bitcoind(tx, flags_));
    if (!value.is_object())
    {
        send_code(error::electrum::daemon_error);
        return;
    }

    inject_tx_scripts(value.as_object(), tx, p2kh_, p2sh_, witness_);
    send_result(std::move(value));
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
