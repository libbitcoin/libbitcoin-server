/*
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
#include <signal.h>
#include <string>
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include "echo.hpp"
#include "message.hpp"
#include "node_impl.hpp"
#include "publisher.hpp"
#include "service/blockchain.hpp"
#include "service/fullnode.hpp"
#include "service/protocol.hpp"
#include "service/transaction_pool.hpp"
#include "service/compat.hpp"
#include "subscribe_manager.hpp"
#include "worker.hpp"

using namespace server;

using std::placeholders::_1;
using std::placeholders::_2;
using boost::filesystem::path;

bool stopped = false;
void interrupt_handler(int)
{
    echo() << "Stopping... Please wait.";
    stopped = true;
}

int main(int argc, char** argv)
{
    config_type config;
    path config_path = argc < 2 ?
        path(system_config_directory()) / "libbitcoin" / "server.cfg" :
        path(argv[1]);

    load_config(config, config_path);

    echo() << "Press CTRL-C to shut down.";
    // Create worker.
    request_worker worker;
    worker.start(config);
    // Fullnode
    node_impl node(config);
    // Publisher
    publisher publish(node);
    if (config.publisher_enabled)
        if (!publish.start(config))
        {
            std::cerr << "Failed to start publisher: "
                << zmq_strerror(zmq_errno()) << std::endl;
            return 1;
        }
    // Address subscriptions
    subscribe_manager addr_sub(node);
    // Attach commands
    typedef std::function<void (node_impl&,
        const incoming_message&, queue_send_callback)> basic_command_handler;
    auto attach = [&worker, &node](
        const std::string& command, basic_command_handler handler)
    {
        worker.attach(command,
            std::bind(handler, std::ref(node), _1, _2));
    };
    worker.attach("address.subscribe",
        std::bind(&subscribe_manager::subscribe, &addr_sub, _1, _2));
    worker.attach("address.renew",
        std::bind(&subscribe_manager::renew, &addr_sub, _1, _2));
    attach("address.fetch_history2", fullnode_fetch_history);
    attach("blockchain.fetch_history", blockchain_fetch_history);
    attach("blockchain.fetch_transaction", blockchain_fetch_transaction);
    attach("blockchain.fetch_last_height", blockchain_fetch_last_height);
    attach("blockchain.fetch_block_header", blockchain_fetch_block_header);
    attach("blockchain.fetch_block_transaction_hashes",
        blockchain_fetch_block_transaction_hashes);
    attach("blockchain.fetch_transaction_index",
        blockchain_fetch_transaction_index);
    attach("blockchain.fetch_spend", blockchain_fetch_spend);
    attach("blockchain.fetch_block_height", blockchain_fetch_block_height);
    attach("blockchain.fetch_stealth", blockchain_fetch_stealth);
    attach("protocol.broadcast_transaction", protocol_broadcast_transaction);
    attach("protocol.total_connections", protocol_total_connections);
    attach("transaction_pool.validate", transaction_pool_validate);
    attach("transaction_pool.fetch_transaction",
        transaction_pool_fetch_transaction);
    // Obsolete command for backwards compatibility.
    attach("address.fetch_history", COMPAT_fetch_history);
    // Start the node last so that all subscriptions to new blocks
    // don't miss anything.
    if (!node.start(config))
    {
        std::cerr << "Failed to start Bitcoin node." << std::endl;
        return 1;
    }
    echo() << "Node started.";
    signal(SIGINT, interrupt_handler);
    // Main loop.
    while (!stopped)
        worker.update();
    worker.stop();
    if (config.publisher_enabled)
        publish.stop();
    if (!node.stop())
        return -1;
    echo() << "Node shutdown cleanly.";
    return 0;
}

