/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_FULL_NODE_HPP
#define LIBBITCOIN_SERVER_FULL_NODE_HPP

#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/sessions/sessions.hpp>

namespace libbitcoin {
namespace server {

/// Thread safe.
class BCS_API server_node
  : public node::full_node
{
public:
    using store = node::store;
    using query = node::query;
    using result_handler = network::result_handler;
    typedef std::shared_ptr<server_node> ptr;

    /// Constructors.
    /// -----------------------------------------------------------------------

    server_node(query& query, const configuration& configuration,
        const network::logger& log) NOEXCEPT;

    ~server_node() NOEXCEPT;

    /// Sequences.
    /// -----------------------------------------------------------------------

    /// Run the node (inbound/outbound services).
    void run(result_handler&& handler) NOEXCEPT override;

    /// Properties.
    /// -----------------------------------------------------------------------

    /// Configuration for all libraries.
    virtual const server::configuration& server_config() const NOEXCEPT;
    ////const node::configuration& node_config() const NOEXCEPT override;

    /// Configuration settings for all libraries.
    ////const system::settings& system_settings() const NOEXCEPT override;
    ////const database::settings& database_settings() const NOEXCEPT override;
    ////const network::settings& network_settings() const NOEXCEPT override;
    ////const node::settings& node_settings() const NOEXCEPT override;
    virtual const server::settings& server_settings() const NOEXCEPT;

protected:
    /// Session attachments.
    /// -----------------------------------------------------------------------

    /// Attach server sessions (base net doesn't specialize or start these).
    virtual session_admin::ptr attach_admin_session() NOEXCEPT;
    virtual session_native::ptr attach_native_session() NOEXCEPT;
    virtual session_bitcoind::ptr attach_bitcoind_session() NOEXCEPT;
    virtual session_electrum::ptr attach_electrum_session() NOEXCEPT;
    virtual session_stratum_v1::ptr attach_stratum_v1_session() NOEXCEPT;
    virtual session_stratum_v2::ptr attach_stratum_v2_session() NOEXCEPT;

    /// Virtual handlers.
    /// -----------------------------------------------------------------------
    void do_run(const result_handler& handler) NOEXCEPT override;

private:
    void start_admin(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_native(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_bitcoind(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_electrum(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_stratum_v1(const code& ec, const result_handler& handler) NOEXCEPT;
    void start_stratum_v2(const code& ec, const result_handler& handler) NOEXCEPT;

    // This is thread safe.
    const configuration& config_;
};

} // namespace server
} // namespace libbitcoin

#endif
