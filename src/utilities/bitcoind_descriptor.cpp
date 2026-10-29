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
#include <bitcoin/server/utilities/bitcoind_descriptor.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/address.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

std::string infer_descriptor(const chain::script& script, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    std::string body{};

    const auto& key = script.public_key();
    const auto required = script.multisig_required();
    if (!key->empty())
    {
        body = "pk(" + encode_base16(*key) + ")";
    }
    else if (!is_zero(required))
    {
        body = "multi(" + std::to_string(required);
        for (const auto& point: script.multisig_keys())
            body += "," + encode_base16(*point);

        body += ")";
    }
    else
    {
        const auto address = to_address(script, p2kh, p2sh, witness);
        body = address.empty() ?
            "raw(" + encode_base16(script.to_data(false)) + ")" :
            "addr(" + address + ")";
    }

    return body + "#" + wallet::descriptor::to_checksum(body);
}

network::rpc::object_t create_multisig(uint8_t required,
    const network::rpc::array_t& keys, const std::string& address_type,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
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
        address = payment_address{ multisig, p2sh }.encoded();
        body = "sh(" + body + ")";
    }
    else if (type == "bech32")
    {
        address = witness_address{ multisig, witness }.encoded();
        body = "wsh(" + body + ")";
    }
    else
    {
        const auto hash = sha256_hash(embedded);
        const script wsh{ script::to_pay_witness_pattern(0, hash) };
        address = payment_address{ wsh, p2sh }.encoded();
        body = "sh(wsh(" + body + "))";
    }

    object_t result
    {
        { "address", address },
        { "redeemScript", encode_base16(embedded) },
        { "descriptor", body + "#" + wallet::descriptor::to_checksum(body) }
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

} // namespace server
} // namespace libbitcoin
