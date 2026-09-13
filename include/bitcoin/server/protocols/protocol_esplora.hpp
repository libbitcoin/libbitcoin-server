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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ESPLORA_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ESPLORA_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_esplora
  : public server::protocol_http,
    protected network::tracker<protocol_esplora>
{
public:
    typedef std::shared_ptr<protocol_esplora> ptr;
    using interface = server::interface::esplora;
    using dispatcher = network::rpc::dispatcher<interface>;
    using options_t = server::settings::esplora_server;
    using channel_t = channel_html;

    inline protocol_esplora(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_http(session, channel, options),
        network::tracker<protocol_esplora>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    /// Message handlers by http method.
    void handle_receive_get(const code& ec,
        const network::http::method::get::cptr& get) NOEXCEPT override;

    /// Dispatch.
    virtual bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Senders.
    virtual void send_json(boost::json::value&& model, size_t size_hint,
        const network::http::request& request={}) NOEXCEPT;

    /// Interface handlers.
    /// -----------------------------------------------------------------------

    bool handle_get_mempool(const code& ec, interface::mempool,
        uint8_t media) NOEXCEPT;
    bool handle_get_mempool_txids(const code& ec, interface::mempool_txids,
        uint8_t media) NOEXCEPT;
    bool handle_get_mempool_recent(const code& ec, interface::mempool_recent,
        uint8_t media) NOEXCEPT;
    bool handle_get_fee_estimates(const code& ec, interface::fee_estimates,
        uint8_t media) NOEXCEPT;

private:
    using media_type = network::http::media_type;
    static constexpr uint8_t json = to_value(media_type::application_json);

    static bool is_implemented(const std::string& method) NOEXCEPT;

    // Completion handlers (for asynchronous query).
    // ------------------------------------------------------------------------

    void next_estimate(size_t index) NOEXCEPT;
    void handle_estimate(const code& ec, uint64_t fee, size_t index) NOEXCEPT;
    void complete_estimate(const code& ec, uint64_t fee,
        size_t index) NOEXCEPT;
    // These are protected by strand.
    boost::json::object estimates_{};
    dispatcher dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
