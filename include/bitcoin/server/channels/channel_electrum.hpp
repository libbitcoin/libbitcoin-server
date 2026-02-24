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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ELECTRUM_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_ELECTRUM_HPP

#include <memory>
#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>

namespace libbitcoin {
namespace server {

// TODO: strip extraneous args before electrum version dispatch.
/// Channel for electrum channels (non-http json-rpc).
class BCS_API channel_electrum
  : public server::channel,
    public network::channel_rpc<interface::electrum>,
    protected network::tracker<channel_electrum>
{
public:
    typedef std::shared_ptr<channel_electrum> ptr;
    using interface_t = interface::electrum;
    using options_t = typename network::channel_rpc<interface_t>::options_t;

    inline channel_electrum(const network::logger& log,
        const network::socket::ptr& socket, uint64_t identifier,
        const node::configuration& config, const options_t& options) NOEXCEPT
      : server::channel(log, socket, identifier, config),
        network::channel_rpc<interface::electrum>(log, socket, identifier,
            config.network, options),
        network::tracker<channel_electrum>(log)
    {
    }

    /// Properties.
    /// -----------------------------------------------------------------------

    inline void set_client(const std::string& name) NOEXCEPT
    {
        name_ = name;
    }

    inline const std::string& client() const NOEXCEPT
    {
        return name_;
    }

    inline void set_version(electrum_version version) NOEXCEPT
    {
        version_ = version;
    }

    inline electrum_version version() const NOEXCEPT
    {
        return version_;
    }

private:
    // These are protected by strand.
    electrum_version version_{ electrum_version::v0_0 };
    std::string name_{};
};

} // namespace server
} // namespace libbitcoin

#endif
