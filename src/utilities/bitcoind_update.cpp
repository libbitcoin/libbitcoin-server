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
#include <bitcoin/server/utilities/bitcoind_update.hpp>

#include <algorithm>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace system::wallet;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The first expansion matching the entry's script updates the entry.
static const descriptor::signing* find_signing(
    const descriptor::signing::list& signings,
    const chain::script& script) NOEXCEPT
{
    const auto it = std::find_if(signings.begin(), signings.end(),
        [&script](const auto& item) NOEXCEPT
        {
            return item.script == script;
        });

    return (it == signings.end()) ? nullptr : &(*it);
}

// Attachments are consistent by construction: the embedded scripts and
// derived keys come from the same expansion as the matched script.
template <typename Entry>
static void update_entry(Entry& entry,
    const descriptor::signing& found) NOEXCEPT
{
    if (found.embedded)
        entry.embedded_script = found.embedded;

    if (found.witness)
        entry.witness_script = found.witness;

    for (const auto& derived: found.derivations)
        if (std::find(entry.derivations.begin(), entry.derivations.end(),
            derived) == entry.derivations.end())
            entry.derivations.push_back(derived);
}

void update_psbt(psbt::transaction& doc, const node::query& query,
    const descriptor::signing::list& signings) NOEXCEPT
{
    using transaction = psbt::transaction;
    const auto version0 = (doc.version() == transaction::version_0);

    for (size_t index{}; index < doc.inputs().size(); ++index)
    {
        auto& in = doc.inputs().at(index);
        auto utxo = doc.prevout(index);
        if (!utxo)
        {
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

            utxo = out;
        }

        const auto found = find_signing(signings, utxo->script());
        if (!is_null(found))
            update_entry(in, *found);
    }

    for (size_t index{}; index < doc.outputs().size(); ++index)
    {
        auto& out = doc.outputs().at(index);
        const auto script = !version0 ? out.script :
            doc.unsigned_tx().outputs_ptr()->at(index)->script_ptr();

        if (!script)
            continue;

        const auto found = find_signing(signings, *script);
        if (!is_null(found))
            update_entry(out, *found);
    }
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
