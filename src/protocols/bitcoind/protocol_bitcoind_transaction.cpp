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
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {

namespace server {

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

// The hint is unused (not required).
bool protocol_bitcoind_transaction::handle_get_raw_transaction(const code& ec,
    rpc_interface::get_raw_transaction, const std::string& txid,
    double verbose, const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, txid))
    {
        send_error(error::invalid_argument);
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

    enum verbosity : size_t
    {
        hexadecimal = 0,
        json_object = 1,
        json_verbose = 2
    };

    size_t level{};
    if (!to_integer(level, verbose) || level > verbosity::json_verbose)
    {
        send_error(error::invalid_argument);
        return true;
    }

    if (level == verbosity::hexadecimal)
    {
        send_text(to_text(*tx, tx->serialized_size(witness), witness));
        return true;
    }

    auto model = value_from(bitcoind(*tx));
    inject_tx_context(model.as_object(), query, link);
    if (level == verbosity::json_verbose && !tx->is_coinbase() &&
        query.populate_without_metadata(*tx))
    {
        size_t height{};
        auto entry = model.as_object().at("vin").as_array().begin();
        std::ranges::for_each(*tx->inputs_ptr(), [&](const auto& in) NOEXCEPT
        {
            const auto spent = query.to_tx(in->point().hash());
            if (query.get_tx_height(height, spent))
            {
                auto out = value_from(bitcoind(*in->prevout)).as_object();
                boost::json::object prevout
                {
                    { "generated", query.is_coinbase(spent) },
                    { "height", height },
                    { "value", out.at("value") },
                    { "scriptPubKey", std::move(out.at("scriptPubKey")) }
                };

                entry->as_object()["prevout"] = std::move(prevout);
            }

            ++entry;
        });

        model.as_object()["fee"] =
            tx->fee() / to_floating(chain::satoshi_per_bitcoin);
    }

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

    const auto tx = to_shared<chain::transaction>(data, true);
    if (!tx->is_valid())
    {
        send_error(error::invalid_argument);
        return true;
    }

    if (const auto fault = broadcast_tx(tx); fault)
    {
        send_error(fault);
        return true;
    }

    send_result(encode_hash(tx->hash(false)), two * system::hash_size);
    return true;
}

bool protocol_bitcoind_transaction::handle_test_mempool_accept(const code& ec,
    rpc_interface::test_mempool_accept, const array_t& rawtxs,
    double) NOEXCEPT
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

        const auto fault = validate_tx(tx);
        object_t result
        {
            { "txid", encode_hash(tx.hash(false)) },
            { "wtxid", encode_hash(tx.hash(true)) },
            { "allowed", !fault }
        };

        if (fault)
            result.emplace("reject-reason", fault.message());

        results.emplace_back(std::move(result));
    }

    const auto size = 128 * results.size();
    send_result(std::move(results), size);
    return true;
}

// Shared by createrawtransaction and createpsbt.
code protocol_bitcoind_transaction::build_transaction(chain::transaction& out,
    const array_t& inputs, const object_t& outputs, double locktime,
    bool replaceable) const NOEXCEPT
{
    uint32_t lock_time{};
    if (!to_integer(lock_time, locktime))
        return error::invalid_argument;

    using namespace chain;
    const auto sequence = replaceable ? messages::peer::bip125_sequence :
        (is_zero(lock_time) ? max_input_sequence : sub1(max_input_sequence));

    const auto ins = to_shared<input_cptrs>();
    ins->reserve(inputs.size());
    hash_digest hash{};
    uint32_t vout{};

    for (const auto& item: inputs)
    {
        if (!std::holds_alternative<object_t>(item.value()))
            return error::invalid_argument;

        const auto& fields = std::get<object_t>(item.value());
        const auto txid_it = fields.find("txid");
        const auto vout_it = fields.find("vout");
        if (txid_it == fields.end() || vout_it == fields.end() ||
            !std::holds_alternative<string_t>(txid_it->second.value()) ||
            !std::holds_alternative<number_t>(vout_it->second.value()))
            return error::invalid_argument;

        if (!decode_hash(hash, std::get<string_t>(txid_it->second.value())) ||
            !to_integer(vout, std::get<number_t>(vout_it->second.value())))
            return error::invalid_argument;

        ins->push_back(to_shared<input>(point{ hash, vout }, script{}, sequence));
    }

    script script{};
    uint64_t satoshi{};
    const auto outs = std::make_shared<output_cptrs>();
    outs->reserve(outputs.size());
    for (const auto& pair: outputs)
    {
        // A data output carries a null data script and no value.
        if (pair.first == "data")
        {
            data_chunk data{};
            if (!std::holds_alternative<string_t>(pair.second.value()) ||
                !decode_base16(data, std::get<string_t>(pair.second.value())) ||
                data.size() > max_null_data_size)
                return error::invalid_argument;

            outs->push_back(to_shared<output>(zero,
                chain::script{ script::to_pay_null_data_pattern(data) }));
            continue;
        }

        if (!std::holds_alternative<number_t>(pair.second.value()))
            return error::invalid_argument;

        if (const auto fault = output_script(script, pair.first, p2kh_, p2sh_,
            witness_))
            return fault;

        const auto btc = std::get<number_t>(pair.second.value());
        if (!to_integer(satoshi, btc * satoshi_per_bitcoin, false))
            return error::invalid_argument;

        outs->push_back(to_shared<output>(satoshi, std::move(script)));
    }

    out = { 1, ins, outs, lock_time };
    return error::success;
}

bool protocol_bitcoind_transaction::handle_create_raw_transaction(
    const code& ec, rpc_interface::create_raw_transaction,
    const array_t& inputs, const object_t& outputs, double locktime,
    bool replaceable) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::transaction tx{};
    if (const auto fault = build_transaction(tx, inputs, outputs, locktime,
        replaceable))
    {
        send_error(fault);
        return true;
    }

    constexpr auto witness = false;
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
