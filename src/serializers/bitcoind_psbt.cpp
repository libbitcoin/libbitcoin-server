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
#include <bitcoin/server/serializers/bitcoind_psbt.hpp>

#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/bitcoind_json.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

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

std::string to_key_path(const std::vector<uint32_t>& path) NOEXCEPT
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

object_t to_unknown(const wallet::psbt::entry::list& entries) NOEXCEPT
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
        const auto fingerprint = to_little_endian(derived.origin.fingerprint);
        out.emplace_back(object_t
        {
            { "pubkey", encode_base16(derived.point) },
            { "master_fingerprint", encode_base16(fingerprint) },
            { "path", to_key_path(derived.origin.path) }
        });
    }

    return out;
}

object_t decode_psbt_input(const wallet::psbt::input& in, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    using namespace chain;
    object_t entry{};

    if (in.non_witness_utxo)
    {
        const auto& tx = *in.non_witness_utxo;
        auto utxo = value_from(bitcoind(tx, flags));
        inject_tx_scripts(utxo.as_object(), tx, p2kh, p2sh, witness);
        entry.emplace("non_witness_utxo", std::move(utxo));
    }

    if (in.witness_utxo)
    {
        const auto& script = in.witness_utxo->script();
        constexpr auto satoshis = to_floating(satoshi_per_bitcoin);
        const auto key = to_script_public_key(script, p2kh, p2sh, witness, flags);
        entry.emplace("witness_utxo", object_t
        {
            { "amount", in.witness_utxo->value() / satoshis },
            { "scriptPubKey", key }
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
            value_from(bitcoind(*in.embedded_script, flags)));

    if (in.witness_script)
        entry.emplace("witness_script",
            value_from(bitcoind(*in.witness_script, flags)));

    if (!in.derivations.empty())
        entry.emplace("bip32_derivs", to_derivations(in.derivations));

    if (in.final_script_sig)
        entry.emplace("final_scriptSig",
            value_from(bitcoind_signature(*in.final_script_sig, flags)));

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

object_t decode_psbt_output(const wallet::psbt::output& out, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness, uint32_t flags) NOEXCEPT
{
    using namespace chain;
    object_t entry{};

    if (out.embedded_script)
        entry.emplace("redeem_script",
            value_from(bitcoind(*out.embedded_script, flags)));

    if (out.witness_script)
        entry.emplace("witness_script",
            value_from(bitcoind(*out.witness_script, flags)));

    if (!out.derivations.empty())
        entry.emplace("bip32_derivs", to_derivations(out.derivations));

    if (out.amount.has_value())
        entry.emplace("amount", out.amount.value() /
            to_floating(satoshi_per_bitcoin));

    if (out.script)
        entry.emplace("script", to_script_public_key(*out.script, p2kh, p2sh,
            witness, flags));

    if (!out.others.empty())
        entry.emplace("unknown", to_unknown(out.others));

    return entry;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
