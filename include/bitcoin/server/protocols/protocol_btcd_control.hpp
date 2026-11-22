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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_CONTROL_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_CONTROL_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_control.hpp>
#include <bitcoin/server/protocols/protocol_btcd_subgroup.hpp>

namespace libbitcoin {
namespace server {

/// bitcoind control subgroup on btcd endpoint, answering help as btcd does.
class BCS_API protocol_btcd_control
  : public protocol_btcd_subgroup<protocol_bitcoind_control>,
    protected network::tracker<protocol_btcd_control>
{
public:
    typedef std::shared_ptr<protocol_btcd_control> ptr;
    using base = protocol_btcd_subgroup<protocol_bitcoind_control>;

    inline protocol_btcd_control(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : base(session, channel, options),
        network::tracker<protocol_btcd_control>(session->log)
    {
    }

protected:
    std::string usage(const std::string& command) const NOEXCEPT override;
    void send_help_list() NOEXCEPT override;
    void send_help_unknown(const std::string& command) NOEXCEPT override;
};

} // namespace server
} // namespace libbitcoin

#endif
