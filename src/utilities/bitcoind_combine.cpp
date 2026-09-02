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
#include <bitcoin/server/utilities/bitcoind_combine.hpp>

#include <set>
#include <vector>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace system::chain;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The endorsements ordered by public key position, verified against the
// subscript (scripts do not associate endorsements with their keys).
static operations merge_endorsements(const transaction& tx, uint32_t index,
    uint64_t value, const script& subscript, script_version version,
    const chunk_cptrs& endorsements) NOEXCEPT
{
    // Satoshi's op_check_multisig consumes an extra (zero) element.
    operations ops{ operation{ opcode::push_size_0 } };
    const auto complete = add1<size_t>(subscript.multisig_required());

    std::set<size_t> used{};
    for (const auto& key: subscript.multisig_keys())
    {
        if (ops.size() == complete)
            break;

        for (size_t sig{}; sig < endorsements.size(); ++sig)
        {
            const auto& endorse = endorsements.at(sig);
            if (used.contains(sig) || endorse->empty())
                continue;

            ec_signature signature{};
            const data_chunk der{ endorse->begin(), std::prev(endorse->end()) };
            if (!ecdsa::decode_signature(signature, der, false))
                continue;

            if (tx.check_signature(signature, *key, subscript, index, value,
                endorse->back(), version, flags::bip143_rule))
            {
                ops.push_back({ data_chunk{ *endorse }, false });
                used.insert(sig);
                break;
            }
        }
    }

    return ops;
}

code combine_input(input::cptr& out, const node::query& query,
    const transaction_cptrs& variants, uint32_t index) NOEXCEPT
{
    const auto& base = *variants.front();
    const auto& in = *base.inputs_ptr()->at(index);

    // The prevout must be known and unspent.
    const auto tx_link = query.to_tx(in.point().hash());
    const auto prevout = query.get_output(tx_link, in.point().index());
    if (!prevout || query.is_confirmed_spent(query.to_output(tx_link,
        in.point().index())))
        return error::bitcoind::verify_error;

    // A variant with fewer inputs contributes no candidate.
    std::vector<script::cptr> scripts{};
    std::vector<witness::cptr> witnesses{};
    for (const auto& variant: variants)
    {
        if (index >= variant->inputs_ptr()->size())
            continue;

        const auto& input = *variant->inputs_ptr()->at(index);
        scripts.push_back(input.script_ptr());
        witnesses.push_back(input.witness_ptr());
    }

    const auto value = prevout->value();
    const auto& pay = prevout->script();

    // Bare multisig, endorsements merged across candidate input scripts.
    if (!is_zero(pay.multisig_required()))
    {
        chunk_cptrs sigs{};
        for (const auto& candidate: scripts)
            for (const auto& sig: candidate->endorsements())
                sigs.push_back(sig);

        if (!sigs.empty())
        {
            out = emplace_shared<input>(in.point(), script{ merge_endorsements(
                base, index, value, pay, script_version::unversioned, sigs) },
                witness{}, in.sequence());
            return error::success;
        }
    }

    // p2sh, endorsements merged when the embedded script is multisig.
    if (script::is_pay_script_hash_pattern(pay.ops()))
    {
        for (const auto& candidate: scripts)
        {
            script embedded{};
            if (!candidate->extract_sigop_script(embedded, pay) ||
                is_zero(embedded.multisig_required()))
                continue;

            chunk_cptrs sigs{};
            for (const auto& other: scripts)
            {
                if (other->ops().empty())
                    continue;

                const operations partial
                {
                    other->ops().begin(), std::prev(other->ops().end())
                };

                for (const auto& sig: script{ partial }.endorsements())
                    sigs.push_back(sig);
            }

            auto ops = merge_endorsements(base, index, value, embedded,
                script_version::unversioned, sigs);
            ops.push_back({ embedded.to_data(false), false });
            out = emplace_shared<input>(in.point(), script{ std::move(ops) },
                witness{}, in.sequence());
            return error::success;
        }
    }

    // p2wsh, endorsements merged when the embedded script is multisig.
    if (script::is_pay_witness_script_hash_pattern(pay.ops()))
    {
        for (const auto& candidate: witnesses)
        {
            const auto& stack = candidate->stack();
            if (stack.empty())
                continue;

            const script embedded{ *stack.back(), false };
            if (is_zero(embedded.multisig_required()))
                continue;

            // The stack is the sign_multisig script form, without the script.
            chunk_cptrs sigs{};
            for (const auto& other: witnesses)
            {
                const auto& elements = other->stack();
                if (elements.size() < two)
                    continue;

                for (auto it = std::next(elements.begin());
                    it != std::prev(elements.end()); ++it)
                    if (is_endorsement(**it))
                        sigs.push_back(*it);
            }

            chunk_cptrs stacked{ to_shared<data_chunk>() };
            for (const auto& op: merge_endorsements(base, index, value,
                embedded, script_version::segwit, sigs))
                if (!op.data().empty())
                    stacked.push_back(op.data_ptr());

            stacked.push_back(stack.back());
            out = emplace_shared<input>(in.point(), script{},
                witness{ stacked }, in.sequence());
            return error::success;
        }
    }

    // Otherwise the first endorsing candidate wins (no partial forms).
    auto best_script = in.script_ptr();
    for (const auto& candidate: scripts)
    {
        if (!candidate->ops().empty())
        {
            best_script = candidate;
            break;
        }
    }

    auto best_witness = in.witness_ptr();
    for (const auto& candidate: witnesses)
    {
        if (!candidate->stack().empty())
        {
            best_witness = candidate;
            break;
        }
    }

    out = emplace_shared<input>(in.point_ptr(), best_script, best_witness,
        in.sequence());
    return error::success;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
