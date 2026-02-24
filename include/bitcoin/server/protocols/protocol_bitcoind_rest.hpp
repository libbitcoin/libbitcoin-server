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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_REST_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_REST_HPP

#include <memory>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_bitcoind_rest
  : public protocol_bitcoind_rpc,
    protected network::tracker<protocol_bitcoind_rest>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_rest> ptr;
    using rest_interface = interface::bitcoind_rest;
    using rest_dispatcher = network::rpc::dispatcher<rest_interface>;

    inline protocol_bitcoind_rest(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_rpc(session, channel, options),
        network::tracker<protocol_bitcoind_rest>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    using get = network::http::method::get;

    /// Dispatch.
    void handle_receive_get(const code& ec,
        const get::cptr& get) NOEXCEPT override;

    /// REST interface handlers.
    bool handle_get_block(const code& ec, rest_interface::block,
        uint8_t media, system::hash_cptr hash) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        rest_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    // This is protected by strand.
    rest_dispatcher rest_dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
