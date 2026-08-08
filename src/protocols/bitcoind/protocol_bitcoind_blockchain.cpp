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
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

#include <algorithm>
#include <utility>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace network::messages;
using namespace std::placeholders;
using namespace boost::json;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Blockchain methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_rpc::handle_get_best_block_hash(const code& ec,
    rpc_interface::get_best_block_hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto hash = archive().get_top_confirmed_hash();
    send_result(encode_hash(hash), two * system::hash_size);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block(const code& ec,
    rpc_interface::get_block, const std::string& blockhash,
    double verbosity) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    size_t level{};
    if (!to_integer(level, verbosity))
    {
        send_error(error::invalid_argument);
        return true;
    }

    constexpr auto witness = true;
    const auto& query = archive();
    const auto link = query.to_header(hash);

    if (level == zero)
    {
        const auto block = query.get_block(link, witness);
        if (!block)
        {
            send_error(error::not_found, blockhash, blockhash.size());
            return true;
        }

        send_text(to_text(*block, block->serialized_size(witness), witness));
        return true;
    }

    if (level == one || level == two)
    {
        const auto block = query.get_block(link, witness);
        if (!block)
        {
            send_error(error::not_found, blockhash, blockhash.size());
            return true;
        }

        // TODO: map "level/verbosity" to enumeration and remove comments.
        // verbosity 1 lists txids; verbosity 2 embeds full tx objects.
        auto model = is_one(level) ?
            value_from(bitcoind_hashed(*block)) :
            value_from(bitcoind_verbose(*block));

        inject_block_context(model.as_object(), query, link, block->header());
        send_result(std::move(model), two * block->serialized_size(witness));
        return true;
    }

    send_error(error::invalid_argument);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_chain_info(const code& ec,
    rpc_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto blocks = query.get_top_confirmed();
    const auto headers = query.get_top_candidate();
    const auto link = query.to_confirmed(blocks);
    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(database::error::integrity);
        return true;
    }

    // TODO: make utility method and move explanation there.
    // verificationprogress is approximated as confirmed/candidate height, the
    // best available estimate of the chain top during sync (1.0 once current).
    const auto progress = is_zero(headers) ? 1.0 :
        std::min(1.0, to_floating(blocks) / to_floating(headers));

    // Softfork activation is configured, not assumable. Taproot is reported
    // when configured active, with its configured activation height -- lnd's
    // backendSupportsTaproot requires the key's presence (not its field
    // values) before treating any btcd/bitcoind backend as usable.
    const auto& settings = system_settings();
    object_t soft_forks{};
    if (settings.forks.bip341 && settings.forks.bip342)
    {
        soft_forks.emplace("taproot", object_t
        {
            { "status", std::string{ "active" } },
            { "bit", 2 },
            { "startTime", -1 },
            { "timeout", -1 },
            { "since", settings.bip9_bit2_active_checkpoint.height() },
            { "min_activation_height", 0 }
        });
    }

    // TODO: blocks/headers is a misnomer (off-by-one), intended?
    using namespace chain;
    send_result(object_t
    {
        { "chain", chain_name(query) },
        { "blocks", blocks },
        { "headers", headers },
        { "bestblockhash", encode_hash(query.get_header_key(link)) },
        { "bits", encode_base16(to_big_endian(header->bits())) },
        { "target", encode_hash(from_uintx(compact::expand(header->bits()))) },
        { "difficulty", header->difficulty() },
        { "time", header->timestamp() },
        { "mediantime", median_time_past(query, link) },
        { "verificationprogress", progress },
        { "initialblockdownload", !is_current_chain(true) },
        { "pruned", network_settings().pruned_node() },
        { "warnings", std::string{} },
        { "bip9_softforks", std::move(soft_forks) }
    }, 512);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_count(const code& ec,
    rpc_interface::get_block_count) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto top = archive().get_top_confirmed();
    send_result(top, 20);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_filter(const code& ec,
    rpc_interface::get_block_filter, const std::string& blockhash,
    const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    const auto& query = archive();
    if (!query.filter_enabled())
    {
        send_error(error::not_implemented);
        return true;
    }

    const auto link = query.to_header(hash);
    data_chunk filter{};
    hash_digest filter_header{};
    if (!query.get_filter_body(filter, link) ||
        !query.get_filter_head(filter_header, link))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    send_result(object_t
    {
        { "filter", encode_base16(filter) },
        { "header", encode_hash(filter_header) }
    }, two * filter.size());
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_hash(const code& ec,
    rpc_interface::get_block_hash, double height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    size_t block_height{};
    if (!to_integer(block_height, height))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_confirmed(block_height);
    if (link.is_terminal())
    {
        send_error(error::not_found);
        return true;
    }

    send_result(encode_hash(query.get_header_key(link)), two * hash_size);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_header(const code& ec,
    rpc_interface::get_block_header, const std::string& blockhash,
    bool verbose) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest hash{};
    if (!decode_hash(hash, blockhash))
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(hash);
    const auto header = query.get_header(link);
    if (!header)
    {
        send_error(error::not_found, blockhash, blockhash.size());
        return true;
    }

    if (!verbose)
    {
        send_text(to_text(*header, chain::header::serialized_size()));
        return true;
    }

    auto out = header_to_bitcoind(*header);
    out["nTx"] = query.get_tx_count(link);
    inject_block_context(out, query, link, *header);
    send_result(value{ std::move(out) }, 512);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_stats(const code& ec,
    rpc_interface::get_block_stats, const value_t&, const array_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_chain_tx_stats(const code& ec,
    rpc_interface::get_chain_tx_stats, double, const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_tx_out(const code& ec,
    rpc_interface::get_tx_out, const std::string& txid, double n,
    bool) NOEXCEPT
{
    if (stopped(ec))
        return false;

    uint32_t index{};
    hash_digest hash{};
    if (!decode_hash(hash, txid) || !to_integer(index, n))
    {
        send_error(error::invalid_argument);
        return true;
    }

    const auto& query = archive();
    const auto output_link = query.to_output(hash, index);

    // TODO: is this meant to be query.is_confirmed_spent(output_link)?
    // bitcoind returns json null for missing or spent output (mempool ignored).
    if (output_link.is_terminal() || query.is_spent(output_link))
    {
        send_result({}, 42);
        return true;
    }

    const auto output = query.get_output(output_link);
    if (!output)
    {
        send_result({}, 42);
        return true;
    }

    // Output's tx must exist.
    const auto tx_link = query.to_tx(hash);
    if (tx_link.is_terminal())
    {
        send_error(error::server_error);
        return true;
    }

    // Derive header from top for consistent depth result (also cheaper).
    const auto top = query.get_top_confirmed();
    const auto header_link = query.to_confirmed(top);

    size_t height{};
    const auto strong = query.get_tx_height(height, tx_link);
    const auto depth = strong ? add1(floored_subtract(top, height)) : zero;
    const auto coins = to_floating(output->value()) /
        chain::satoshi_per_bitcoin;

    send_result(object_t
    {
        { "bestblock", encode_hash(query.get_header_key(header_link)) },
        { "confirmations", depth },
        { "value", coins },
        { "scriptPubKey", value_from(bitcoind(output->script())) },
        { "coinbase", query.is_coinbase(tx_link) }
    }, 256);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_tx_out_set_info(const code& ec,
    rpc_interface::get_tx_out_set_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_prune_block_chain(const code& ec,
    rpc_interface::prune_block_chain, double) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_save_mempool(const code& ec,
    rpc_interface::save_mempool) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_scan_tx_out_set(const code& ec,
    rpc_interface::scan_tx_out_set, const std::string&,
    const array_t&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_verify_chain(const code& ec,
    rpc_interface::verify_chain, double, double) NOEXCEPT
{
    if (stopped(ec))
        return false;

    // no-op, store is reliable (integrity is guarded by the flush lock).
    send_result(value{ true }, 8);
    return true;
}

bool protocol_bitcoind_rpc::handle_dump_tx_out_set(const code& ec,
    rpc_interface::dump_tx_out_set) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_load_tx_out_set(const code& ec,
    rpc_interface::load_tx_out_set) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_tx_out_proof(const code& ec,
    rpc_interface::get_tx_out_proof) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_verify_tx_out_proof(const code& ec,
    rpc_interface::verify_tx_out_proof) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_block_from_peer(const code& ec,
    rpc_interface::get_block_from_peer) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_chain_states(const code& ec,
    rpc_interface::get_chain_states) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_chain_tips(const code& ec,
    rpc_interface::get_chain_tips) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_deployment_info(const code& ec,
    rpc_interface::get_deployment_info) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_descriptor_activity(const code& ec,
    rpc_interface::get_descriptor_activity) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_get_difficulty(const code& ec,
    rpc_interface::get_difficulty) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_precious_block(const code& ec,
    rpc_interface::precious_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_scan_blocks(const code& ec,
    rpc_interface::scan_blocks) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_wait_for_block(const code& ec,
    rpc_interface::wait_for_block) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_wait_for_block_height(const code& ec,
    rpc_interface::wait_for_block_height) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::not_implemented);
    return true;
}

bool protocol_bitcoind_rpc::handle_wait_for_new_block(const code& ec,
    rpc_interface::wait_for_new_block) NOEXCEPT
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
