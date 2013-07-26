#include <obelisk/message.hpp>
#include "echo.hpp"
#include "worker.hpp"
#include "node_impl.hpp"
#include "publisher.hpp"
#include "service/blockchain.hpp"

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;

void stop_worker(bool& stopped, const std::string& stop_secret,
    const incoming_message& request)
{
    const data_chunk& data = request.data();
    std::string user_secret(data.begin(), data.end());
    if (user_secret == stop_secret)
        stopped = true;
}

int main(int argc, char** argv)
{
    config_map_type config;
    if (argc == 2)
    {
        echo() << "Using config file: " << argv[1];
        load_config(config, argv[1]);
    }
    else
        load_config(config, "worker.cfg");
    // Create worker.
    request_worker worker;
    worker.start(config["service"]);
    // Fullnode
    node_impl node;
    // Publisher
    publisher publish(node);
    publish.start(config);
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
    attach("blockchain.fetch_transaction", blockchain_fetch_transaction);
    attach("blockchain.fetch_last_height", blockchain_fetch_last_height);
    attach("blockchain.fetch_block_header", blockchain_fetch_block_header);
    // Method to stop the worker over the network.
    bool stopped = false;
    const std::string stop_secret = config["stop-secret"];
    worker.attach("stop",
        std::bind(stop_worker, std::ref(stopped), stop_secret, _1));
    // Start the node last so that all subscriptions to new blocks
    // don't miss anything.
    if (!node.start(config))
        return 1;
    echo() << "Node started.";
    std::thread thr([&stopped]()
        {
            while (true)
            {
                std::string user_cmd;
                std::getline(std::cin, user_cmd);
                if (user_cmd == "stop")
                    break;
            }
            stopped = true;
        });
    while (!stopped)
    {
        worker.update();
        sleep(0.1);
    }
    thr.join();
    publish.stop();
    if (!node.stop())
        return -1;
    return 0;
}
