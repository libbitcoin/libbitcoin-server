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
#include <bitcoin/server/protocols/protocol_btcd_transaction.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// btcd creates version one transactions.
double protocol_btcd_transaction::created_version(double) const NOEXCEPT
{
    return 1.0;
}

// btcd decodes without hash, sizes or hex.
void protocol_btcd_transaction::inject_decoded(json::object& out,
    const chain::transaction& tx) const NOEXCEPT
{
    inject_scripts(out, tx);
    out.stable_erase("hash");
    out.stable_erase("size");
    out.stable_erase("vsize");
    out.stable_erase("weight");
    out.stable_erase("hex");
}

void protocol_btcd_transaction::send_rejected(const code& ec,
    const chain::transaction& tx) NOEXCEPT
{
    using namespace error::btcd;
    if (translate(ec, verify_rejected) == internal_error)
    {
        send_error(internal_error);
        return;
    }

    std::string text{};
    const auto coded = rejection(text, ec, tx);
    send_error_message(coded, "TX rejected: " + text);
}

object_t protocol_btcd_transaction::test_result(const code& ec,
    const chain::transaction& tx, double maxfeerate) NOEXCEPT
{
    object_t result
    {
        { "txid", encode_hash(tx.hash(false)) },
        { "wtxid", encode_hash(tx.hash(true)) },
        { "package-error", std::string{} }
    };

    // An accepted tx is populated, so its fee is known. btcd defaults a zero
    // maximum rate.
    constexpr auto default_rate = 0.1;
    const auto maximum = is_zero(maxfeerate) ? default_rate : maxfeerate;
    uint64_t fee{};
    if (!ec)
        fee = tx.fee();

    const auto vsize = tx.virtual_size();
    if (!ec && is_excessive(fee, vsize, maximum))
    {
        result.emplace("reject-reason", std::string{ "max-fee-exceeded" });
        return result;
    }

    if (!ec)
    {
        constexpr auto coin = to_floating(chain::satoshi_per_bitcoin);
        const auto rate = fee * 1'000u / possible_wide_cast<uint64_t>(vsize);
        result.emplace("allowed", true);
        result.emplace("vsize", to64(vsize));
        result.emplace("fees", object_t
        {
            { "base", fee / coin },
            { "effective-feerate", rate / coin },
            { "effective-includes", null_t{} }
        });

        return result;
    }

    std::string reason{ "missing-inputs" };
    if (!is_orphan(ec, tx))
        rejection(reason, ec, tx);

    result.emplace("reject-reason", std::move(reason));
    return result;
}

// A missing coin, or a confirmed duplicate with all of its outputs spent.
bool protocol_btcd_transaction::is_orphan(const code& ec,
    const chain::transaction& tx) const NOEXCEPT
{
    if (is_missing(ec))
        return true;

    if (ec != node::error::duplicate_transaction)
        return false;

    const auto& query = archive();
    const auto link = query.to_tx(tx.hash(false));
    return query.is_confirmed_tx(link) && !has_unspent_output(link);
}

// The previous tx of the first input without an unspent previous output.
hash_digest protocol_btcd_transaction::missing_parent(
    const chain::transaction& tx) const NOEXCEPT
{
    const auto& query = archive();
    const auto& ins = *tx.inputs_ptr();
    for (const auto& in: ins)
    {
        const auto out = query.to_output(in->point());
        if (out.is_terminal() || query.is_confirmed_spent(out))
            return in->point().hash();
    }

    return ins.front()->point().hash();
}

error::btcd::error_t protocol_btcd_transaction::rejection(std::string& out,
    const code& ec, const chain::transaction& tx) const NOEXCEPT
{
    using namespace error::btcd;
    const auto txid = encode_hash(tx.hash(false));
    if (is_orphan(ec, tx))
    {
        const auto parent = encode_hash(missing_parent(tx));
        out = "orphan transaction " + txid + " references outputs of "
            "unknown or fully-spent transaction " + parent;
        return verify_error;
    }

    if (ec != node::error::duplicate_transaction)
        return reject(out, ec);

    const auto& query = archive();
    if (query.is_confirmed_tx(query.to_tx(tx.hash(false))))
    {
        out = "transaction already exists in blockchain";
        return verify_already_in_chain;
    }

    out = "already have transaction in mempool " + txid;
    return verify_rejected;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
