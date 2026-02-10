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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNELS_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNELS_HPP

#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/channels/channel_electrum.hpp>
#include <bitcoin/server/channels/channel_http.hpp>
#include <bitcoin/server/channels/channel_stratum_v1.hpp>
#include <bitcoin/server/channels/channel_stratum_v2.hpp>
#include <bitcoin/server/channels/channel_ws.hpp>

#endif

/*

Network provides the base for all channels as rpc, http, peer. Websocket and
html are not fully independent subclasses (operate within http).

network::channel
├── [server::channel_stratum_v2]
├── channel_rpc<Interface>
│   └── [server::channel_stratum_v1]
│   └── [server::channel_electrum]
├── channel_http
│   ├── [server::channel_http]
│   └── channel_ws
│       └── [server::channel_ws]
└── channel_peer
    └── [node::channel_peer]

node::channel
├── [server::channel]
└── channel_peer       → network::channel_peer

server::channel → node::channel
├── channel_stratum_v2 → network::channel
├── channel_stratum_v1 → network::channel_rpc<interface::stratum_v1>
├── channel_electrum   → network::channel_rpc<interface::electrum>
├── channel_http       → network::channel_http
└── channel_ws         → network::channel_ws

*/
