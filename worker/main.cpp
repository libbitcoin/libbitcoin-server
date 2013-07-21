#include <obelisk/message.hpp>
#include "echo.hpp"
#include "worker.hpp"
#include "node_impl.hpp"
#include "service/blockchain.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

int main()
{
    config_map_type config;
    load_config(config, "worker.cfg");
    // Create worker.
    request_worker worker;
    // Fullnode
    node_impl node;
    // Attach commands
    typedef std::function<void (node_impl&,
        const incoming_message&, zmq_socket_ptr)> basic_command_handler;
    auto attach = [&worker, &node](
        const std::string& command, basic_command_handler handler)
        {
            worker.attach(command,
                std::bind(handler, std::ref(node), _1, _2));
        };
    attach("blockchain.fetch_history", blockchain_fetch_history);
    // Start the node last so that all subscriptions to new blocks
    // don't miss anything.
    if (!node.start(config))
        return 1;
    echo() << "Node started.";
    while (true)
    {
        worker.update();
        sleep(0.1);
    }
    if (!node.stop())
        return -1;
    return 0;
}
