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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_MINING_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_MINING_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>
#include <bitcoin/server/protocols/protocol_submit.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_mining>;

namespace server {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_mining>;

class BCS_API protocol_bitcoind_mining
  : public protocol_submit<protocol_bitcoind_dispatch<interface::bitcoind_mining>>,
    protected network::tracker<protocol_bitcoind_mining>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_mining> ptr;
    using rpc_interface = interface::bitcoind_mining;
    using submit_t = protocol_submit<protocol_bitcoind_dispatch<rpc_interface>>;

    inline protocol_bitcoind_mining(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : submit_t(session, channel, options),
        network::tracker<protocol_bitcoind_mining>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Answer the submitted block (bip22).
    void handle_submitted(const system::hash_digest& hash,
        submission outcome, const code& ec) NOEXCEPT override;

    /// Complete a getblocktemplate result.
    virtual void finish_template(network::rpc::object_t& out) const NOEXCEPT;

    /// Handlers.
    void handle_get_network_hash_ps(const code& ec,
        rpc_interface::get_network_hash_ps, double nblocks,
        double height) NOEXCEPT;
    void handle_get_mining_info(const code& ec,
        rpc_interface::get_mining_info) NOEXCEPT;
    void handle_submit_block(const code& ec,
        rpc_interface::submit_block, const std::string& hexdata,
        const std::string& dummy) NOEXCEPT;
    void handle_submit_header(const code& ec,
        rpc_interface::submit_header, const std::string& hexdata) NOEXCEPT;
    void handle_get_block_template(const code& ec,
        rpc_interface::get_block_template,
        const network::rpc::object_t& template_request) NOEXCEPT;
    void handle_get_prioritised_transactions(const code& ec,
        rpc_interface::get_prioritised_transactions) NOEXCEPT;
    void handle_prioritise_transaction(const code& ec,
        rpc_interface::prioritise_transaction) NOEXCEPT;

private:
    using longpoll_race = network::race_any<const code&>;

    struct longpoll
    {
        typedef std::shared_ptr<longpoll> ptr;

        uint64_t sequence;
        gate_t::ptr gate{};
        network::proxy::object_key stop{};
        node::object_key events{};
        bool done{};
    };

    // Pool completions (bounced to the channel strand).
    void handle_mining_info(const code& ec, const database::tx_links& links,
        const gate_t::ptr& gate) NOEXCEPT;
    virtual void do_mining_info(const code& ec,
        const database::tx_links& links,
        const gate_t::ptr& gate) NOEXCEPT;

    // Organize completions (bounced to the channel strand).
    void handle_organize_header(const code& ec, size_t height,
        const gate_t::ptr& gate) NOEXCEPT;
    void do_submit_header(const code& ec, const gate_t::ptr& gate) NOEXCEPT;

    // Template completions (bounced to the channel strand).
    void handle_template(const code& ec,
        const node::block_template::cptr& held,
        const gate_t::ptr& gate) NOEXCEPT;
    void do_template(const code& ec, const node::block_template::cptr& held,
        const gate_t::ptr& gate) NOEXCEPT;

    // Long polls.
    bool is_current(uint64_t& sequence,
        const network::rpc::value_t& longpollid) const NOEXCEPT;
    void arm_longpoll(uint64_t sequence) NOEXCEPT;
    bool handle_longpoll_event(const code& ec, node::event_value value,
        const longpoll::ptr& poll, const longpoll_race::ptr& race) NOEXCEPT;
    void handle_longpoll_subscribed(const code& ec, node::object_key key,
        const longpoll::ptr& poll, const longpoll_race::ptr& race) NOEXCEPT;
    void do_longpoll_subscribed(const code& ec, node::object_key key,
        const longpoll::ptr& poll, const longpoll_race::ptr& race) NOEXCEPT;
    void handle_longpoll_stop(const code& ec,
        const longpoll_race::ptr& race) NOEXCEPT;
    void handle_longpoll_complete(const code& ec,
        const longpoll::ptr& poll) NOEXCEPT;
    void do_longpoll_complete(const code& ec,
        const longpoll::ptr& poll) NOEXCEPT;
    bool longpoll_done(const longpoll& poll) const NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
