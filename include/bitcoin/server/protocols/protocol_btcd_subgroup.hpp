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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_SUBGROUP_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_BTCD_SUBGROUP_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/serializers.hpp>

namespace libbitcoin {
namespace server {

/// bitcoind interface subgroup on btcd endpoint, answering parameter faults,
/// output scripts and transaction context as btcd does.
template <typename Subgroup>
class protocol_btcd_subgroup
  : public Subgroup
{
public:
    using Subgroup::Subgroup;

protected:
    inline code to_parameter_error(const code& ec) const NOEXCEPT override
    {
        return error::btcd::parameter_error(ec);
    }

    inline code to_hex_error(const code&) const NOEXCEPT override
    {
        return error::btcd::deserialization_error;
    }

    inline system::json::value script_public_key(
        const system::chain::script& script) const NOEXCEPT override
    {
        return btcd::script_public_key(script, this->p2kh_, this->p2sh_,
            this->witness_, this->flags_);
    }

    inline void inject_scripts(system::json::object& out,
        const system::chain::transaction& tx) const NOEXCEPT override
    {
        btcd::inject_scripts(out, tx, this->p2kh_, this->p2sh_,
            this->witness_, this->flags_);
    }

    inline void inject_context(system::json::object& out,
        const database::tx_link& link) const NOEXCEPT override
    {
        btcd::inject_tx_context(out, this->archive(), link);
    }
};

} // namespace server
} // namespace libbitcoin

#endif
