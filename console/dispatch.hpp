/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_DISPATCH_HPP
#define LIBBITCOIN_SERVER_DISPATCH_HPP

#include <iostream>
#include <bitcoin/server.hpp>

// Localizable messages.
#define BS_SETTINGS_MESSAGE \
    "These are the configuration settings that can be set."
#define BS_INFORMATION_MESSAGE \
    "Runs a full bitcoin node in the global peer-to-peer network."
#define BS_UNINITIALIZED_CHAIN \
    "The %1% directory is not initialized."
#define BS_INITIALIZING_CHAIN \
    "Please wait while initializing %1% directory..."
#define BS_INITCHAIN_DIR_NEW \
    "Failed to create directory %1% with error, '%2%'."
#define BS_INITCHAIN_DIR_EXISTS \
    "Failed because the directory %1% already exists."
#define BS_INITCHAIN_DIR_TEST \
    "Failed to test directory %1% with error, '%2%'."
#define BS_SERVER_STARTING \
    "Please wait while server is starting."
#define BS_SERVER_START_FAIL \
    "Server failed to start with error, %1%."
#define BS_SERVER_STARTED \
    "Server started, press CTRL-C to stop."
#define BS_SERVER_STOPPING \
    "Please wait while server is stopping (code: %1%)..."
#define BS_SERVER_UNMAPPING \
    "Please wait while files are unmapped..."
#define BS_SERVER_STOP_FAIL \
    "Server stopped with error, %1%."
#define BS_PUBLISHER_START_FAIL \
    "Publisher service failed to start: %1%"
#define BS_PUBLISHER_STOP_FAIL \
    "Publisher service failed to stop."
#define BS_WORKER_START_FAIL \
    "Query service failed to start."
#define BS_WORKER_STOP_FAIL \
    "Query service failed to stop."
#define BS_USING_CONFIG_FILE \
    "Using config file: %1%"
#define BS_INVALID_PARAMETER \
    "Error: %1%"
#define BS_VERSION_MESSAGE \
    "\nVersion Information:\n\n" \
    "libbitcoin-server:     %1%\n" \
    "libbitcoin-node:       %2%\n" \
    "libbitcoin-blockchain: %3%\n" \
    "libbitcoin:            %4%"

namespace libbitcoin {
namespace server {

console_result wait_for_stop(server_node& server);
void monitor_for_stop(server_node& server, server_node::result_handler);

void handle_started(const code& ec, server_node& server);
void handle_running(const code& ec);

// Load arguments and config and then run the server.
console_result dispatch(int argc, const char* argv[], std::istream&,
    std::ostream& output, std::ostream& error);

} // namespace server
} // namespace libbitcoin

#endif
