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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOLS_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOLS_HPP

#include <bitcoin/server/protocols/protocol.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rest.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>
#include <bitcoin/server/protocols/protocol_electrum.hpp>
#include <bitcoin/server/protocols/protocol_electrum_version.hpp>
#include <bitcoin/server/protocols/protocol_native.hpp>
#include <bitcoin/server/protocols/protocol_html.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>
#include <bitcoin/server/protocols/protocol_stratum_v1.hpp>
#include <bitcoin/server/protocols/protocol_stratum_v2.hpp>
#include <bitcoin/server/protocols/protocol_admin.hpp>

#endif

/*

Network provides the base for all protocols as rpc, http, peer. However node
and server do not inherit directly from network. Server extends node, but
neither inherit from network until a subclass is defined (rpc, http, peer).
Since stratum_v2 is just a stub it doesn't yet have its own subclass. Websocket
and html are not independent subclasses (operate within http).

network::protocol
├── [server:protocol_stratum_v2]
├── protocol_rpc<Channel>
│   └── [server::protocol_rpc<server::channel_stratum_v1>]
│   └── [server::protocol_rpc<server::channel_electrum>]
├── protocol_http
│   ├── [server::protocol_bitcoind_rpc]
│   └── protocol_ws
│       └── [server::protocol_html]
└── protocol_peer
    ├── [node::protocol_peer]
    ├── protocol_alert_311
    ├── protocol_reject_70002
    ├── protocol_seed_209
    ├── protocol_address_in_209
    ├── protocol_address_out_209
    ├── protocol_ping_106
    │   └── protocol_ping_60001
    └── protocol_version_106
        └── protocol_version_70001
            └── protocol_version_70002
                └── protocol_version_70016

node::protocol
├── [server::protocol]
└── protocol_peer → network::protocol_peer
    ├── protocol_observer
    ├── protocol_filter_out_70015
    ├── protocol_block_in_106
    ├── protocol_performer
    │   └── protocol_block_in_31800
    ├── protocol_block_out_106
    │   └── protocol_block_out_70012
    ├── protocol_header_in_31800
    │   └── protocol_header_in_70012
    ├── protocol_header_out_31800
    │   └── protocol_header_out_70012
    ├── protocol_transaction_in_106
    └── protocol_transaction_out_106

server::protocol → node::protocol
├── protocol_stratum_v2              → network::protocol
├── protocol_rpc<channel_stratum_v1> → network::protocol_rpc<channel_stratum_v1>
│   └── protocol_stratum_v1
├── protocol_rpc<channel_electrum>   → network::protocol_rpc<channel_electrum>
│   ├── protocol_electrum
│   └── protocol_electrum_version
└── protocol_http
    ├── protocol_html                → network::protocol_ws
    │   ├── protocol_admin
    │   └── protocol_native
    └── protocol_bitcoind_rpc        → network::protocol_http
        └── protocol_bitcoind_rest

*/
