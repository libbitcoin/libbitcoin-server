/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_SESSIONS_SESSIONS_HPP
#define LIBBITCOIN_SERVER_SESSIONS_SESSIONS_HPP

#include <bitcoin/server/sessions/session.hpp>
#include <bitcoin/server/sessions/session_handshake.hpp>
#include <bitcoin/server/sessions/session_server.hpp>

#include <bitcoin/server/protocols/protocols.hpp>

namespace libbitcoin {
namespace server {

/// Alias server sessions, all derived from node::session.
using session_admin = session_server<protocol_admin>;
using session_native = session_server<protocol_native>;
using session_bitcoind = session_server<protocol_bitcoind_rest>;
using session_stratum_v1 = session_server<protocol_stratum_v1>;
using session_stratum_v2 = session_server<protocol_stratum_v2>;
using session_electrum = session_handshake<protocol_electrum_version,
    protocol_electrum>;

} // namespace server
} // namespace libbitcoin

#endif
