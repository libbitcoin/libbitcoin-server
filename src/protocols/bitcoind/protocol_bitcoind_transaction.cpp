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

    SUBSCRIBE_BITCOIND(handle_create_raw_transaction, _1, _2, _3, _4, _5, _6, _7);
    SUBSCRIBE_BITCOIND(handle_decode_raw_transaction, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_raw_transaction, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_send_raw_transaction, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_test_mempool_accept, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_analyze_psbt, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_combine_psbt, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_convert_to_psbt, _1, _2, _3, _4, _5, _6);
    SUBSCRIBE_BITCOIND(handle_create_psbt, _1, _2, _3, _4, _5, _6, _7, _8);
    SUBSCRIBE_BITCOIND(handle_decode_psbt, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_finalize_psbt, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_join_psbts, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_descriptor_process_psbt, _1, _2);
    SUBSCRIBE_BITCOIND(handle_utxo_update_psbt, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_abort_private_broadcast, _1, _2);
    SUBSCRIBE_BITCOIND(handle_get_private_broadcast_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_package, _1, _2);
    SUBSCRIBE_BITCOIND(handle_combine_raw_transaction, _1, _2);
    SUBSCRIBE_BITCOIND(handle_sign_raw_transaction_with_key, _1, _2);
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    constexpr auto witness = true;
    auto& query = archive();
    const auto link = query.to_tx(hash);
    const auto tx = query.get_transaction(link, witness);
    if (!tx)
    {
        send_error(error::bitcoind::invalid_address_or_key, txid, txid.size());
        return true;
    }

    enum verbosity : size_t
    {
        hexadecimal = 0,
        json_object = 1,
        json_verbose = 2
    };

    int64_t requested{};
    if (!to_integer(requested, verbose))
    {
        send_error(error::bitcoind::misc_error);
        return true;
    }

    // bitcoind clamps out of range verbosity.
    const auto level = limit<size_t>(requested, verbosity::json_verbose);

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
        inject_tx_prevouts(model.as_object(), query, *tx);
        model.as_object()["fee"] =
            tx->fee() / to_floating(chain::satoshi_per_bitcoin);
    }

    send_result(std::move(model), two * tx->serialized_size(witness));
    return true;
}

bool protocol_bitcoind_transaction::handle_send_raw_transaction(const code& ec,
    rpc_interface::send_raw_transaction, const std::string& hexstring,
    double, double) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    const auto tx = to_shared<chain::transaction>(data, true);
    if (!tx->is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    // A confirmed tx with any unspent output is reported without validating.
    const auto& query = archive();
    const auto link = query.to_tx(tx->hash(false));
    if (!link.is_terminal() && query.is_confirmed_tx(link))
    {
        const auto outs = query.to_outputs(link);
        const auto unspent = [&query](const auto& out) NOEXCEPT
        {
            return !query.is_confirmed_spent(out);
        };

        if (std::any_of(outs.begin(), outs.end(), unspent))
        {
            send_error(error::bitcoind::verify_already_in_utxo_set);
            return true;
        }
    }

    if (const auto fault = broadcast_tx(tx); fault)
    {
        using namespace error::bitcoind;

        // Absent and confirmed-spent inputs are missing coins (as bitcoind).
        const auto missing =
            (fault == system::error::missing_previous_output) ||
            (fault == system::error::confirmed_double_spend);

        send_error(translate(fault, missing ? verify_error : verify_rejected));
        return true;
    }

    send_result(encode_hash(tx->hash(false)), two * hash_size);
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
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    array_t results{};
    results.reserve(rawtxs.size());
    for (const auto& item: rawtxs)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        read::base16::copy hexer{ std::get<string_t>(item.value()) };
        const chain::transaction tx{ hexer, true };
        if (!tx.is_valid() || !hexer.is_exhausted())
        {
            send_error(error::bitcoind::deserialization_error);
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
            result.emplace("reject-reason", error::bitcoind::reject(fault));

        results.emplace_back(std::move(result));
    }

    const auto size = 128 * results.size();
    send_result(std::move(results), size);
    return true;
}

// Shared by createrawtransaction and createpsbt.
code protocol_bitcoind_transaction::build_transaction(chain::transaction& out,
    const array_t& inputs, const value_t& outputs, double locktime,
    bool replaceable, double version) const NOEXCEPT
{
    uint32_t lock_time{};
    if (!to_integer(lock_time, locktime))
        return error::bitcoind::invalid_parameter;

    // bitcoind bounds the version to the maximum standard (currently 3).
    uint32_t tx_version{};
    if (!to_integer(tx_version, version) || is_zero(tx_version) ||
        (tx_version > 3u))
        return error::bitcoind::invalid_parameter;

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
            return error::bitcoind::type_error;

        const auto& fields = std::get<object_t>(item.value());
        const auto txid_it = fields.find("txid");
        const auto vout_it = fields.find("vout");
        if (txid_it == fields.end() || vout_it == fields.end() ||
            !std::holds_alternative<string_t>(txid_it->second.value()) ||
            !std::holds_alternative<number_t>(vout_it->second.value()))
            return error::bitcoind::invalid_parameter;

        if (!decode_hash(hash, std::get<string_t>(txid_it->second.value())) ||
            !to_integer(vout, std::get<number_t>(vout_it->second.value())))
            return error::bitcoind::invalid_parameter;

        // An explicit sequence overrides the derived default.
        auto sequenced = sequence;
        const auto sequence_it = fields.find("sequence");
        if (sequence_it != fields.end() &&
            (!std::holds_alternative<number_t>(sequence_it->second.value()) ||
            !to_integer(sequenced,
                std::get<number_t>(sequence_it->second.value()))))
            return error::bitcoind::invalid_parameter;

        ins->push_back(to_shared<input>(point{ hash, vout }, script{},
            sequenced));
    }

    script script{};
    uint64_t satoshi{};
    const auto outs = std::make_shared<output_cptrs>();

    // Appends one address or data output from a name/value pair.
    const auto append = [&](const std::string& name,
        const value_t& item) NOEXCEPT -> code
    {
        // A data output carries a null data script and no value.
        if (name == "data")
        {
            data_chunk data{};
            if (!std::holds_alternative<string_t>(item.value()) ||
                !decode_base16(data, std::get<string_t>(item.value())) ||
                data.size() > max_null_data_size)
                return error::bitcoind::invalid_parameter;

            outs->push_back(to_shared<output>(zero,
                chain::script{ script::to_pay_null_data_pattern(data) }));
            return error::bitcoind::success;
        }

        if (!std::holds_alternative<number_t>(item.value()))
            return error::bitcoind::type_error;

        if (output_script(script, name, p2kh_, p2sh_, witness_))
            return error::bitcoind::invalid_address_or_key;

        const auto btc = std::get<number_t>(item.value());
        if (!to_integer(satoshi, btc * satoshi_per_bitcoin, true) ||
            satoshi > system_settings().max_money())
            return error::bitcoind::type_error;

        outs->push_back(to_shared<output>(satoshi, std::move(script)));
        return error::bitcoind::success;
    };

    // bitcoind accepts outputs as one object or an array of objects (which
    // permits address repetition).
    if (std::holds_alternative<object_t>(outputs.value()))
    {
        for (const auto& pair: std::get<object_t>(outputs.value()))
            if (const auto fault = append(pair.first, pair.second))
                return fault;
    }
    else if (std::holds_alternative<array_t>(outputs.value()))
    {
        for (const auto& element: std::get<array_t>(outputs.value()))
        {
            if (!std::holds_alternative<object_t>(element.value()))
                return error::bitcoind::type_error;

            for (const auto& pair: std::get<object_t>(element.value()))
                if (const auto fault = append(pair.first, pair.second))
                    return fault;
        }
    }
    else
    {
        return error::bitcoind::type_error;
    }

    out = { tx_version, ins, outs, lock_time };
    return error::bitcoind::success;
}

bool protocol_bitcoind_transaction::handle_create_raw_transaction(
    const code& ec, rpc_interface::create_raw_transaction,
    const array_t& inputs, const value_t& outputs, double locktime,
    bool replaceable, double version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::transaction tx{};
    if (const auto fault = build_transaction(tx, inputs, outputs, locktime,
        replaceable, version))
    {
        send_error(fault);
        return true;
    }

    constexpr auto witness = false;
    send_result(to_text(tx, tx.serialized_size(witness), witness), 400);
    return true;
}

bool protocol_bitcoind_transaction::handle_decode_raw_transaction(const code& ec,
    rpc_interface::decode_raw_transaction, const std::string& hexstring,
    const std::optional<bool>& iswitness) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    // Absent the hint, witness deserialization is tried first (as bitcoind).
    const auto witness = iswitness.value_or(true);
    auto tx = chain::transaction{ data, witness };
    if (!iswitness.has_value() && !tx.is_valid())
        tx = chain::transaction{ data, false };

    if (!tx.is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    send_result(value_from(bitcoind(tx)), two * tx.serialized_size(true));
    return true;
}

// PSBT methods.
// ----------------------------------------------------------------------------

using psbt_tx = wallet::psbt::transaction;

static std::string sighash_name(uint32_t type) NOEXCEPT
{
    std::string name{};
    switch (type & 0x03_u32)
    {
        case 0: name = "DEFAULT"; break;
        case 1: name = "ALL"; break;
        case 2: name = "NONE"; break;
        default: name = "SINGLE";
    }

    if (to_bool(type & 0x80_u32))
        name += "|ANYONECANPAY";

    return name;
}

static std::string to_key_path(const std_vector<uint32_t>& path) NOEXCEPT
{
    constexpr auto hardened = 0x80000000_u32;
    std::string out{ "m" };
    for (const auto& index: path)
    {
        out += "/" + std::to_string(index & ~hardened);
        if (to_bool(index & hardened))
            out += "'";
    }

    return out;
}

static object_t to_unknown(const wallet::psbt::entry::list& entries) NOEXCEPT
{
    object_t out{};
    for (const auto& entry: entries)
        out.emplace(encode_base16(entry.key), encode_base16(entry.value));

    return out;
}

static array_t to_derivations(
    const wallet::psbt::derivation::list& derivations) NOEXCEPT
{
    array_t out{};
    for (const auto& derived: derivations)
    {
        out.emplace_back(object_t
        {
            { "pubkey", encode_base16(derived.point) },
            { "master_fingerprint", encode_base16(to_little_endian(
                derived.origin.fingerprint)) },
            { "path", to_key_path(derived.origin.path) }
        });
    }

    return out;
}

static object_t decode_psbt_input(const wallet::psbt::input& in) NOEXCEPT
{
    using namespace chain;
    object_t entry{};

    if (in.non_witness_utxo)
        entry.emplace("non_witness_utxo",
            value_from(bitcoind(*in.non_witness_utxo)));

    if (in.witness_utxo)
    {
        entry.emplace("witness_utxo", object_t
        {
            { "amount", in.witness_utxo->value() /
                to_floating(satoshi_per_bitcoin) },
            { "scriptPubKey", value_from(bitcoind(in.witness_utxo->script())) }
        });
    }

    if (!in.partial_signatures.empty())
    {
        object_t signatures{};
        for (const auto& signature: in.partial_signatures)
            signatures.emplace(encode_base16(signature.keydata()),
                encode_base16(signature.value));

        entry.emplace("partial_signatures", std::move(signatures));
    }

    if (in.sighash_type.has_value())
        entry.emplace("sighash", sighash_name(in.sighash_type.value()));

    if (in.embedded_script)
        entry.emplace("redeem_script",
            value_from(bitcoind(*in.embedded_script)));

    if (in.witness_script)
        entry.emplace("witness_script",
            value_from(bitcoind(*in.witness_script)));

    if (!in.derivations.empty())
        entry.emplace("bip32_derivs", to_derivations(in.derivations));

    if (in.final_script_sig)
        entry.emplace("final_scriptSig",
            value_from(bitcoind(*in.final_script_sig)));

    if (in.final_script_witness)
    {
        array_t stack{};
        for (const auto& item: in.final_script_witness->stack())
            stack.emplace_back(encode_base16(*item));

        entry.emplace("final_scriptwitness", std::move(stack));
    }

    if (in.previous_txid.has_value())
        entry.emplace("previous_txid", encode_hash(in.previous_txid.value()));

    if (in.output_index.has_value())
        entry.emplace("output_index", in.output_index.value());

    if (in.sequence.has_value())
        entry.emplace("sequence", in.sequence.value());

    if (in.required_time_locktime.has_value())
        entry.emplace("time_locktime", in.required_time_locktime.value());

    if (in.required_height_locktime.has_value())
        entry.emplace("height_locktime", in.required_height_locktime.value());

    if (!in.others.empty())
        entry.emplace("unknown", to_unknown(in.others));

    return entry;
}

static object_t decode_psbt_output(const wallet::psbt::output& out) NOEXCEPT
{
    using namespace chain;
    object_t entry{};

    if (out.embedded_script)
        entry.emplace("redeem_script",
            value_from(bitcoind(*out.embedded_script)));

    if (out.witness_script)
        entry.emplace("witness_script",
            value_from(bitcoind(*out.witness_script)));

    if (!out.derivations.empty())
        entry.emplace("bip32_derivs", to_derivations(out.derivations));

    if (out.amount.has_value())
        entry.emplace("amount", out.amount.value() /
            to_floating(satoshi_per_bitcoin));

    if (out.script)
        entry.emplace("script", value_from(bitcoind(*out.script)));

    if (!out.others.empty())
        entry.emplace("unknown", to_unknown(out.others));

    return entry;
}

bool protocol_bitcoind_transaction::handle_decode_psbt(const code& ec,
    rpc_interface::decode_psbt, const std::string& psbt) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const psbt_tx doc(psbt);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    object_t result{};
    const auto version0 = (doc.version() == psbt_tx::version_0);
    if (version0)
        result.emplace("tx", value_from(bitcoind(doc.unsigned_tx())));

    if (!doc.xpubs().empty())
    {
        array_t xpubs{};
        for (const auto& key: doc.xpubs())
        {
            auto checked = key.key;
            append_checksum(checked);
            xpubs.emplace_back(object_t
            {
                { "xpub", encode_base58(checked) },
                { "master_fingerprint", encode_base16(to_little_endian(
                    key.origin.fingerprint)) },
                { "path", to_key_path(key.origin.path) }
            });
        }

        result.emplace("global_xpubs", std::move(xpubs));
    }

    result.emplace("psbt_version", doc.version());

    if (!version0)
    {
        result.emplace("tx_version", doc.tx_version());
        if (doc.fallback_locktime().has_value())
            result.emplace("fallback_locktime",
                doc.fallback_locktime().value());

        if (doc.tx_modifiable().has_value())
            result.emplace("tx_modifiable", doc.tx_modifiable().value());
    }

    if (!doc.others().empty())
        result.emplace("unknown", to_unknown(doc.others()));

    array_t ins{};
    for (const auto& in: doc.inputs())
        ins.emplace_back(decode_psbt_input(in));

    array_t outs{};
    for (const auto& out: doc.outputs())
        outs.emplace_back(decode_psbt_output(out));

    result.emplace("inputs", std::move(ins));
    result.emplace("outputs", std::move(outs));

    if (const auto fee = doc.fee(); fee.has_value())
        result.emplace("fee", fee.value() /
            to_floating(chain::satoshi_per_bitcoin));

    send_result(std::move(result), 2048);
    return true;
}

bool protocol_bitcoind_transaction::handle_analyze_psbt(const code& ec,
    rpc_interface::analyze_psbt, const std::string& psbt) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const psbt_tx doc(psbt);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    auto missing_utxo = false;
    array_t ins{};
    for (size_t index = 0; index < doc.inputs().size(); ++index)
    {
        const auto& in = doc.inputs().at(index);
        const auto utxo = !!doc.prevout(index);
        missing_utxo |= !utxo;

        object_t entry
        {
            { "has_utxo", utxo },
            { "is_final", in.is_final() }
        };

        if (!in.is_final())
        {
            array_t unsigned_keys{};
            for (const auto& derived: in.derivations)
            {
                const auto match = [&](const auto& signature) NOEXCEPT
                {
                    return signature.keydata() == derived.point;
                };

                if (std::none_of(in.partial_signatures.begin(),
                    in.partial_signatures.end(), match))
                    unsigned_keys.emplace_back(encode_base16(derived.point));
            }

            if (!unsigned_keys.empty())
                entry.emplace("missing", object_t
                {
                    { "signatures", std::move(unsigned_keys) }
                });

            entry.emplace("next", std::string{ utxo ? "signer" : "updater" });
        }

        ins.emplace_back(std::move(entry));
    }

    object_t result{ { "inputs", std::move(ins) } };

    if (const auto fee = doc.fee(); fee.has_value())
        result.emplace("fee", fee.value() /
            to_floating(chain::satoshi_per_bitcoin));

    if (doc.is_final())
    {
        const auto tx = doc.extract();
        const auto vsize = ceilinged_divide(tx.weight(),
            chain::light_weight_factor);
        result.emplace("estimated_vsize", vsize);

        if (const auto fee = doc.fee(); fee.has_value() && !is_zero(vsize))
            result.emplace("estimated_feerate", (fee.value() * 1000u) /
                to_floating(chain::satoshi_per_bitcoin) / vsize);

        result.emplace("next", std::string{ "extractor" });
    }
    else
    {
        result.emplace("next", std::string{ missing_utxo ? "updater" : "signer" });
    }

    send_result(std::move(result), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_combine_psbt(const code& ec,
    rpc_interface::combine_psbt, const array_t& txs) NOEXCEPT
{
    if (stopped(ec))
        return false;

    psbt_tx combined{};
    for (const auto& item: txs)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        psbt_tx doc(std::get<string_t>(item.value()));
        if (!doc)
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        if (combined && !combined.combine(doc))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        if (!combined)
            combined = std::move(doc);
    }

    if (!combined)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    send_result(combined.encoded(), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_convert_to_psbt(const code& ec,
    rpc_interface::convert_to_psbt, const std::string& hexstring,
    bool permitsigdata, const std::optional<bool>& iswitness,
    double psbt_version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Construction is bip370 only.
    if (psbt_version != 2.0)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    // Absent the hint, witness deserialization is tried first (as bitcoind).
    auto tx = chain::transaction{ data, iswitness.value_or(true) };
    if (!iswitness.has_value() && !tx.is_valid())
        tx = chain::transaction{ data, false };

    if (!tx.is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    const auto is_signed = [](const auto& in) NOEXCEPT
    {
        return !in->script().ops().empty() || !in->witness().stack().empty();
    };

    const auto& ins = *tx.inputs_ptr();
    if (std::any_of(ins.begin(), ins.end(), is_signed))
    {
        if (!permitsigdata)
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        // Strip signature data for the unsigned psbt transaction.
        const auto stripped = to_shared<chain::input_cptrs>();
        stripped->reserve(ins.size());
        for (const auto& in: ins)
            stripped->push_back(to_shared<chain::input>(in->point(),
                chain::script{}, chain::witness{}, in->sequence()));

        tx = { tx.version(), stripped, tx.outputs_ptr(), tx.locktime() };
    }

    const psbt_tx doc(tx);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    send_result(doc.encoded(), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_create_psbt(const code& ec,
    rpc_interface::create_psbt, const array_t& inputs,
    const value_t& outputs, double locktime, bool replaceable, double version,
    double psbt_version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Construction is bip370 only.
    if (psbt_version != 2.0)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    chain::transaction tx{};
    if (const auto fault = build_transaction(tx, inputs, outputs, locktime,
        replaceable, version))
    {
        send_error(fault);
        return true;
    }

    const psbt_tx doc(tx);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    send_result(doc.encoded(), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_finalize_psbt(const code& ec,
    rpc_interface::finalize_psbt, const std::string& psbt,
    bool extract) NOEXCEPT
{
    if (stopped(ec))
        return false;

    psbt_tx doc(psbt);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    const auto complete = doc.finalize();
    object_t result{};
    if (complete && extract)
    {
        constexpr auto witness = true;
        const auto tx = doc.extract();
        result.emplace("hex", encode_base16(tx.to_data(witness)));
    }
    else
    {
        result.emplace("psbt", doc.encoded());
    }

    result.emplace("complete", complete);
    send_result(std::move(result), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_join_psbts(const code& ec,
    rpc_interface::join_psbts, const array_t& txs) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (txs.size() < 2u)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    psbt_tx joined{};
    for (const auto& item: txs)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        psbt_tx doc(std::get<string_t>(item.value()));
        if (!doc)
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        if (joined && !joined.join(doc))
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }

        if (!joined)
            joined = std::move(doc);
    }

    send_result(joined.encoded(), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_descriptor_process_psbt(const code& ec,
    rpc_interface::descriptor_process_psbt) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_transaction::handle_utxo_update_psbt(const code& ec,
    rpc_interface::utxo_update_psbt, const std::string& psbt,
    const array_t& descriptors) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Descriptor expansion requires the descriptor engine (pending).
    if (!descriptors.empty())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    psbt_tx doc(psbt);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    const auto& query = archive();
    const auto version0 = (doc.version() == psbt_tx::version_0);
    for (size_t index = 0; index < doc.inputs().size(); ++index)
    {
        if (doc.prevout(index))
            continue;

        auto& in = doc.inputs().at(index);
        const auto& hash = version0 ?
            doc.unsigned_tx().inputs_ptr()->at(index)->point().hash() :
            in.previous_txid.value_or(null_hash);
        const auto vout = version0 ?
            doc.unsigned_tx().inputs_ptr()->at(index)->point().index() :
            in.output_index.value_or(0);

        const auto out = query.get_output(query.to_tx(hash), vout);
        if (!out)
            continue;

        // Only witness utxos are populated (as bitcoind).
        if (chain::script::is_pay_witness_pattern(out->script().ops()))
            in.witness_utxo = out;
    }

    send_result(doc.encoded(), 1024);
    return true;
}

bool protocol_bitcoind_transaction::handle_abort_private_broadcast(const code& ec,
    rpc_interface::abort_private_broadcast) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_transaction::handle_get_private_broadcast_info(const code& ec,
    rpc_interface::get_private_broadcast_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

bool protocol_bitcoind_transaction::handle_submit_package(const code& ec,
    rpc_interface::submit_package) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_transaction::handle_combine_raw_transaction(
    const code& ec, rpc_interface::combine_raw_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

// Signing is a wallet function, keys never transit the server.
bool protocol_bitcoind_transaction::handle_sign_raw_transaction_with_key(
    const code& ec, rpc_interface::sign_raw_transaction_with_key) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::method_not_found);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
