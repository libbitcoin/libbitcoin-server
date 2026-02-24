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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ELECTRUM_VERSION_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ELECTRUM_VERSION_HPP

#include <memory>
#include <unordered_map>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_electrum_version
  : public protocol_rpc<channel_electrum>,
    protected network::tracker<protocol_electrum_version>
{
public:
    typedef std::shared_ptr<protocol_electrum_version> ptr;
    using rpc_interface = interface::electrum;

    inline protocol_electrum_version(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_rpc<channel_electrum>(session, channel, options),
        channel_(std::dynamic_pointer_cast<channel_t>(channel)),
        network::tracker<protocol_electrum_version>(session->log)
    {
    }

    virtual void shake(network::result_handler&& handler) NOEXCEPT;
    virtual void finished(const code& ec, const code& shake) NOEXCEPT;

protected:
    static constexpr electrum_version minimum = electrum_version::v1_4;
    static constexpr electrum_version maximum = electrum_version::v1_4_2;
    static constexpr size_t max_client_name_length = 1024;

    void handle_server_version(const code& ec,
        rpc_interface::server_version, const std::string& client_name,
        const interface::value_t& protocol_version) NOEXCEPT;

    electrum_version version() const NOEXCEPT;
    std::string_view negotiated_version() const NOEXCEPT;
    bool set_version(const interface::value_t& version) NOEXCEPT;
    bool get_versions(electrum_version& min, electrum_version& max,
        const interface::value_t& version) NOEXCEPT;

    std::string_view server_name() const NOEXCEPT;
    std::string_view client_name() const NOEXCEPT;
    std::string escape_client(const std::string& in) NOEXCEPT;
    bool set_client(const std::string& name) NOEXCEPT;

private:
    static std::string_view version_to_string(
        electrum_version version) NOEXCEPT;
    static electrum_version version_from_string(
        const std::string_view& version) NOEXCEPT;

    // This is mostly thread safe, and used in a thread safe manner.
    const channel_t::ptr channel_;

    // This is protected by strand.
    std::shared_ptr<network::result_handler> handler_{};
};

} // namespace server
} // namespace libbitcoin

#endif
