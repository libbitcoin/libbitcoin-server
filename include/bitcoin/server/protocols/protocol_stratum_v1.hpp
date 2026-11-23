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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_STRATUM_V1_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_stratum_v1
  : public server::protocol,
    public network::protocol_rpc<channel_stratum_v1>,
    protected network::tracker<protocol_stratum_v1>
{
public:
    typedef std::shared_ptr<protocol_stratum_v1> ptr;
    using channel_t = channel_stratum_v1;
    using options_t = channel_t::options_t;
    using rpc_interface = interface::stratum_v1;

    LCOV_EXCL_START("Stratum is not implemented.")
    inline protocol_stratum_v1(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol(session, channel),
        network::protocol_rpc<channel_stratum_v1>(session, channel, options),
        network::tracker<protocol_stratum_v1>(session->log)
    {
    }
    LCOV_EXCL_STOP()

    void start() NOEXCEPT override;

protected:
    /// Handlers.
    bool handle_mining_subscribe(const code& ec,
        rpc_interface::mining_subscribe, const std::string& user_agent,
        const std::string& session_id) NOEXCEPT;
    bool handle_mining_authorize(const code& ec,
        rpc_interface::mining_authorize, const std::string& username,
        const std::string& password) NOEXCEPT;
    bool handle_mining_submit(const code& ec,
        rpc_interface::mining_submit, const std::string& worker_name,
        const std::string& job_id, const std::string& extranonce2,
        const std::string& ntime, const std::string& nonce,
        const std::string& version_bits) NOEXCEPT;
    bool handle_mining_configure(const code& ec,
        rpc_interface::mining_configure,
        const interface::array_t& extensions,
        const interface::object_t& parameters) NOEXCEPT;
    bool handle_mining_suggest_difficulty(const code& ec,
        rpc_interface::mining_suggest_difficulty,
        double difficulty) NOEXCEPT;
    bool handle_mining_suggest_target(const code& ec,
        rpc_interface::mining_suggest_target,
        const std::string& target) NOEXCEPT;
    bool handle_mining_get_transactions(const code& ec,
        rpc_interface::mining_get_transactions,
        const std::string& job_id) NOEXCEPT;
    bool handle_mining_multi_version(const code& ec,
        rpc_interface::mining_multi_version, double version) NOEXCEPT;
    bool handle_mining_extranonce_subscribe(const code& ec,
        rpc_interface::mining_extranonce_subscribe) NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
