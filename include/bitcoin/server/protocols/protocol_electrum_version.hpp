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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ELECTRUM_VERSION_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ELECTRUM_VERSION_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/serializers/serializers.hpp>
#include <bitcoin/server/utilities/utilities.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_electrum_version
  : public protocol_rpc<interface::electrum_handshake>,
    protected network::tracker<protocol_electrum_version>
{
public:
    typedef std::shared_ptr<protocol_electrum_version> ptr;
    using rpc_interface = interface::electrum_handshake;
    using channel_t = channel_electrum;
    using options_t = channel_t::options_t;

    inline protocol_electrum_version(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<interface::electrum_handshake>(session, channel, options),
        options_(options),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        network::tracker<protocol_electrum_version>(session->log)
    {
    }

    virtual void shake(network::result_handler&& handler) NOEXCEPT;
    virtual void finished(const code& ec, const code& shake) NOEXCEPT;

protected:
    void handle_unclaimed(
        const network::rpc::request_t& message) NOEXCEPT override;

    void handle_server_version(const code& ec,
        rpc_interface::server_version, const std::string& client_name,
        const interface::value_t& protocol_version) NOEXCEPT;

    bool set_version(const interface::value_t& version) NOEXCEPT;
    bool set_client(const std::string& name) NOEXCEPT;
    std::string_view client_name() const NOEXCEPT;

    inline const options_t& options() const NOEXCEPT
    {
        return options_;
    }

private:
    // This is thread safe.
    const options_t& options_;

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;

    // This is protected by strand.
    std::shared_ptr<network::result_handler> handler_{};
};

} // namespace server
} // namespace libbitcoin

#endif
