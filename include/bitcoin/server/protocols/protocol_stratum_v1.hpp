/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP

#include <memory>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_stratum_v1
  : public protocol_rpc<channel_stratum_v1>,
    protected network::tracker<protocol_stratum_v1>
{
public:
    typedef std::shared_ptr<protocol_stratum_v1> ptr;
    using rpc_interface = interface::stratum_v1;

    inline protocol_stratum_v1(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<channel_stratum_v1>(session, channel, options),
        network::tracker<protocol_stratum_v1>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// Handlers (client requests).
    bool handle_mining_subscribe(const code& ec,
        rpc_interface::mining_subscribe, const std::string& user_agent,
        double extranonce1_size) NOEXCEPT;
    bool handle_mining_authorize(const code& ec,
        rpc_interface::mining_authorize, const std::string& username,
        const std::string& password) NOEXCEPT;
    bool handle_mining_submit(const code& ec,
        rpc_interface::mining_submit, const std::string& worker_name,
        const std::string& job_id, const std::string& extranonce2,
        double ntime, const std::string& nonce) NOEXCEPT;
    bool handle_mining_extranonce_subscribe(const code& ec,
        rpc_interface::mining_extranonce_subscribe) NOEXCEPT;
    bool handle_mining_extranonce_unsubscribe(const code& ec,
        rpc_interface::mining_extranonce_unsubscribe, double id) NOEXCEPT;

    /// Handlers (server notifications).
    bool handle_mining_configure(const code& ec,
        rpc_interface::mining_configure,
        const interface::object_t& extensions) NOEXCEPT;
    bool handle_mining_set_difficulty(const code& ec,
        rpc_interface::mining_set_difficulty, double difficulty) NOEXCEPT;
    bool handle_mining_notify(const code& ec,
        rpc_interface::mining_notify, const std::string& job_id,
        const std::string& prevhash, const std::string& coinb1,
        const std::string& coinb2, const interface::array_t& merkle_branch,
        double version, double nbits, double ntime, bool clean_jobs,
        bool hash1, bool hash2) NOEXCEPT;
    bool handle_client_reconnect(const code& ec,
        rpc_interface::client_reconnect, const std::string& url, double port,
        double id) NOEXCEPT;
    bool handle_client_hello(const code& ec,
        rpc_interface::client_hello,
        const interface::object_t& protocol) NOEXCEPT;
    bool handle_client_rejected(const code& ec,
        rpc_interface::client_rejected, const std::string& job_id,
        const std::string& reject_reason) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
