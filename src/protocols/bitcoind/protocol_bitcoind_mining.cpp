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
    submit_t::start();
}

// Mining methods.
// ----------------------------------------------------------------------------

void protocol_bitcoind_mining::handle_get_network_hash_ps(const code& ec,
    rpc_interface::get_network_hash_ps, double nblocks,
    double height) NOEXCEPT
{
    if (stopped(ec))
        return;

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
        return;
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
        return;
    }

    window = std::min(window, target);
    if (is_zero(window))
    {
        send_result(to64(zero));
        return;
    }

    // The pinned ancestry makes the window walk reorg-stable.
    database::header_links branch{};
    if (!query.get_ancestry(branch, query.to_confirmed(target), add1(window)))
    {
        send_error(error::bitcoind::internal_error);
        return;
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
            return;
        }

        minimum = std::min(minimum, header->timestamp());
        maximum = std::max(maximum, header->timestamp());
    }

    if (minimum == maximum)
    {
        send_result(to64(zero));
        return;
    }

    uint256_t start_work{}, end_work{};
    if (!query.get_branch_work(start_work, branch.back()) ||
        !query.get_branch_work(end_work, branch.front()))
    {
        send_error(error::bitcoind::internal_error);
        return;
    }

    const auto work = (end_work - start_work).convert_to<double>();
    send_result(work / (maximum - minimum));
}

void protocol_bitcoind_mining::handle_get_mining_info(const code& ec,
    rpc_interface::get_mining_info) NOEXCEPT
{
    if (stopped(ec))
        return;

    get_confirmable_txs(BIND(handle_mining_info, _1, _2, gate()));
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

    const auto minimum = node_settings().pool.minimum_fee_rate_();
    const auto span = to_floating(power2<uint64_t>(32u));
    const auto period = bitcoin.block_spacing_seconds;

    // bitcoind "blocks" is a height, not a count.
    send_result(object_t
    {
        { "blocks", to64(height) },
        { "bits", encode_base16(to_big_endian(top->bits())) },
        { "difficulty", top->difficulty() },
        { "target", encode_hash(from_uintx(chain::compact::expand(top->bits()))) },
        { "networkhashps", top->difficulty() * span / period },
        { "pooledtx", to64(links.size()) },
        { "blockmintxfee", to_btc(minimum) },
        { "chain", chain_name(bitcoin) },
        { "next", std::move(next_block) },
        { "warnings", array_t{} }
    });
}

void protocol_bitcoind_mining::handle_submit_block(const code& ec,
    rpc_interface::submit_block, const std::string& hexdata,
    const std::string&) NOEXCEPT
{
    if (stopped(ec))
        return;

    data_chunk data{};
    if (!decode_base16(data, hexdata))
    {
        send_error(error::bitcoind::deserialization_error);
        return;
    }

    const auto header = emplace_shared<const chain::header>(data);
    const auto block = emplace_shared<const chain::view::block>(
        std::move(data), true);

    if (!header->is_valid() || !block->is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return;
    }

    submit_block(header, block, gate());
}

// bip22: null when accepted, otherwise duplicate, inconclusive or reject token.
void protocol_bitcoind_mining::handle_submitted(const hash_digest&,
    submission outcome, const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    switch (outcome)
    {
        case submission::accepted:
        {
            send_result(null_t{});
            break;
        }
        case submission::duplicate:
        {
            send_result(string_t{ "duplicate" });
            break;
        }
        case submission::duplicate_inconclusive:
        {
            send_result(string_t{ "duplicate-inconclusive" });
            break;
        }
        case submission::duplicate_invalid:
        {
            send_result(string_t{ "duplicate-invalid" });
            break;
        }
        case submission::inconclusive:
        {
            send_result(string_t{ "inconclusive" });
            break;
        }
        case submission::bad_parent:
        {
            send_result(string_t{ "bad-prevblk" });
            break;
        }
        case submission::rejected:
        {
            send_result(string_t{ error::bitcoind::reject(ec) });
            break;
        }
        case submission::failed:
        {
            send_error(error::bitcoind::internal_error);
            break;
        }
    }
}

// The response defers to organize completion (see dispatch).
void protocol_bitcoind_mining::handle_submit_header(const code& ec,
    rpc_interface::submit_header, const std::string& hexdata) NOEXCEPT
{
    if (stopped(ec))
        return;

    data_chunk data{};
    if (!decode_base16(data, hexdata))
    {
        send_error(error::bitcoind::deserialization_error);
        return;
    }

    const auto header = emplace_shared<chain::header>(data);
    if (!header->is_valid())
    {
        send_error(error::bitcoind::deserialization_error);
        return;
    }

    if (!archive().to_header(header->hash()).is_terminal())
    {
        send_result(null_t{});
        return;
    }

    organize(header, BIND(handle_organize_header, _1, _2, gate()));
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

// bip22: template mode only (not bip23 proposals), without long polling.
// bip145: client must declare segwit rule.
void protocol_bitcoind_mining::handle_get_block_template(const code& ec,
    rpc_interface::get_block_template,
    const object_t& template_request) NOEXCEPT
{
    if (stopped(ec))
        return;

    const auto mode = template_request.find("mode");
    if (mode != template_request.end())
    {
        const auto name = std::get_if<string_t>(&mode->second.value());
        if (is_null(name) || *name != "template")
        {
            send_error(error::bitcoind::invalid_parameter);
            return;
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
        return;
    }

    get_template(BIND(handle_template, _1, _2, gate()));
}

void protocol_bitcoind_mining::handle_template(const code& ec,
    const node::block_template::cptr& held, const gate_t::ptr& gate) NOEXCEPT
{
    if (stopped())
        return;

    POST(do_template, ec, held, gate);
}

// A template not over the confirmed top is requested again.
void protocol_bitcoind_mining::do_template(const code& ec,
    const node::block_template::cptr& held, const gate_t::ptr& gate) NOEXCEPT
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

    const auto& query = archive();
    if (held->previous != query.get_top_confirmed_hash())
    {
        get_template(BIND(handle_template, _1, _2, gate));
        return;
    }

    const auto& bitcoin = system_settings();
    const auto& ctx = held->context;
    const auto height = ctx.height;
    const auto bip141 = ctx.is_enabled(flags::bip141_rule);

    // Tx depends on template txs it spends, by position from one (coinbase 0).
    array_t transactions{};
    transactions.reserve(held->txs.size());
    for (size_t index{}; index < held->txs.size(); ++index)
    {
        size_t light{};
        size_t heavy{};
        const auto& tx = held->txs.at(index);
        const auto& link = held->links.at(index);
        const auto data = query.get_wire_tx(link, bip141);
        if (data.empty() || !query.get_tx_sizes(light, heavy, link))
        {
            send_error(error::bitcoind::internal_error);
            return;
        }

        array_t depends{};
        depends.reserve(tx.parents.size());
        for (const auto parent: tx.parents)
            depends.emplace_back(to64(add1(parent)));

        const auto weight = ceilinged_add(
            ceilinged_multiply(base_size_contribution, light),
            ceilinged_multiply(total_size_contribution, heavy));

        transactions.emplace_back(object_t
        {
            { "data", encode_base16(data) },
            { "txid", encode_hash(tx.txid) },
            { "hash", encode_hash(tx.wtxid) },
            { "depends", std::move(depends) },
            { "fee", tx.fee },
            { "sigops", to64(tx.sigops) },
            { "weight", to64(weight) }
        });
    }

    // bitcoind names bip9 bit zero rules csv, and marks as required (!) a rule
    // a client cannot ignore.
    array_t rules{};
    if (ctx.is_enabled(flags::bip68_rule))
        rules.emplace_back(string_t{ "csv" });

    if (bip141)
        rules.emplace_back(string_t{ "!segwit" });

    if (ctx.is_enabled(flags::bip341_rule))
        rules.emplace_back(string_t{ "taproot" });

    const auto bits = ctx.work_required;
    const auto minimum = add1(ctx.median_time_past);
    const auto subsidy = block::subsidy(height,
        bitcoin.subsidy_interval_blocks, bitcoin.initial_subsidy(),
        bitcoin.forks.bip42);

    const auto previous = encode_hash(held->previous);
    const auto longpoll = previous + std::to_string(held->sequence);

    object_t result
    {
        { "capabilities", array_t{} },
        { "version", chain_state::signal_version(bitcoin) },
        { "rules", std::move(rules) },
        { "vbavailable", object_t{} },
        { "vbrequired", to64(zero) },
        { "previousblockhash", previous },
        { "transactions", std::move(transactions) },
        { "coinbaseaux", object_t{} },
        { "coinbasevalue", ceilinged_add(subsidy, held->fees) },
        { "longpollid", longpoll },
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

    // bip141: commitment output script of coinbase, over witness hashes of
    // template with reserved value of zero.
    if (bip141)
    {
        const data_chunk prefix{ 0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed };
        result.emplace("weightlimit", to64(max_block_weight));
        result.emplace("default_witness_commitment",
            encode_base16(splice(prefix, held->commitment)));
    }

    finish_template(result);
    send_result(std::move(result));
}

void protocol_bitcoind_mining::finish_template(object_t&) const NOEXCEPT
{
}

void protocol_bitcoind_mining::handle_get_prioritised_transactions(
    const code& ec, rpc_interface::get_prioritised_transactions) NOEXCEPT
{
    if (stopped(ec)) return;
    send_error(error::bitcoind::client_mempool_disabled);
}

void protocol_bitcoind_mining::handle_prioritise_transaction(const code& ec,
    rpc_interface::prioritise_transaction) NOEXCEPT
{
    if (stopped(ec)) return;
    send_error(error::bitcoind::client_mempool_disabled);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
