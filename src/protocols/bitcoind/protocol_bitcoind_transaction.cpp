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
#include <bitcoin/server/protocols/protocol_bitcoind_transaction.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

// Isolate the subgroup dispatch metaprogramming to this translation unit.
template class network::rpc::dispatcher<
    server::interface::bitcoind_transaction>;

namespace server {

template class protocol_bitcoind_dispatch<interface::bitcoind_transaction>;

#define CLASS protocol_bitcoind_transaction
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

void protocol_bitcoind_transaction::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_create_raw_transaction, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_BITCOIND(handle_decode_raw_transaction, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_raw_transaction, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_send_raw_transaction, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_test_mempool_accept, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_analyze_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_combine_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_convert_to_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_create_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_decode_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_finalize_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_join_psbts, _1, _2);
    SUBSCRIBE_BITCOIND(handle_descriptor_process_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_utxo_update_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_abort_private_broadcast, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_private_broadcast_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_package, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Raw transaction methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_transaction::handle_get_raw_transaction(const code& ec,
    rpc_interface::get_raw_transaction, const std::string& txid,
    double verbose, const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // The blockhash hint is unused: libbitcoin archives all tx (global index).
    hash_digest hash{};
    if (!decode_hash(hash, txid))
    {
        send_error(error::not_found, txid, txid.size());
        return true;
    }

    constexpr auto witness = true;
    auto& query = archive();
    const auto link = query.to_tx(hash);
    const auto tx = query.get_transaction(link, witness);
    if (!tx)
    {
        send_error(error::not_found, txid, txid.size());
        return true;
    }

    // bitcoind parses verbose as an integer (ParseVerbosity): level zero yields
    // hex, nonzero yields the json object (verbosity 2 fee/prevout not yet done).
    size_t level{};
    if (!to_integer(level, verbose))
    {
        send_error(error::invalid_argument);
        return true;
    }

    if (level == zero)
    {
        send_text(to_text(*tx, tx->serialized_size(witness), witness));
        return true;
    }

    // bitcoind() (not bitcoind_verbose) yields bitcoind's tx fields: txid/hash/
    // size/vsize/weight/vin/vout/hex (bitcoind_verbose on a standalone tx
    // falls back to libbitcoin's plain inputs/outputs form).
    auto model = value_from(bitcoind(*tx));
    inject_tx_context(model.as_object(), query, link);
    send_result(std::move(model), two * tx->serialized_size(witness));
    return true;
}

bool protocol_bitcoind_transaction::handle_send_raw_transaction(const code& ec,
    rpc_interface::send_raw_transaction, const std::string& hexstring,
    double) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto tx = to_shared<const chain::transaction>(data, true);
    if (!tx->is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    // Tx archive not allowed in v4, must move through node::tx_chaser (v5).
    // See libbitcoin-node#1075.
    ////auto& query = archive();
    ////const auto hash = tx->hash(false);
    ////
    ////// Archive (so the out-relay can serve getdata) only if not already known.
    ////// TODO: contextual validation (populate_with_metadata + connect) for policy.
    ////if (query.to_tx(hash).is_terminal())
    ////{
    ////    if (tx->check())
    ////    {
    ////        send_error(error::invalid_argument);
    ////        return true;
    ////    }
    ////
    ////    if (query.set_code(*tx))
    ////    {
    ////        send_error(database::error::integrity);
    ////        return true;
    ////    }
    ////}

    // Full validation (TODO above) handled in broadcast_tx() below.
    ////// Announce to peers; protocol_transaction_out_106 serves the tx on getdata.
    ////broadcast<messages::peer::transaction>(
    ////    std::make_shared<const messages::peer::transaction>(
    ////        messages::peer::transaction{ tx }));

    if (const auto fault = broadcast_tx(tx); fault)
    {
        send_error(fault);
        return true;
    }

    send_result(encode_hash(tx->hash(false)), two * system::hash_size);
    return true;
}

// Validation runs against the confirmed chain (no tx pool until v5).
bool protocol_bitcoind_transaction::handle_test_mempool_accept(const code& ec,
    rpc_interface::test_mempool_accept, const array_t& rawtxs,
    uint32_t) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (rawtxs.empty())
    {
        send_error(error::invalid_argument);
        return true;
    }

    array_t results{};
    results.reserve(rawtxs.size());
    for (const auto& item: rawtxs)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        read::base16::copy hexer{ std::get<string_t>(item.value()) };
        const chain::transaction tx{ hexer, true };
        if (!tx.is_valid() || !hexer.is_exhausted())
        {
            send_error(error::invalid_argument);
            return true;
        }

        object_t result{};
        result.emplace("txid", encode_hash(tx.hash(false)));
        result.emplace("wtxid", encode_hash(tx.hash(true)));

        const auto fault = validate_tx(tx);
        result.emplace("allowed", !fault);
        if (fault)
            result.emplace("reject-reason", fault.message());

        results.emplace_back(std::move(result));
    }

    const auto size = 128 * results.size();
    send_result(std::move(results), size);
    return true;
}

bool protocol_bitcoind_transaction::handle_create_raw_transaction(const code& ec,
    rpc_interface::create_raw_transaction, const array_t& inputs,
    const object_t& outputs, uint32_t locktime, bool replaceable) NOEXCEPT
{
    if (stopped(ec))
        return false;

    using namespace chain;
    uint32_t vout{};
    hash_digest hash{};

    // bip125 signals replace-by-fee via a sequence below 0xfffffffe.
    constexpr auto bip125_sequence = 0xfffffffd_u32;
    const auto sequence = replaceable ? bip125_sequence : max_input_sequence;

    // The transaction owns shared inputs/outputs, so these are populated
    // directly, avoiding the intermediate vectors that to_shareds() copies.
    const auto ins = std::make_shared<input_cptrs>();
    ins->reserve(inputs.size());

    for (const auto& item: inputs)
    {
        if (!std::holds_alternative<object_t>(item.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        const auto& fields = std::get<object_t>(item.value());
        const auto txid_it = fields.find("txid");
        const auto vout_it = fields.find("vout");
        if (txid_it == fields.end() || vout_it == fields.end() ||
            !std::holds_alternative<string_t>(txid_it->second.value()) ||
            !std::holds_alternative<number_t>(vout_it->second.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        if (!decode_hash(hash, std::get<string_t>(txid_it->second.value())) ||
            !to_integer(vout, std::get<number_t>(vout_it->second.value())))
        {
            send_error(error::invalid_argument);
            return true;
        }

        ins->push_back(to_shared<input>(point{ hash, vout }, script{},
            sequence));
    }

    script script{};
    uint64_t satoshi{};
    const auto outs = std::make_shared<output_cptrs>();
    outs->reserve(outputs.size());
    for (const auto& pair: outputs)
    {
        if (!std::holds_alternative<number_t>(pair.second.value()))
        {
            send_error(error::invalid_argument);
            return true;
        }

        if (const auto fault = output_script(script, pair.first, p2kh_,
            p2sh_, witness_))
        {
            send_error(fault);
            return true;
        }

        const auto btc = std::get<number_t>(pair.second.value());
        if (!to_integer(satoshi, btc * satoshi_per_bitcoin, false))
        {
            send_error(error::invalid_argument);
            return true;
        }

        outs->push_back(to_shared<output>(satoshi, std::move(script)));
    }

    constexpr auto witness = false;
    const transaction tx{ 1, ins, outs, locktime };
    send_result(to_text(tx, tx.serialized_size(witness), witness), 400);
    return true;
}

bool protocol_bitcoind_transaction::handle_decode_raw_transaction(const code& ec,
    rpc_interface::decode_raw_transaction,
    const std::string& hexstring) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::invalid_argument);
        return true;
    }

    constexpr auto witness = true;
    const chain::transaction tx{ data, witness };
    if (!tx.is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    send_result(value_from(bitcoind(tx)), two * tx.serialized_size(witness));
    return true;
}

bool protocol_bitcoind_transaction::handle_analyze_psbt(const code& ec,
    rpc_interface::analyze_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_combine_psbt(const code& ec,
    rpc_interface::combine_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_convert_to_psbt(const code& ec,
    rpc_interface::convert_to_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_create_psbt(const code& ec,
    rpc_interface::create_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_decode_psbt(const code& ec,
    rpc_interface::decode_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_finalize_psbt(const code& ec,
    rpc_interface::finalize_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_join_psbts(const code& ec,
    rpc_interface::join_psbts) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_descriptor_process_psbt(const code& ec,
    rpc_interface::descriptor_process_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_utxo_update_psbt(const code& ec,
    rpc_interface::utxo_update_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_abort_private_broadcast(const code& ec,
    rpc_interface::abort_private_broadcast) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_get_private_broadcast_info(const code& ec,
    rpc_interface::get_private_broadcast_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_transaction::handle_submit_package(const code& ec,
    rpc_interface::submit_package) NOEXCEPT
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
