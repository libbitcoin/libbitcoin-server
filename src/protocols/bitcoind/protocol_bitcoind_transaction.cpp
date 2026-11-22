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

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

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
using namespace system::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

static bool is_amount(double value) NOEXCEPT
{
    uint64_t units{};
    return to_units(units, value, btc_decimal_places);
}

constexpr auto decode_failed =
    "TX decode failed. Make sure the tx has at least one input.";
constexpr auto inputs_missing = "Inputs missing or spent";
constexpr auto outputs_known = "Transaction outputs already in utxo set";

bool protocol_bitcoind_transaction::is_missing(const code& ec) NOEXCEPT
{
    return (ec == system::error::missing_previous_output) ||
        (ec == system::error::confirmed_double_spend);
}

bool protocol_bitcoind_transaction::has_unspent_output(
    const database::tx_link& link) const NOEXCEPT
{
    const auto& query = archive();
    const auto outs = query.to_outputs(link);
    return std::any_of(outs.begin(), outs.end(),
        [&query](const auto& out) NOEXCEPT
        {
            return !query.is_confirmed_spent(out);
        });
}

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
    SUBSCRIBE_BITCOIND(handle_submit_package, _1, _2, _3, _4, _5);
    SUBSCRIBE_BITCOIND(handle_combine_raw_transaction, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_sign_raw_transaction_with_key, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Raw transaction methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_transaction::handle_create_raw_transaction(
    const code& ec, rpc_interface::create_raw_transaction,
    const array_t& inputs, const value_t& outputs, double locktime,
    bool replaceable, double version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    chain::transaction tx{};
    if (const auto fault = parse_transaction(tx, inputs, outputs, locktime,
        replaceable, version, p2kh_, p2sh_, witness_,
        system_settings().max_money()))
    {
        send_error(fault);
        return true;
    }

    constexpr auto witness = false;
    send_result(to_text(tx, tx.serialized_size(witness), witness));
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

    // Absent the hint, witness deserialization is tried first.
    const auto witness = iswitness.value_or(true);
    auto tx = chain::transaction{ data, witness };
    if (!iswitness.has_value() && !tx.is_valid())
        tx = chain::transaction{ data, false };

    if (!tx.is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    auto model = value_from(bitcoind(tx, flags_));
    inject_tx_scripts(model.as_object(), tx, p2kh_, p2sh_, witness_);
    send_result(std::move(model));
    return true;
}

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
    size_t size{};
    if (query.is_pruned(link) || !query.get_tx_size(size, link, witness))
    {
        send_error(error::bitcoind::invalid_address_or_key, txid);
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
        if (!send_hex(two * size, [&query, link](const data_slab& out) NOEXCEPT
        {
            stream::out::fast sink{ out };
            write::base16::fast writer{ sink };
            return query.get_wire_tx(writer, link, witness) && writer;
        }))
            send_error(error::bitcoind::internal_error);

        return true;
    }

    const auto tx = query.get_transaction(link, witness);
    if (!tx)
    {
        send_error(error::bitcoind::invalid_address_or_key, txid);
        return true;
    }

    auto model = value_from(bitcoind(*tx, flags_));
    auto& object = model.as_object();
    inject_tx_context(object, query, link);
    inject_tx_scripts(object, *tx, p2kh_, p2sh_, witness_);
    if (level == verbosity::json_verbose && !tx->is_coinbase() &&
        query.populate_with_metadata(*tx))
    {
        inject_tx_prevouts(object, query, *tx, p2kh_, p2sh_, witness_, flags_);
        object["fee"] = to_btc(tx->fee());
    }

    send_result(std::move(model));
    return true;
}

bool protocol_bitcoind_transaction::handle_send_raw_transaction(const code& ec,
    rpc_interface::send_raw_transaction, const std::string& hexstring,
    double maxfeerate, double maxburnamount) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!is_amount(maxfeerate) || !is_amount(maxburnamount))
    {
        send_error(error::bitcoind::type_error);
        return true;
    }

    data_chunk data{};
    if (!decode_base16(data, hexstring))
    {
        send_error_message(error::bitcoind::deserialization_error,
            decode_failed);
        return true;
    }

    const auto tx = emplace_shared<chain::transaction>(data, true);
    if (!tx->is_valid())
    {
        send_error_message(error::bitcoind::deserialization_error,
            decode_failed);
        return true;
    }

    // A confirmed tx with any unspent output is reported without validating.
    const auto& query = archive();
    const auto link = query.to_tx(tx->hash(false));
    if (!link.is_terminal() && query.is_confirmed_tx(link) &&
        has_unspent_output(link))
    {
        send_rejected(node::error::duplicate_transaction, *tx);
        return true;
    }

    // A single tx is the minimal package.
    constexpr auto test = false;
    submit(to_shared(chain::transaction_cptrs{ tx }),
        test, BIND(handle_submit_tx, _1, _2, tx, gate()));
    return true;
}

void protocol_bitcoind_transaction::handle_submit_tx(const code& ec, size_t,
    const chain::transaction::cptr& tx, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_submit_tx, ec, tx, gate);
}

void protocol_bitcoind_transaction::complete_submit_tx(const code& ec,
    const chain::transaction::cptr& tx, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (ec)
    {
        send_rejected(ec, *tx);
        return;
    }

    send_result(encode_hash(tx->hash(false)));
}

void protocol_bitcoind_transaction::send_rejected(const code& ec,
    const chain::transaction& tx) NOEXCEPT
{
    using namespace error::bitcoind;
    if (ec == node::error::duplicate_transaction)
    {
        const auto& query = archive();
        const auto link = query.to_tx(tx.hash(false));
        if (!query.is_confirmed_tx(link))
            send_result(encode_hash(tx.hash(false)));
        else if (has_unspent_output(link))
            send_error_message(verify_already_in_utxo_set, outputs_known);
        else
            send_error_message(verify_error, inputs_missing);

        return;
    }

    if (is_missing(ec))
    {
        send_error_message(verify_error, inputs_missing);
        return;
    }

    const auto coded = translate(ec, verify_rejected);
    if (coded == verify_rejected)
        send_error_message(coded, reject(ec));
    else
        send_error(coded);
}

bool protocol_bitcoind_transaction::handle_test_mempool_accept(const code& ec,
    rpc_interface::test_mempool_accept, const array_t& rawtxs,
    double maxfeerate) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!is_amount(maxfeerate))
    {
        send_error(error::bitcoind::type_error);
        return true;
    }

    if (rawtxs.empty())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    chain::transaction_cptrs txs{};
    txs.reserve(rawtxs.size());
    for (const auto& item: rawtxs)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        read::base16::copy hexer{ std::get<string_t>(item.value()) };
        const auto tx = to_shared<chain::transaction>(hexer, true);
        if (!tx->is_valid() || !hexer.is_exhausted())
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        txs.push_back(tx);
    }

    constexpr auto test = true;
    const auto package = to_shared<chain::transaction_cptrs>(std::move(txs));
    submit(package, test, BIND(handle_test_package, _1, _2, package, gate()));
    return true;
}

void protocol_bitcoind_transaction::handle_test_package(const code& ec, size_t,
    const chain::transactions_cptr& txs, const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_test_package, ec, txs, gate);
}

// The package is accepted as a whole, so its code applies to each of its txs.
void protocol_bitcoind_transaction::complete_test_package(const code& ec,
    const chain::transactions_cptr& txs, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    array_t results{};
    results.reserve(txs->size());
    for (const auto& tx: *txs)
        results.emplace_back(test_result(ec, *tx));

    send_result(std::move(results));
}

object_t protocol_bitcoind_transaction::test_result(const code& ec,
    const chain::transaction& tx) NOEXCEPT
{
    object_t result
    {
        { "txid", encode_hash(tx.hash(false)) },
        { "wtxid", encode_hash(tx.hash(true)) },
        { "allowed", !ec }
    };

    // An accepted tx is populated, so its fee is known.
    if (!ec)
    {
        const auto fee = tx.fee();
        const auto vsize = tx.virtual_size();
        const auto rate = fee * 1'000u / possible_wide_cast<uint64_t>(vsize);
        result.emplace("vsize", to64(vsize));
        result.emplace("fees", object_t
        {
            { "base", to_btc(fee) },
            { "effective-feerate", to_btc(rate) },
            { "effective-includes", array_t{ encode_hash(tx.hash(true)) } }
        });

        return result;
    }

    std::string reason{ "missing-inputs" };
    if (ec != node::error::duplicate_transaction && !is_missing(ec))
        reason = error::bitcoind::reject(ec);

    if (ec == node::error::duplicate_transaction)
    {
        const auto& query = archive();
        const auto link = query.to_tx(tx.hash(false));
        if (!query.is_confirmed_tx(link))
            reason = "txn-already-in-mempool";
        else if (has_unspent_output(link))
            reason = "txn-already-known";
    }

    result.emplace("reject-reason", std::move(reason));
    return result;
}

// PSBT methods.
// ----------------------------------------------------------------------------

using psbt_tx = wallet::psbt::transaction;

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
    for (size_t index{}; index < doc.inputs().size(); ++index)
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
        result.emplace("fee", to_btc(fee.value()));

    if (doc.is_final())
    {
        const auto tx = doc.extract();
        const auto vsize = ceilinged_divide(tx.weight(),
            chain::light_weight_factor);
        result.emplace("estimated_vsize", to64(vsize));

        const auto fee = doc.fee();
        if (fee.has_value() && !is_zero(vsize))
        {
            const auto bytes = possible_wide_cast<uint64_t>(vsize);
            const auto rate = fee.value() * 1'000u / bytes;
            result.emplace("estimated_feerate", to_btc(rate));
        }

        result.emplace("next", std::string{ "extractor" });
    }
    else
    {
        result.emplace("next", std::string{ missing_utxo ? "updater" : "signer" });
    }

    send_result(std::move(result));
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

    send_result(combined.encoded());
    return true;
}

bool protocol_bitcoind_transaction::handle_convert_to_psbt(const code& ec,
    rpc_interface::convert_to_psbt, const std::string& hexstring,
    bool permitsigdata, const std::optional<bool>& iswitness,
    double psbt_version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Construction is bip174 (version 0) or bip370 (version 2).
    uint32_t document{};
    if (!to_integer(document, psbt_version) ||
        (document != psbt_tx::version_0 && document != psbt_tx::version_2))
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

    // Absent the hint, witness deserialization is tried first.
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

    const psbt_tx doc(tx, document);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    send_result(doc.encoded());
    return true;
}

bool protocol_bitcoind_transaction::handle_create_psbt(const code& ec,
    rpc_interface::create_psbt, const array_t& inputs,
    const value_t& outputs, double locktime, bool replaceable, double version,
    double psbt_version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // Construction is bip174 (version 0) or bip370 (version 2).
    uint32_t document{};
    if (!to_integer(document, psbt_version) ||
        (document != psbt_tx::version_0 && document != psbt_tx::version_2))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    chain::transaction tx{};
    if (const auto fault = parse_transaction(tx, inputs, outputs, locktime,
        replaceable, version, p2kh_, p2sh_, witness_,
        system_settings().max_money()))
    {
        send_error(fault);
        return true;
    }

    const psbt_tx doc(tx, document);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    send_result(doc.encoded());
    return true;
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
    {
        const auto& unsigned_tx = doc.unsigned_tx();
        auto tx = value_from(bitcoind(unsigned_tx, flags_));
        inject_tx_scripts(tx.as_object(), unsigned_tx, p2kh_, p2sh_, witness_);

        // bitcoind omits the hex of the decoded psbt transaction.
        tx.as_object().stable_erase("hex");
        result.emplace("tx", std::move(tx));
    }

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
    {
        auto entry = decode_psbt_input(in, p2kh_, p2sh_, witness_, flags_);
        ins.emplace_back(std::move(entry));
    }

    array_t outs{};
    for (const auto& out: doc.outputs())
    {
        auto entry = decode_psbt_output(out, p2kh_, p2sh_, witness_, flags_);
        outs.emplace_back(std::move(entry));
    }

    result.emplace("inputs", std::move(ins));
    result.emplace("outputs", std::move(outs));

    if (const auto fee = doc.fee(); fee.has_value())
        result.emplace("fee", to_btc(fee.value()));

    send_result(std::move(result));
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
    send_result(std::move(result));
    return true;
}

bool protocol_bitcoind_transaction::handle_join_psbts(const code& ec,
    rpc_interface::join_psbts, const array_t& txs) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (txs.size() < two)
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

    send_result(joined.encoded());
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

    using namespace wallet;
    descriptor::signing::list signings{};
    for (const auto& item: descriptors)
    {
        if (!expand_scan_signings(signings, item, context_))
        {
            send_error(error::bitcoind::invalid_address_or_key);
            return true;
        }
    }

    psbt_tx doc(psbt);
    if (!doc)
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    update_psbt(doc, archive(), signings);
    send_result(doc.encoded());
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
    rpc_interface::submit_package, const array_t& package, double maxfeerate,
    double maxburnamount) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (!is_amount(maxfeerate) || !is_amount(maxburnamount))
    {
        send_error(error::bitcoind::type_error);
        return true;
    }

    if (package.empty() || package.size() > maximum_package)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    chain::transaction_cptrs txs{};
    txs.reserve(package.size());
    for (const auto& item: package)
    {
        if (!std::holds_alternative<string_t>(item.value()))
        {
            send_error(error::bitcoind::type_error);
            return true;
        }

        read::base16::copy hexer{ std::get<string_t>(item.value()) };
        const auto tx = to_shared<chain::transaction>(hexer, true);
        if (!tx->is_valid() || !hexer.is_exhausted())
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        txs.push_back(tx);
    }

    constexpr auto test = false;
    const auto txs_ptr = to_shared<chain::transaction_cptrs>(std::move(txs));
    submit(txs_ptr, test, BIND(handle_package, _1, _2, txs_ptr, gate()));
    return true;
}

void protocol_bitcoind_transaction::handle_package(const code& ec,
    size_t index, const chain::transactions_cptr& txs,
    const gate_t::ptr& gate) NOEXCEPT
{
    POST(complete_package, ec, index, txs, gate);
}

void protocol_bitcoind_transaction::complete_package(const code& ec,
    size_t index, const chain::transactions_cptr& txs,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    system::json::object result{};
    if (!package_result(result, archive(), ec, index, *txs))
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    send_result(system::json::value{ std::move(result) });
}

bool protocol_bitcoind_transaction::handle_combine_raw_transaction(
    const code& ec, rpc_interface::combine_raw_transaction,
    const array_t& txs) NOEXCEPT
{
    if (stopped(ec))
        return false;

    using namespace chain;
    transaction_cptrs variants{};
    variants.reserve(txs.size());
    for (const auto& item: txs)
    {
        data_chunk data{};
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_base16(data, std::get<string_t>(item.value())))
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        const auto tx = emplace_shared<transaction>(data, true);
        if (!tx->is_valid())
        {
            send_error(error::bitcoind::deserialization_error);
            return true;
        }

        variants.push_back(tx);
    }

    if (variants.empty())
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    const auto& base = *variants.front();
    const auto ins = to_shared<input_cptrs>();
    ins->reserve(base.inputs_ptr()->size());

    const auto inputs = base.inputs_ptr()->size();
    for (uint32_t index{}; index < inputs; ++index)
    {
        input::cptr combined{};
        if (const auto fault = combine_input(combined, archive(), variants,
            index))
        {
            send_error(fault);
            return true;
        }

        ins->push_back(combined);
    }

    constexpr auto witness = true;
    const auto outs = base.outputs_ptr();
    const transaction merged{ base.version(), ins, outs, base.locktime() };
    send_result(to_text(merged, merged.serialized_size(witness), witness));
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
