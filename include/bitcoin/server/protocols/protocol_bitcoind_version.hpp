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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_VERSION_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_VERSION_HPP

#include <optional>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_handshake>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_handshake>;

/// The bitcoind handshake, attached first. A negotiateversion opener sets the
/// interface version of the channel, any other opener passes at the
/// configured version and is dispatched again once the service is attached.
class BCS_API protocol_bitcoind_version
  : public protocol_bitcoind_dispatch<interface::bitcoind_handshake>,
    protected network::tracker<protocol_bitcoind_version>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_version> ptr;
    using rpc_interface = interface::bitcoind_handshake;

    inline protocol_bitcoind_version(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_version>(session->log),
        channel_(std::dynamic_pointer_cast<channel_t>(channel))
    {
    }

    virtual void shake(network::result_handler&& handler) NOEXCEPT;
    virtual void finished(const code& ec, const code& shake) NOEXCEPT;

protected:
    /// Overridden to publish no method names.
    void register_names() NOEXCEPT override;

    /// Overridden to claim a negotiateversion opener, or pass any other.
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Overridden to pass any opener other than a json-rpc request.
    void handle_receive_get(const code& ec,
        const network::http::method::get::cptr& get) NOEXCEPT override;
    void handle_receive_head(const code& ec,
        const network::http::method::head::cptr& head) NOEXCEPT override;
    void handle_receive_put(const code& ec,
        const network::http::method::put::cptr& put) NOEXCEPT override;
    void handle_receive_delete(const code& ec,
        const network::http::method::delete_::cptr& delete_) NOEXCEPT override;
    void handle_receive_trace(const code& ec,
        const network::http::method::trace::cptr& trace) NOEXCEPT override;
    void handle_receive_options(const code& ec,
        const options_verb::cptr& options) NOEXCEPT override;
    void handle_receive_connect(const code& ec,
        const network::http::method::connect::cptr& connect) NOEXCEPT override;
    void handle_receive_unknown(const code& ec,
        const network::http::method::unknown::cptr& unknown) NOEXCEPT override;

    /// Overridden to complete the handshake upon its response.
    void handle_complete(const code& ec, const code& reason) NOEXCEPT override;

    /// Handlers.
    bool handle_negotiate_version(const code& ec,
        rpc_interface::negotiate_version,
        const std::optional<double>& minimum,
        const std::optional<double>& maximum) NOEXCEPT;

private:
    static bool is_negotiation(
        const network::rpc::request_t& message) NOEXCEPT;
    void negotiate(const network::rpc::request_t& message) NOEXCEPT;
    void pass_opener() NOEXCEPT;

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;

    // These are protected by strand.
    std::shared_ptr<network::result_handler> handler_{};
    bool negotiated_{};
};

} // namespace server
} // namespace libbitcoin

#endif
