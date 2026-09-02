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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_HPP

#include <memory>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>

namespace libbitcoin {
namespace server {

/// Common base for the bitcoind interface subgroup protocols, and the
/// terminal default responder. Subgroup protocols carry their own interface
/// dispatchers and are attached to the channel before this class, which is
/// attached last. A subgroup claims each request it dispatches (via the
/// channel latch), so default responses are sent here exactly once and only
/// when no subgroup has claimed the request.
class BCS_API protocol_bitcoind
  : public server::protocol_http,
    protected network::tracker<protocol_bitcoind>
{
public:
    // Replace base class channel_t (json-rpc websocket reader body).
    using channel_t = channel_http<network::rpc::request>;

    typedef std::shared_ptr<protocol_bitcoind> ptr;

    inline protocol_bitcoind(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_http(session, channel, options),
        network::tracker<protocol_bitcoind>(session->log),
        p2kh_(session->server_settings().wallet.p2kh_prefix),
        p2sh_(session->server_settings().wallet.p2sh_prefix),
        witness_(session->server_settings().wallet.witness_prefix)
    {
    }

protected:
    using post = network::http::method::post;
    using options = network::http::method::options;

    /// Terminal dispatch (unclaimed requests only).
    void handle_receive_get(const code& ec,
        const network::http::method::get::cptr& get) NOEXCEPT override;
    void handle_receive_options(const code& ec,
        const network::http::method::options::cptr& options) NOEXCEPT override;
    void handle_receive_post(const code& ec,
        const post::cptr& post) NOEXCEPT override;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// The method names reported by help (channel-registered on start).
    std::string help_names() const NOEXCEPT;

    /// Senders. close_reason (if truthy) stops the channel only once the
    /// write has completed, so the error reaches the client first.
    void send_error(const code& ec) NOEXCEPT;
    void send_error(const code& ec, size_t size_hint) NOEXCEPT;
    void send_error(const code& ec, size_t size_hint,
        const code& close_reason) NOEXCEPT;
    void send_error(const code& ec, network::rpc::value_option&& error,
        size_t size_hint) NOEXCEPT;
    void send_text(std::string&& hexidecimal) NOEXCEPT;
    void send_result(network::rpc::value_option&& result,
        size_t size_hint) NOEXCEPT;

    /// Cache rpc response context for serialization (requires strand). The
    /// websocket overload has no http request to echo headers from.
    void set_rpc_request(network::rpc::version version,
        const network::rpc::id_option& id,
        const network::http::request_cptr& request) NOEXCEPT;
    void set_rpc_request(const network::rpc::request_t& message) NOEXCEPT;

    /// Validate a transaction given next block context (node utility).
    code validate_tx(const system::chain::transaction& tx) const NOEXCEPT;
    code broadcast_tx(const system::chain::transaction::cptr& tx) NOEXCEPT;

private:
    // Senders.
    void send_rpc(network::rpc::response_t&& model,
        size_t size_hint) NOEXCEPT;
    void send_rpc(network::rpc::response_t&& model, size_t size_hint,
        const code& close_reason) NOEXCEPT;

    // Obtain cached request and clear cache (requires strand).
    network::http::request_cptr reset_rpc_request() NOEXCEPT;

    // These are protected by strand.
    network::rpc::version version_{};
    network::rpc::id_option id_{};

protected:
    // These are thread safe.
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    const std::string witness_;
};

} // namespace server
} // namespace libbitcoin

#endif
