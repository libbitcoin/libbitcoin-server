#include <boost/filesystem.hpp>
#include <obelisk/message.hpp>
#include <bitcoin/bitcoin.hpp>
#include <signal.h>
#include <string>
#include "echo.hpp"
#include "worker.hpp"
#include "node_impl.hpp"
#include "publisher.hpp"
#include "subscribe_manager.hpp"
#include "service/fullnode.hpp"
#include "service/blockchain.hpp"
#include "service/protocol.hpp"
#include "service/transaction_pool.hpp"

using namespace obelisk;
using namespace obelisk::bc;

using std::placeholders::_1;
using std::placeholders::_2;

bool stopped = false;
void interrupt_handler(int)
{
    echo() << "Stopping... Please wait.";
    stopped = true;
}

int tmain(int argc, tchar* argv[])
{
    config_type configuration;
    tpath config_path = argc < 2 ?
        tpath(system_config_directory()) / "obelisk" / "worker.cfg" :
        tpath(argv[1]);

    // libconfig is ANSI/MBCS on Windows - no Unicode support.
    // This translates the path from Unicode to a "generic" path in
    // ANSI/MBCS, which can result in failures.
    load_config(configuration, config_path);

    echo() << "Press CTRL-C to shut down.";
    // Create worker.
    request_worker worker;
    worker.start(configuration);
    // Fullnode
    node_impl node;
    // Publisher
    publisher publish(node);
    if (configuration.publisher_enabled)
        if (!publish.start(configuration))
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
    attach("address.fetch_history", fullnode_fetch_history);
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
    attach("transaction_pool.validate", transaction_pool_validate);
    attach("transaction_pool.fetch_transaction",
        transaction_pool_fetch_transaction);
    // Start the node last so that all subscriptions to new blocks
    // don't miss anything.
    if (!node.start(configuration))
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
    if (configuration.publisher_enabled)
        publish.stop();
    if (!node.stop())
        return -1;
    echo() << "Node shutdown cleanly.";
    return 0;
}

