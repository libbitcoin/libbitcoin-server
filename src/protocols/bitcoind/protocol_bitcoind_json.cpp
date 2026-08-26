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
#include <bitcoin/server/protocols/protocol_bitcoind.hpp>

#include <iterator>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

// The createmultisig result, empty if a key is invalid or the p2sh embedded
// script exceeds one push element. An uncompressed key downgrades a segwit
// address type to legacy with a warning (as bitcoind).
network::rpc::object_t protocol_bitcoind::create_multisig(uint8_t required,
    const network::rpc::array_t& keys,
    const std::string& address_type) const NOEXCEPT
{
    using namespace chain;
    using namespace wallet;
    using namespace network::rpc;

    std::string list{};
    data_stack points{};
    auto compressed = true;
    points.reserve(keys.size());
    for (const auto& key: keys)
    {
        data_chunk point{};
        if (!std::holds_alternative<string_t>(key.value()) ||
            !decode_base16(point, std::get<string_t>(key.value())) ||
            !is_public_key(point))
            return {};

        compressed &= is_compressed_key(point);

        // Preceded by required in the descriptor list: multi(N,key,...).
        list += "," + encode_base16(point);
        points.push_back(std::move(point));
    }

    const script multisig{ script::to_pay_multisig_pattern(required, points) };
    const auto embedded = multisig.to_data(false);
    const auto type = compressed ? address_type : "legacy";
    if (type != "bech32" && embedded.size() > max_push_data_size)
        return {};

    std::string address{};
    auto body = "multi(" + std::to_string(required) + list + ")";
    if (type == "legacy")
    {
        address = payment_address{ multisig, p2sh_ }.encoded();
        body = "sh(" + body + ")";
    }
    else if (type == "bech32")
    {
        address = witness_address{ multisig, witness_ }.encoded();
        body = "wsh(" + body + ")";
    }
    else
    {
        const auto hash = sha256_hash(embedded);
        const script wsh{ script::to_pay_witness_pattern(0, hash) };
        address = payment_address{ wsh, p2sh_ }.encoded();
        body = "sh(wsh(" + body + "))";
    }

    object_t result
    {
        { "address", address },
        { "redeemScript", encode_base16(embedded) },
        { "descriptor", body + "#" + descriptor_checksum(body) }
    };

    if (type != address_type)
    {
        // This is ridiculous.
        result.emplace("warnings", array_t{ std::string{ "Unable to make "
            "chosen address type, please ensure no uncompressed public keys "
            "are present." } });
    }

    return result;
}

// The address of a singular output script (empty if unaddressable).
std::string protocol_bitcoind::to_address(
    const chain::script& script) const NOEXCEPT
{
    using namespace wallet;

    const auto& ops = script.ops();
    if (chain::script::is_pay_witness_pattern(ops))
    {
        // TODO: this should be an extractor (don't parse scripts).
        const auto code = ops.front().code();
        const auto& program = ops.at(1).data();
        const auto version = chain::operation::opcode_to_nonnegative(code);
        return witness_address{ program, version, witness_ }.encoded();
    }

    const auto pay = payment_address::extract_output(script, p2kh_, p2sh_);
    return pay ? pay.encoded() : std::string{};
}

// Inferred where a pattern is expressible, otherwise raw.
std::string protocol_bitcoind::infer_descriptor(
    const chain::script& script) const NOEXCEPT
{
    std::string body{};

    const auto& ops = script.ops();
    if (chain::script::is_pay_public_key_pattern(ops))
    {
        // TODO: this should be an extractor (don't parse scripts).
        body = "pk(" + encode_base16(ops.front().data()) + ")";
    }
    else if (chain::script::is_pay_multisig_pattern(ops))
    {
        // TODO: this should be an extractor (don't parse scripts).
        using namespace chain;
        const auto code = ops.front().code();
        body = "multi(" + std::to_string(operation::opcode_to_positive(code));
        for (auto op = std::next(ops.begin());
            op != std::prev(ops.end(), 2); ++op)
            body += "," + encode_base16(op->data());

        body += ")";
    }
    else
    {
        const auto address = to_address(script);
        body = address.empty() ?
            "raw(" + encode_base16(script.to_data(false)) + ")" :
            "addr(" + address + ")";
    }

    return body + "#" + descriptor_checksum(body);
}

} // namespace server
} // namespace libbitcoin
