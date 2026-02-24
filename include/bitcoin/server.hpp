///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2026 libbitcoin-server developers (see COPYING).
//
//        GENERATED SOURCE CODE, DO NOT EDIT EXCEPT EXPERIMENTALLY
//
///////////////////////////////////////////////////////////////////////////////
#ifndef LIBBITCOIN_SERVER_HPP
#define LIBBITCOIN_SERVER_HPP

/**
 * API Users: Include only this header. Direct use of other headers is fragile
 * and unsupported as header organization is subject to change.
 *
 * Maintainers: Do not include this header internal to this library.
 */

#include <bitcoin/node.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/error.hpp>
#include <bitcoin/server/estimator.hpp>
#include <bitcoin/server/parser.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/version.hpp>
#include <bitcoin/server/channels/channel.hpp>
#include <bitcoin/server/channels/channel_electrum.hpp>
#include <bitcoin/server/channels/channel_http.hpp>
#include <bitcoin/server/channels/channel_stratum_v1.hpp>
#include <bitcoin/server/channels/channel_stratum_v2.hpp>
#include <bitcoin/server/channels/channel_ws.hpp>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/interfaces/bitcoind_rest.hpp>
#include <bitcoin/server/interfaces/bitcoind_rpc.hpp>
#include <bitcoin/server/interfaces/electrum.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/interfaces/native.hpp>
#include <bitcoin/server/interfaces/stratum_v1.hpp>
#include <bitcoin/server/interfaces/stratum_v2.hpp>
#include <bitcoin/server/interfaces/types.hpp>
#include <bitcoin/server/parsers/bitcoind_query.hpp>
#include <bitcoin/server/parsers/bitcoind_target.hpp>
#include <bitcoin/server/parsers/electrum_version.hpp>
#include <bitcoin/server/parsers/native_query.hpp>
#include <bitcoin/server/parsers/native_target.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/protocols/protocol.hpp>
#include <bitcoin/server/protocols/protocol_admin.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rest.hpp>
#include <bitcoin/server/protocols/protocol_bitcoind_rpc.hpp>
#include <bitcoin/server/protocols/protocol_electrum.hpp>
#include <bitcoin/server/protocols/protocol_electrum_version.hpp>
#include <bitcoin/server/protocols/protocol_html.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>
#include <bitcoin/server/protocols/protocol_native.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>
#include <bitcoin/server/protocols/protocol_stratum_v1.hpp>
#include <bitcoin/server/protocols/protocol_stratum_v2.hpp>
#include <bitcoin/server/protocols/protocols.hpp>
#include <bitcoin/server/sessions/session.hpp>
#include <bitcoin/server/sessions/session_handshake.hpp>
#include <bitcoin/server/sessions/session_server.hpp>
#include <bitcoin/server/sessions/sessions.hpp>

#endif
