///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2015 libbitcoin-server developers (see COPYING).
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
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/parser.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/version.hpp>
#include <bitcoin/server/interface/blockchain.hpp>
#include <bitcoin/server/interface/subscribe.hpp>
#include <bitcoin/server/interface/transaction_pool.hpp>
#include <bitcoin/server/interface/unsubscribe.hpp>
#include <bitcoin/server/messages/message.hpp>
#include <bitcoin/server/messages/route.hpp>
#include <bitcoin/server/messages/subscription.hpp>
#include <bitcoin/server/services/block_service.hpp>
#include <bitcoin/server/services/heartbeat_service.hpp>
#include <bitcoin/server/services/query_service.hpp>
#include <bitcoin/server/services/transaction_service.hpp>
#include <bitcoin/server/web/block_socket.hpp>
#include <bitcoin/server/web/heartbeat_socket.hpp>
#include <bitcoin/server/web/query_socket.hpp>
#include <bitcoin/server/web/socket.hpp>
#include <bitcoin/server/web/transaction_socket.hpp>
#include <bitcoin/server/workers/authenticator.hpp>
#include <bitcoin/server/workers/notification_worker.hpp>
#include <bitcoin/server/workers/query_worker.hpp>

#endif
