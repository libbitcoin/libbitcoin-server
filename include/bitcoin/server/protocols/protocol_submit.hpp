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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SUBMIT_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SUBMIT_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

/// Outcome of a submitted block.
enum class submission
{
    /// Confirmable.
    accepted,

    /// Archived before, and valid.
    duplicate,

    /// Archived before, and not yet validated.
    duplicate_inconclusive,

    /// Archived before (or its header), and not confirmable.
    duplicate_invalid,

    /// Not of the candidate chain.
    inconclusive,

    /// Parent is not confirmable.
    bad_parent,

    /// Not valid, by the code.
    rejected,

    /// Not determined, by fault.
    failed
};

/// Block submission, common shape of protocols that submit blocks. The block
/// is organized by its header, checked and archived, and its outcome reported
/// once it is confirmable or not.
template <typename Base>
class protocol_submit
  : public Base
{
public:
    /// Subscribe to chase events (submitted block completion).
    void start() NOEXCEPT override;

    void stopping(const code& ec) NOEXCEPT override;

protected:
    using base = Base;
    using base::log;
    using gate_t = network::channel::gate_t;
    using header_cptr = system::chain::header::cptr;
    using block_cptr = system::chain::view::block::cptr;

    inline protocol_submit(const auto& session,
        const network::channel::ptr& channel,
        const typename base::options_t& options) NOEXCEPT
      : base(session, channel, options)
    {
    }

    /// Submit the block, its gate held to the outcome (requires strand).
    void submit_block(const header_cptr& header, const block_cptr& block,
        const gate_t::ptr& gate) NOEXCEPT;

    /// The outcome of the submitted block, with its code if rejected.
    virtual void handle_submitted(const system::hash_digest& hash,
        submission outcome, const code& ec) NOEXCEPT = 0;

    /// Chase events (submitted block completion).
    virtual bool handle_chase(const code& ec,
        node::event_value value) NOEXCEPT;

private:
    struct pending
    {
        system::hash_digest hash;
        gate_t::ptr gate;
    };

    // Submit block completions (bounced to the channel strand).
    void handle_organize_block(const code& ec, size_t height,
        const block_cptr& block, const gate_t::ptr& gate) NOEXCEPT;
    void do_organize_block(const code& ec, const block_cptr& block,
        const gate_t::ptr& gate) NOEXCEPT;
    void accept_block(const block_cptr& block,
        const database::header_link& link, const gate_t::ptr& gate) NOEXCEPT;
    void handle_archive_block(const code& ec, const system::hash_digest& hash,
        size_t height, const database::header_link& link) NOEXCEPT;
    void do_archive_block(const code& ec,
        const database::header_link& link) NOEXCEPT;
    void do_submitted(const code& ec, node::event_value value) NOEXCEPT;
    void complete(const database::header_link& link, submission outcome,
        const code& ec) NOEXCEPT;

    // This is protected by strand.
    std::unordered_map<database::header_link::integer, pending> submitted_{};
};

} // namespace server
} // namespace libbitcoin

#include <bitcoin/server/impl/protocols/protocol_submit.ipp>

#endif
