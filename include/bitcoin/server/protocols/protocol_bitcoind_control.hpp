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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_CONTROL_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BITCOIND_CONTROL_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

namespace libbitcoin {

// The dispatch metaprogramming is isolated to the subgroup translation unit.
extern template class network::rpc::dispatcher<
    server::interface::bitcoind_control>;

namespace server {

extern template class protocol_bitcoind_dispatch<
    interface::bitcoind_control>;

class BCS_API protocol_bitcoind_control
  : public protocol_bitcoind_dispatch<interface::bitcoind_control>,
    protected network::tracker<protocol_bitcoind_control>
{
public:
    typedef std::shared_ptr<protocol_bitcoind_control> ptr;
    using rpc_interface = interface::bitcoind_control;

    inline protocol_bitcoind_control(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_bitcoind_dispatch<rpc_interface>(session, channel, options),
        network::tracker<protocol_bitcoind_control>(session->log)
    {
    }

    void start() NOEXCEPT override;

protected:
    /// The usage line of a served command, empty if none.
    virtual std::string usage(const std::string& command) const NOEXCEPT;

    /// Answer help without a command.
    virtual void send_help_list() NOEXCEPT;

    /// Answer help for a command that is not served.
    virtual void send_help_unknown(const std::string& command) NOEXCEPT;

    /// Set usage line of command if defined by methods,
    /// with optional parameters parenthesized.
    template <typename Methods>
    static void find_usage(std::string& out, const std::string& command,
        server::bitcoind::version version) NOEXCEPT
    {
        std::apply([&](const auto&... entries) NOEXCEPT
        {
            (append_usage(out, entries, command, version), ...);
        }, Methods::methods);
    }

    /// Handlers.
    void handle_help(const code& ec, rpc_interface::help,
        const std::string& command) NOEXCEPT;
    void handle_stop(const code& ec,
        rpc_interface::stop) NOEXCEPT;
    void handle_get_memory_info(const code& ec,
        rpc_interface::get_memory_info, const std::string& mode) NOEXCEPT;
    void handle_get_openrpc_info(const code& ec,
        rpc_interface::get_openrpc_info, bool show_hidden) NOEXCEPT;
    void handle_get_rpc_info(const code& ec,
        rpc_interface::get_rpc_info) NOEXCEPT;
    void handle_logging(const code& ec, rpc_interface::logging,
        const network::rpc::array_t& include,
        const network::rpc::array_t& exclude) NOEXCEPT;
    void handle_uptime(const code& ec,
        rpc_interface::uptime) NOEXCEPT;
    void handle_rpc_discover(const code& ec,
        rpc_interface::rpc_discover, bool show_hidden) NOEXCEPT;

private:
    template <typename Method>
    static void append_usage(std::string& out, const Method& entry,
        const std::string& command,
        server::bitcoind::version version) NOEXCEPT
    {
        using namespace network::rpc;
        if (!entry.implemented() || entry.name != command ||
            version < server::bitcoind::introduced(entry.name))
            return;

        out = command;
        const auto& names = entry.parameter_names();
        const auto append = [&](const auto& name, bool optional) NOEXCEPT
        {
            const std::string text{ primary_name(name) };
            out += optional ? " ( " + text + " )" : " " + text;
        };

        [&]<size_t... Index>(std::index_sequence<Index...>) NOEXCEPT
        {
            (append(names.at(Index), is_defaulted<std::tuple_element_t<Index,
                typename Method::args_native>>), ...);
        }(std::make_index_sequence<Method::size>{});
    }

    void send_openrpc() NOEXCEPT;
};

} // namespace server
} // namespace libbitcoin

#endif
