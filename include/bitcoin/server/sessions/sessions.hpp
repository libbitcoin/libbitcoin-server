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
#ifndef LIBBITCOIN_SERVER_SESSIONS_SESSIONS_HPP
#define LIBBITCOIN_SERVER_SESSIONS_SESSIONS_HPP

#include <bitcoin/server/sessions/session.hpp>
#include <bitcoin/server/sessions/session_handshake.hpp>
#include <bitcoin/server/sessions/session_server.hpp>

#include <bitcoin/server/protocols/protocols.hpp>

namespace libbitcoin {
namespace server {

/// Alias server sessions, all derived from node::session.
/// The first protocol supplies channel_t/options_t, protocol_bitcoind is the
/// terminal default responder and must be attached (subscribed) last.
using session_admin = session_server<protocol_admin>;
using session_native = session_server<protocol_native>;
using session_bitcoind = session_server<protocol_bitcoind_rest,
    protocol_bitcoind_blockchain, protocol_bitcoind_control,
    protocol_bitcoind_mining, protocol_bitcoind_network,
    protocol_bitcoind_notifications, protocol_bitcoind_test,
    protocol_bitcoind_transaction, protocol_bitcoind_utility,
    protocol_bitcoind_wallet, protocol_bitcoind>;
using session_btcd = session_server<protocol_btcd,
    protocol_bitcoind_blockchain, protocol_bitcoind_control,
    protocol_bitcoind_mining, protocol_bitcoind_network,
    protocol_bitcoind_notifications, protocol_bitcoind_test,
    protocol_bitcoind_transaction, protocol_bitcoind_utility,
    protocol_bitcoind_wallet, protocol_bitcoind>;
using session_stratum_v1 = session_server<protocol_stratum_v1>;
using session_stratum_v2 = session_server<protocol_stratum_v2>;
using session_bitcoind_zmq = session_server<protocol_bitcoind_zmq>;
using session_electrum = session_handshake<protocol_electrum_version,
    protocol_electrum>;
using session_sparrow = session_handshake<protocol_electrum_version,
    protocol_sparrow>;

} // namespace server
} // namespace libbitcoin

#endif

/*

Network provides the base for all channels as client-server (server) and
peer-to-peer (peer). ══ represents template instantiation (vs. derivation).

network::session
├── session_server
│   └── [server::session_server<...Protocols>]
└── session_peer
    ├── session_seed
    ├── session_inbound
    │   └── [node::session_peer<network::session_inbound>]
    ├── session_outbound
    │   └── [node::session_peer<network::session_outbound>]
    └── session_manual
        └── [node::session_peer<network::session_manual>]

node::session
├── [server::session]
└── session_peer<NetworkSession> → NetworkSession
    ╞══ session_peer<network::session_inbound>
    │   └── session_inbound
    ╞══ session_peer<network::session_outbound>
    │   └── session_outbound
    ╘══ session_peer<network::session_manual>
        └── session_manual

server::session → node::session
└── server::session_server<...Protocols> → network::session_server
    ╞══ session_admin      = server::session_server<protocol_admin>
    ╞══ session_native     = server::session_server<protocol_native>
    ╞══ session_bitcoind   = server::session_server<protocol_bitcoind_rest,
            protocol_bitcoind_<subgroup>..., protocol_bitcoind>
    ╞══ session_btcd       = server::session_server<protocol_btcd,
            protocol_bitcoind_<subgroup>..., protocol_bitcoind>
    ╞══ session_stratum_v1 = server::session_server<protocol_stratum_v1>
    ╞══ session_stratum_v2 = server::session_server<protocol_stratum_v2>
    ╞══ session_bitcoind_zmq = server::session_server<protocol_bitcoind_zmq>
    └── server::session_handshake<...Protocols>
        ╞══ session_electrum = server::session_handshake<
        │       protocol_electrum_version, protocol_electrum>
        ╘══ session_sparrow  = server::session_handshake<
                protocol_electrum_version, protocol_sparrow>

The bitcoind interface subgroups (blockchain, control, mining, network,
notifications, test, transaction, utility, wallet) are independent protocols,
each with its own interface dispatcher, attached to the same channel. A
subgroup claims each request defined by its interface (via the channel latch);
protocol_bitcoind is the terminal default responder, attached last, replying
only to unclaimed requests. The first protocol supplies channel_t/options_t.

protocol_sparrow derives protocol_electrum and carries a second dispatcher
for the methods sparrow adds, as protocol_btcd does over protocol_bitcoind.
So one protocol serves both interfaces, and the electrum handlers it wants to
extend (server.features) are overridden rather than reimplemented.

*/
