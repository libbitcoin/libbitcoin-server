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
#include <bitcoin/server/protocols/protocol_bitcoind_mining.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_bitcoind_mining
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

// Start.
// ----------------------------------------------------------------------------

void protocol_bitcoind_mining::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    SUBSCRIBE_BITCOIND(handle_get_network_hash_ps, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_get_mining_info, _1, _2);
    SUBSCRIBE_BITCOIND(handle_submit_block, _1, _2, _3, _4);
    SUBSCRIBE_BITCOIND(handle_submit_header, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_block_template, _1, _2, _3);
    SUBSCRIBE_BITCOIND(handle_get_prioritised_transactions, _1, _2);
    SUBSCRIBE_BITCOIND(handle_prioritise_transaction, _1, _2);
    protocol_bitcoind_dispatch<rpc_interface>::start();
}

// Mining methods.
// ----------------------------------------------------------------------------

bool protocol_bitcoind_mining::handle_get_network_hash_ps(const code& ec,
    rpc_interface::get_network_hash_ps, double nblocks,
    double height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();

    // A negative height selects the confirmed top.
    size_t target{};
    if (height < 0)
    {
        target = top;
    }
    else if (!to_integer(target, height))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }
    else
    {
        target = std::min(target, top);
    }

    // A non-positive window selects the span since the last retarget.
    size_t window{};
    if (nblocks <= 0)
    {
        window = add1(target % system_settings().retargeting_interval());
    }
    else if (!to_integer(window, nblocks))
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    window = std::min(window, target);
    if (is_zero(window))
    {
        send_result(to64(zero));
        return true;
    }

    // The pinned ancestry makes the window walk reorg-stable.
    database::header_links branch{};
    if (!query.get_ancestry(branch, query.to_confirmed(target), add1(window)))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    // The window timespan is bounded by its observed timestamps.
    auto minimum = max_uint32;
    auto maximum = min_uint32;
    for (const auto& ancestor: branch)
    {
        const auto header = query.get_header(ancestor);
        if (!header)
        {
            send_error(error::bitcoind::internal_error);
            return true;
        }

        minimum = std::min(minimum, header->timestamp());
        maximum = std::max(maximum, header->timestamp());
    }

    if (minimum == maximum)
    {
        send_result(to64(zero));
        return true;
    }

    uint256_t start_work{};
    uint256_t end_work{};
    if (!query.get_branch_work(start_work, branch.back()) ||
        !query.get_branch_work(end_work, branch.front()))
    {
        send_error(error::bitcoind::internal_error);
        return true;
    }

    const auto work = (end_work - start_work).convert_to<double>();
    send_result(work / (maximum - minimum));
    return true;
}

bool protocol_bitcoind_mining::handle_get_mining_info(const code& ec,
    rpc_interface::get_mining_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    get_pool_txs(BIND(handle_mining_info, _1, _2, gate()));
    return true;
}

void protocol_bitcoind_mining::handle_mining_info(const code& ec,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_mining_info, ec, links, gate);
}

// bitcoind omits currentblockweight/currentblocktx until block assembly.
void protocol_bitcoind_mining::do_mining_info(const code& ec,
    const database::tx_links& links, const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace chain;

    if (stopped())
        return;

    if (ec && ec != node::error::pooling_disabled)
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    size_t height{};
    const auto& query = archive();
    const auto link = query.to_top_confirmed();
    const auto top = query.get_header(link);
    if (!top || !query.get_height(height, link))
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    // The pool state over the top block carries the next work required.
    const auto& bitcoin = system_settings();
    const auto key = query.get_header_key(link);
    const auto state = query.get_confirmed_chain_state(bitcoin, key);
    if (!state)
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    const chain_state pool{ *state, bitcoin };
    const header header{ 0, {}, {}, 0, pool.work_required(), 0 };
    object_t next_block
    {
        { "height", to64(add1(height)) },
        { "bits", encode_base16(to_big_endian(header.bits())) },
        { "difficulty", header.difficulty() },
        { "target", encode_hash(from_uintx(chain::compact::expand(header.bits()))) }
    };

    const auto minimum = to_floating(node_settings().pool.minimum_fee_rate_());
    const auto span = to_floating(power2<uint64_t>(32u));
    const auto period = bitcoin.block_spacing_seconds;

    // bitcoind OB1 error ("blocks" wants height).
    send_result(object_t
    {
        { "blocks", to64(height) },
        { "bits", encode_base16(to_big_endian(top->bits())) },
        { "difficulty", top->difficulty() },
        { "target", encode_hash(from_uintx(chain::compact::expand(top->bits()))) },
        { "networkhashps", top->difficulty() * span / period },
        { "pooledtx", to64(links.size()) },
        { "blockmintxfee", minimum / satoshi_per_bitcoin },
        { "chain", chain_name(bitcoin) },
        { "next", std::move(next_block) },
        { "warnings", array_t{} }
    });
}

// The node accepts only blocks that it requests, so there is no path for a
// client block, and a submitted block is by construction unrequested.
bool protocol_bitcoind_mining::handle_submit_block(const code& ec,
    rpc_interface::submit_block, const std::string&,
    const std::string&) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

// The response defers to organize completion (see dispatch).
bool protocol_bitcoind_mining::handle_submit_header(const code& ec,
    rpc_interface::submit_header, const std::string& hexdata) NOEXCEPT
{
    if (stopped(ec))
        return false;

    data_chunk data{};
    if (!decode_base16(data, hexdata))
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    const auto header = emplace_shared<chain::header>(data);
    if (!header->is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return true;
    }

    if (!archive().to_header(header->hash()).is_terminal())
    {
        send_result(null_t{});
        return true;
    }

    organize(header, BIND(handle_organize_header, _1, _2, gate()));
    return true;
}

void protocol_bitcoind_mining::handle_organize_header(const code& ec,
    size_t, const gate_t::ptr& gate) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_submit_header, ec, gate);
}

void protocol_bitcoind_mining::do_submit_header(const code& ec,
    const gate_t::ptr&) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace error::bitcoind;
    if (ec)
        send_error(translate(ec, verify_error));
    else
        send_result(null_t{});
}

// bip22: the template mode only (not bip23 proposals), without long polling.
// bip145: the client must declare the segwit rule.
bool protocol_bitcoind_mining::handle_get_block_template(const code& ec,
    rpc_interface::get_block_template,
    const object_t& template_request) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto mode = template_request.find("mode");
    if (mode != template_request.end())
    {
        const auto name = std::get_if<string_t>(&mode->second.value());
        if (is_null(name) || *name != "template")
        {
            send_error(error::bitcoind::invalid_parameter);
            return true;
        }
    }

    auto segwit = false;
    const auto rules = template_request.find("rules");
    if (rules != template_request.end())
    {
        const auto names = std::get_if<array_t>(&rules->second.value());
        if (!is_null(names))
        {
            segwit = std::ranges::any_of(*names, [](const auto& rule) NOEXCEPT
            {
                const auto name = std::get_if<string_t>(&rule.value());
                return !is_null(name) && *name == "segwit";
            });
        }
    }

    if (!segwit)
    {
        send_error(error::bitcoind::invalid_parameter);
        return true;
    }

    get_template(BIND(handle_template, _1, _2, _3, gate()));
    return true;
}

void protocol_bitcoind_mining::handle_template(const code& ec, size_t height,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_template, ec, height, links, gate);
}

// The template is of the block above the confirmed top, so one built for
// another height is requested again.
void protocol_bitcoind_mining::do_template(const code& ec, size_t height,
    const database::tx_links& links, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(stranded());
    using namespace chain;

    if (stopped())
        return;

    if (ec == node::error::pooling_disabled)
    {
        send_error(error::bitcoind::client_in_initial_download);
        return;
    }

    if (ec)
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    size_t top{};
    const auto& query = archive();
    const auto& bitcoin = system_settings();
    const auto link = query.to_top_confirmed();
    const auto key = query.get_header_key(link);
    const auto state = query.get_confirmed_chain_state(bitcoin, key);
    if (!state || !query.get_height(top, link))
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    if (add1(top) != height)
    {
        get_template(BIND(handle_template, _1, _2, _3, gate));
        return;
    }

    const chain_state pool{ *state, bitcoin };
    const auto ctx = pool.context();
    const auto bip141 = ctx.is_enabled(flags::bip141_rule);

    // A tx depends on the txs of the template that it spends, by position
    // from one (the coinbase is zero).
    uint64_t fees{};
    array_t transactions{};
    hashes witnesses{ null_hash };
    std::unordered_map<hash_digest, size_t> positions{};
    for (const auto& tx_link: links)
    {
        system::pool::algorithm::totals_t totals{};
        const auto tx = query.get_transaction(tx_link, true);
        if (!tx || !query.get_pooled_totals(totals, tx_link))
        {
            send_error(error::bitcoind::internal_error);
            return;
        }

        std::vector<size_t> parents{};
        for (const auto& input: *tx->inputs_ptr())
        {
            const auto it = positions.find(input->point().hash());
            if (it != positions.end() &&
                std::ranges::find(parents, it->second) == parents.end())
                parents.push_back(it->second);
        }

        array_t depends{};
        for (const auto parent: parents)
            depends.emplace_back(to64(parent));

        positions.emplace(tx->hash(false), add1(transactions.size()));
        witnesses.push_back(tx->hash(true));
        fees = ceilinged_add(fees, totals.fee);
        transactions.emplace_back(object_t
        {
            { "data", encode_base16(tx->to_data(bip141)) },
            { "txid", encode_hash(tx->hash(false)) },
            { "hash", encode_hash(tx->hash(true)) },
            { "depends", std::move(depends) },
            { "fee", totals.fee },
            { "sigops", totals.sigops },
            { "weight", to64(tx->weight()) }
        });
    }

    // bitcoind names the bip9 bit zero rules csv, and marks as required (!)
    // a rule that a client cannot ignore.
    array_t rules{};
    if (ctx.is_enabled(flags::bip68_rule))
        rules.emplace_back(string_t{ "csv" });

    if (bip141)
        rules.emplace_back(string_t{ "!segwit" });

    if (ctx.is_enabled(flags::bip341_rule))
        rules.emplace_back(string_t{ "taproot" });

    const auto bits = pool.work_required();
    const auto minimum = add1(ctx.median_time_past);
    const auto subsidy = block::subsidy(height,
        bitcoin.subsidy_interval_blocks, bitcoin.initial_subsidy(),
        bitcoin.forks.bip42);

    object_t result
    {
        { "capabilities", array_t{} },
        { "version", chain_state::signal_version(bitcoin) },
        { "rules", std::move(rules) },
        { "vbavailable", object_t{} },
        { "vbrequired", to64(zero) },
        { "previousblockhash", encode_hash(key) },
        { "transactions", std::move(transactions) },
        { "coinbaseaux", object_t{} },
        { "coinbasevalue", ceilinged_add(subsidy, fees) },
        { "longpollid", encode_hash(key) + std::to_string(links.size()) },
        { "target", encode_hash(from_uintx(chain::compact::expand(bits))) },
        { "mintime", minimum },
        { "mutable", array_t
            {
                string_t{ "time" },
                string_t{ "transactions" },
                string_t{ "prevblock" }
            }
        },
        { "noncerange", string_t{ "00000000ffffffff" } },
        { "sigoplimit", to64(bip141 ? max_fast_sigops : max_block_sigops) },
        { "sizelimit", to64(bip141 ? max_block_weight : max_block_size) },
        { "curtime", std::max(unix_time(), minimum) },
        { "bits", encode_base16(to_big_endian(bits)) },
        { "height", to64(height) }
    };

    // bip141: the commitment output script of the coinbase, over the witness
    // hashes of the template with the reserved value of zero.
    if (bip141)
    {
        const auto root = merkle_root(std::move(witnesses));
        const auto commitment = sha256::double_hash(root, null_hash);
        const data_chunk prefix{ 0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed };
        result.emplace("weightlimit", to64(max_block_weight));
        result.emplace("default_witness_commitment",
            encode_base16(splice(prefix, commitment)));
    }

    send_result(std::move(result));
}

bool protocol_bitcoind_mining::handle_get_prioritised_transactions(const code& ec,
    rpc_interface::get_prioritised_transactions) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

bool protocol_bitcoind_mining::handle_prioritise_transaction(const code& ec,
    rpc_interface::prioritise_transaction) NOEXCEPT
{
    if (stopped(ec)) return false;
    send_error(error::bitcoind::client_mempool_disabled);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
