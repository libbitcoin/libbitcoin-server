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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ADMIN_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ADMIN_HPP

#include <atomic>
#include <memory>
#include <string>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_html.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_admin
  : public protocol_html,
    protected network::tracker<protocol_admin>
{
public:
    typedef std::shared_ptr<protocol_admin> ptr;
    using interface = server::interface::admin;
    using dispatcher = network::rpc::dispatcher<interface>;

    inline protocol_admin(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_html(session, channel, options),
        network::tracker<protocol_admin>(session->log)
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

    /// Dispatch.
    bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT override;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Event handlers.
    /// -----------------------------------------------------------------------

    bool handle_log(const code& ec, uint8_t level, time_t zulu,
        const std::string& message) NOEXCEPT;
    bool handle_event(const code& ec, uint8_t event_, uint64_t value,
        const network::logger::time& point) NOEXCEPT;

    /// Interface handlers.
    /// -----------------------------------------------------------------------

    bool handle_get_log_subscribe(const code& ec, interface::log_subscribe,
        uint8_t version, uint64_t filter) NOEXCEPT;
    bool handle_get_event_subscribe(const code& ec, interface::event_subscribe,
        uint8_t version, uint64_t filter) NOEXCEPT;

protected:
    /// Notification event handlers (protocol strand).
    /// -----------------------------------------------------------------------
    void do_log(uint8_t level, time_t zulu, const std::string& message) NOEXCEPT;
    void do_event(uint8_t event_, time_t zulu, uint64_t value) NOEXCEPT;

private:
    using filter_t = std::atomic<uint64_t>;

    // Utils.
    static time_t to_zulu(const network::logger::time& point) NOEXCEPT;
    static bool get_filter(bool& filtered, size_t bit,
        filter_t& filter) NOEXCEPT;
    static bool update_filter(uint64_t& prior, uint64_t value,
        filter_t& filter) NOEXCEPT;

    // These are thread safe.
    filter_t log_state_{};
    filter_t event_state_{};

    // This is protected by strand.
    dispatcher dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
