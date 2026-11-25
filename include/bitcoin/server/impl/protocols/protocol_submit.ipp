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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SUBMIT_IPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SUBMIT_IPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define TEMPLATE template <typename Base>
#define CLASS protocol_submit<Base>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start.
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::start() NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace std::placeholders;

    if (this->started())
        return;

    this->subscribe_chase(
        this->template bind<CLASS>(&CLASS::handle_chase,
            _1, _2));

    base::start();
}

TEMPLATE
void CLASS::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    this->unsubscribe_chase();
    submitted_.clear();
    base::stopping(ec);
}

// Submit.
// ----------------------------------------------------------------------------

TEMPLATE
void CLASS::submit_block(const header_cptr& header, const block_cptr& block,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace std::placeholders;

    const auto link = this->archive().to_header(header->hash());
    if (!link.is_terminal())
    {
        accept_block(block, link, gate);
        return;
    }

    this->organize_submitted(header,
        this->template bind<CLASS>(&CLASS::handle_organize_block,
            _1, _2, block, gate));
}

TEMPLATE
void CLASS::handle_organize_block(const code& ec, size_t,
    const block_cptr& block, const gate_t::ptr& gate) NOEXCEPT
{
    if (this->stopped())
        return;

    POST(do_organize_block, ec, block, gate);
}

// The header of a weak branch is cached, not archived, so its block is not.
TEMPLATE
void CLASS::do_organize_block(const code& ec, const block_cptr& block,
    const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace system::error;

    if (this->stopped())
        return;

    // An unknown header is unconfirmable only by its parent.
    const auto hash = block->hash();
    if (ec == database::error::block_unconfirmable)
    {
        handle_submitted(hash, submission::bad_parent, ec);
        return;
    }

    if (ec && (ec != node::error::duplicate_header))
    {
        const auto invalid = block_error_category::contains(ec) ||
            (ec == node::error::orphan_header);

        const auto outcome = invalid ? submission::rejected :
            submission::failed;

        handle_submitted(hash, outcome, ec);
        return;
    }

    const auto link = this->archive().to_header(hash);
    if (link.is_terminal())
    {
        handle_submitted(hash, submission::inconclusive, {});
        return;
    }

    accept_block(block, link, gate);
}

// A body that does not match its header is rejected, and leaves no state.
TEMPLATE
void CLASS::accept_block(const block_cptr& block,
    const database::header_link& link, const gate_t::ptr& gate) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace database;
    using namespace std::placeholders;

    const auto& query = this->archive();
    const auto hash = block->hash();
    const auto state = query.get_block_state(link);
    if (is_associated(state))
    {
        const auto confirmed = query.is_confirmed_block(link);
        const auto valid = is_validated(state) || confirmed;
        const auto duplicate = valid ? submission::duplicate :
            submission::duplicate_inconclusive;

        const auto invalid = is_unconfirmable(state);
        const auto outcome = invalid ? submission::duplicate_invalid :
            duplicate;

        handle_submitted(hash, outcome, {});
        return;
    }

    if (is_unconfirmable(state))
    {
        handle_submitted(hash, submission::duplicate_invalid, {});
        return;
    }

    system::chain::context ctx{};
    if (!query.get_context(ctx, link))
    {
        handle_submitted(hash, submission::failed, database::error::integrity);
        return;
    }

    if (const auto failed = block->check_guard())
    {
        handle_submitted(hash, submission::rejected, failed);
        return;
    }

    if (const auto failed = block->check_guard(ctx))
    {
        handle_submitted(hash, submission::rejected, failed);
        return;
    }

    if (!query.is_candidate_header(link))
    {
        handle_submitted(hash, submission::inconclusive, {});
        return;
    }

    if (!submitted_.emplace(link.value, pending{ hash, gate }).second)
    {
        handle_submitted(hash, submission::duplicate_inconclusive, {});
        return;
    }

    // A bypassed block of a pruned node does not store its witness.
    code stored{};
    const auto height = ctx.height;
    const auto bypass = is_bypass(state);
    const auto prune = bypass && this->node_settings().limited_blocks;
    const auto strong = is_any(state, checkpointed);
    if (!this->archive_block
    (
        stored,
        block,
        link,
        strong,
        bypass,
        height,
        prune,
        this->template bind<CLASS>(&CLASS::handle_archive_block, _1, hash,
            height, link)
    )) return;

    handle_archive_block(stored, hash, height, link);
}

// Block archived meanwhile by another channel is awaited as submitted, and
// failure to store one is a fault, whether or not channel has stopped.
TEMPLATE
void CLASS::handle_archive_block(const code& ec,
    const system::hash_digest& hash, size_t height,
    const database::header_link& link) NOEXCEPT
{
    if (!ec)
    {
        this->notify(ec, node::chases::checked{ height });
        this->fire(node::events::block_archived, height);
    }
    else if ((ec != node::error::duplicate_block) &&
        (ec != network::error::service_stopped))
    {
        LOGF("Failure storing block [" << system::encode_hash(hash) << ":"
            << height << "] " << ec.message());
        this->fault(ec);
    }

    if (this->stopped())
        return;

    POST(do_archive_block, ec, link);
}

TEMPLATE
void CLASS::do_archive_block(const code& ec,
    const database::header_link& link) NOEXCEPT
{
    BC_ASSERT(this->stranded());

    if (this->stopped() || !ec || (ec == node::error::duplicate_block))
        return;

    complete(link, submission::failed, ec);
}

// Events.
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::handle_chase(const code& ec, node::event_value value) NOEXCEPT
{
    // Do not pass ec to stopped, it is not a call status.
    if (this->stopped())
        return false;

    switch (node::to_chase(value))
    {
        case node::chase::regressed:
        case node::chase::disorganized:
        case node::chase::unvalid:
        case node::chase::confirmable:
        case node::chase::unconfirmable:
        {
            POST(do_submitted, ec, value);
            break;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// A submitted block leaving the candidate chain unconfirmed is inconclusive.
TEMPLATE
void CLASS::do_submitted(const code& ec, node::event_value value) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    using namespace node;

    if (this->stopped() || submitted_.empty())
        return;

    switch (to_chase(value))
    {
        case chase::confirmable:
        {
            const auto link = to_payload<chase::confirmable>(value).link;
            complete(link, submission::accepted, {});
            break;
        }
        case chase::unvalid:
        {
            const auto link = to_payload<chase::unvalid>(value).link;
            complete(link, submission::rejected, ec);
            break;
        }
        case chase::unconfirmable:
        {
            const auto link = to_payload<chase::unconfirmable>(value).link;
            complete(link, submission::rejected, ec);
            break;
        }
        default:
        {
            std::vector<database::header_link> left{};
            for (const auto& entry: submitted_)
                if (!this->archive().is_candidate_header(entry.first))
                    left.emplace_back(entry.first);

            for (const auto& link: left)
                complete(link, submission::inconclusive, {});

            break;
        }
    }
}

TEMPLATE
void CLASS::complete(const database::header_link& link, submission outcome,
    const code& ec) NOEXCEPT
{
    BC_ASSERT(this->stranded());
    const auto it = submitted_.find(link.value);
    if (it == submitted_.end())
        return;

    const auto entry = it->second;
    submitted_.erase(it);
    handle_submitted(entry.hash, outcome, ec);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

#undef CLASS
#undef TEMPLATE

} // namespace server
} // namespace libbitcoin

#endif
