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
#include <bitcoin/server/protocols/protocol_esplora.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_esplora

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Serializers.
// ----------------------------------------------------------------------------

// Type names are those of the esplora client, absent an interface spec.
std::string protocol_esplora::to_script_type(
    const chain::script& script) NOEXCEPT
{
    if (script.ops().empty())
        return "empty";

    switch (script.output_pattern())
    {
        case chain::script_pattern::pay_null_data:
            return "op_return";
        case chain::script_pattern::pay_multisig:
            return "multisig";
        case chain::script_pattern::pay_public_key:
            return "p2pk";
        case chain::script_pattern::pay_key_hash:
            return "p2pkh";
        case chain::script_pattern::pay_script_hash:
            return "p2sh";
        case chain::script_pattern::pay_witness_key_hash:
            return "v0_p2wpkh";
        case chain::script_pattern::pay_witness_script_hash:
            return "v0_p2wsh";
        case chain::script_pattern::pay_witness_v1_taproot:
            return "v1_p2tr";
        default:
            return "unknown";
    }
}

boost::json::object protocol_esplora::to_output(
    const chain::output& output) NOEXCEPT
{
    const auto& script = output.script();
    boost::json::object out
    {
        { "scriptpubkey", encode_base16(script.to_data(false)) },
        { "scriptpubkey_asm", script.to_string(flags_, true) },
        { "scriptpubkey_type", to_script_type(script) },
        { "value", output.value() }
    };

    // Unaddressable scripts carry no address.
    if (auto address = to_address(script, p2kh_, p2sh_, witness_);
        !address.empty())
        out["scriptpubkey_address"] = std::move(address);

    return out;
}

boost::json::object protocol_esplora::to_input(const chain::input& input,
    bool coinbase) NOEXCEPT
{
    const auto& script = input.script();
    const auto& point = input.point();

    boost::json::array witness{};
    for (const auto& stack: input.witness().stack())
        witness.emplace_back(encode_base16(*stack));

    boost::json::object out
    {
        { "txid", encode_hash(point.hash()) },
        { "vout", point.index() },
        { "is_coinbase", coinbase },
        { "scriptsig", encode_base16(script.to_data(false)) },
        { "scriptsig_asm", script.to_string(flags_, true) },
        { "witness", std::move(witness) },
        { "sequence", input.sequence() }
    };

    // Populated only when the prevout is resolved (never for coinbase).
    if (input.prevout)
        out["prevout"] = to_output(*input.prevout);

    return out;
}

boost::json::object protocol_esplora::to_status(
    const database::tx_link& link) NOEXCEPT
{
    const auto& query = archive();
    const auto block = query.find_confirmed_block(link);

    size_t height{};
    const auto header = query.get_header(block);
    if (!header || !query.get_height(height, block))
        return { { "confirmed", false } };

    return
    {
        { "confirmed", true },
        { "block_height", height },
        { "block_hash", encode_hash(query.get_header_key(block)) },
        { "block_time", header->timestamp() }
    };
}

bool protocol_esplora::to_tx(boost::json::object& out,
    const database::tx_link& link) NOEXCEPT
{
    auto& query = archive();
    const auto tx = query.get_transaction(link, true);
    if (!tx)
        return false;

    const auto coinbase = tx->is_coinbase();
    const auto nominal = tx->serialized_size(false);
    const auto maximal = tx->serialized_size(true);

    boost::json::array inputs{};
    boost::json::array outputs{};

    // Prevouts are required for the fee and for each input's prevout object.
    const auto populated = !coinbase && query.populate_with_metadata(*tx);

    for (const auto& input: *tx->inputs_ptr())
        inputs.emplace_back(to_input(*input, coinbase));

    for (const auto& output: *tx->outputs_ptr())
        outputs.emplace_back(to_output(*output));

    out =
    {
        { "txid", encode_hash(query.get_tx_key(link)) },
        { "version", tx->version() },
        { "locktime", tx->locktime() },
        { "vin", std::move(inputs) },
        { "vout", std::move(outputs) },
        { "size", maximal },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "fee", populated ? tx->fee() : zero },
        { "status", to_status(link) }
    };

    return true;
}

// Transaction methods.
// ----------------------------------------------------------------------------

bool protocol_esplora::handle_get_tx(const code& ec, interface::tx,
    uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = query.to_tx(*hash);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    if (media == json)
    {
        boost::json::object out{};
        if (!to_tx(out, link))
        {
            send_internal_server_error(database::error::integrity);
            return true;
        }

        send_json(std::move(out), 1024);
        return true;
    }

    const auto tx = query.get_transaction(link, true);
    if (!tx)
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    switch (media)
    {
        case data:
            send_chunk(tx->to_data(true));
            return true;
        case text:
            send_text(encode_base16(tx->to_data(true)));
            return true;
    }

    send_not_acceptable();
    return true;
}

bool protocol_esplora::handle_get_tx_status(const code& ec,
    interface::tx_status, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto link = archive().to_tx(*hash);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    send_json(to_status(link), 256);
    return true;
}

bool protocol_esplora::handle_get_tx_merkleblock_proof(const code& ec,
    interface::tx_merkleblock_proof, uint8_t media,
    const hash_cptr& hash) NOEXCEPT
{
    using namespace network::messages::peer;

    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.find_confirmed_block(*hash);
    if (!query.is_associated(link))
    {
        send_not_found();
        return true;
    }

    const auto header = query.get_header(link);
    const auto keys = query.get_tx_keys(link);
    if (!header || keys.empty())
    {
        send_internal_server_error(database::error::integrity);
        return true;
    }

    // The vector<bool> proxy iterator does not satisfy ranges algorithms.
    std::vector<bool> match(keys.size());
    std::transform(keys.begin(), keys.end(), match.begin(),
        [&hash](const auto& key) NOEXCEPT
        {
            return key == *hash;
        });

    if (!is_one(to_unsigned(std::count(match.cbegin(), match.cend(), true))))
    {
        send_not_found();
        return true;
    }

    const auto count = possible_narrow_cast<uint32_t>(keys.size());
    merkle_block merkle{ header, count, {}, {} };
    build_partial_merkle(merkle.flags, merkle.hashes, keys, match);

    const auto version = merkle_block::version_maximum;
    data_chunk out(merkle.size(version));
    merkle.serialize(version, out);
    send_text(encode_base16(out));
    return true;
}

bool protocol_esplora::handle_get_tx_merkle_proof(const code& ec,
    interface::tx_merkle_proof, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_tx(*hash);
    const auto block = query.find_confirmed_block(link);

    size_t height{};
    if (link.is_terminal() || block.is_terminal() ||
        !query.get_height(height, block))
    {
        send_not_found();
        return true;
    }

    auto hashes = query.get_tx_keys(block);
    const auto index = find_position(hashes, *hash);
    if (is_negative(index))
    {
        send_not_found();
        return true;
    }

    const auto position = to_unsigned(index);
    const auto proof = chain::block::merkle_branch(index, std::move(hashes));

    boost::json::array branch(proof.size());
    std::ranges::transform(proof, branch.begin(), [](const auto& item) NOEXCEPT
    {
        return encode_hash(item);
    });

    send_json(boost::json::object
    {
        { "block_height", height },
        { "merkle", std::move(branch) },
        { "pos", position }
    }, two * hash_size * add1(proof.size()));
    return true;
}

bool protocol_esplora::handle_get_tx_outspend(const code& ec,
    interface::tx_outspend, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_tx(*hash);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    boost::json::object out{};
    if (!to_outspend(out, *hash, index))
    {
        send_not_found();
        return true;
    }

    send_json(std::move(out), 256);
    return true;
}

bool protocol_esplora::handle_get_tx_outspends(const code& ec,
    interface::tx_outspends, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_tx(*hash);
    const auto tx = query.get_transaction(link, true);
    if (link.is_terminal() || !tx)
    {
        send_not_found();
        return true;
    }

    const auto count = tx->outputs_ptr()->size();
    boost::json::array out{};
    for (uint32_t index{}; index < count; ++index)
    {
        boost::json::object spend{};
        if (!to_outspend(spend, *hash, index))
        {
            send_internal_server_error(database::error::integrity);
            return true;
        }

        out.emplace_back(std::move(spend));
    }

    send_json(std::move(out), count * 256);
    return true;
}

// Broadcast.
// ----------------------------------------------------------------------------

code protocol_esplora::validate_tx(
    const chain::transaction& tx) const NOEXCEPT
{
    const auto& query = archive();
    const auto& settings = system_settings();
    const auto link = query.to_confirmed(query.get_top_confirmed());
    const auto key = query.get_header_key(link);
    const auto state = query.get_confirmed_chain_state(settings, key);

    // The store always has chain state for the confirmed top.
    if (!state)
        return database::error::integrity;

    // The context of the next block, in which a pool tx would confirm.
    const auto pool = chain::chain_state{ *state, settings }.context();
    return node::validate_transaction(tx, query, pool);
}

code protocol_esplora::broadcast_tx(
    const chain::transaction::cptr& tx) NOEXCEPT
{
    if (const auto ec = validate_tx(*tx))
        return ec;

    BROADCAST(network::messages::peer::transaction,
        to_shared<network::messages::peer::transaction>(tx));
    return error::success;
}

bool protocol_esplora::handle_broadcast(const code& ec, interface::broadcast,
    uint8_t media, const std::string& transaction) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    read::base16::copy hexer{ transaction };
    const auto tx = to_shared<chain::transaction>(hexer, true);
    if (!tx->is_valid() || !hexer.is_exhausted())
    {
        send_rejected(error::invalid_argument);
        return true;
    }

    if (const auto fault = broadcast_tx(tx))
    {
        send_rejected(fault);
        return true;
    }

    send_text(encode_hash(tx->hash(false)));
    return true;
}

bool protocol_esplora::handle_broadcast_package(const code& ec,
    interface::broadcast_package) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_not_implemented();
    return true;
}

// The spending status of one output, false if the output does not exist.
bool protocol_esplora::to_outspend(boost::json::object& out,
    const hash_digest& hash, uint32_t index) NOEXCEPT
{
    const auto& query = archive();
    const chain::point prevout{ hash, index };
    const auto spender = query.find_confirmed_spender(prevout);
    if (spender.is_terminal())
    {
        out = { { "spent", false } };
        return true;
    }

    const auto tx = query.to_input_tx(spender);
    if (tx.is_terminal())
        return false;

    // The input index is the position of the point within the spending tx.
    const auto points = query.to_points(tx);
    size_t position{};
    while (position < points.size() && points.at(position) != spender)
        ++position;

    if (position == points.size())
        return false;

    out =
    {
        { "spent", true },
        { "txid", encode_hash(query.get_tx_key(tx)) },
        { "vin", position },
        { "status", to_status(tx) }
    };

    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
