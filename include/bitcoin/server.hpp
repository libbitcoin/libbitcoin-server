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
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/dispatch.hpp>
#include <bitcoin/server/parser.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/version.hpp>
#include <bitcoin/server/messaging/incoming_message.hpp>
#include <bitcoin/server/messaging/outgoing_message.hpp>
#include <bitcoin/server/messaging/publisher.hpp>
#include <bitcoin/server/messaging/request_worker.hpp>
#include <bitcoin/server/messaging/send_worker.hpp>
#include <bitcoin/server/messaging/subscribe_manager.hpp>
#include <bitcoin/server/queries/blockchain.hpp>
#include <bitcoin/server/queries/protocol.hpp>
#include <bitcoin/server/queries/transaction_pool.hpp>

#endif
