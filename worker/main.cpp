#include <shared/message.hpp>
#include "worker.hpp"

struct fullnode_impl
{
};

void blockchain_fetch_history(fullnode_impl& fullnode,
    const incoming_message& request, socket_ptr socket)
{
    outgoing_message response(request, bc::data_chunk());
    response.send(*socket);
}

using std::placeholders::_1;
using std::placeholders::_2;

int main()
{
    request_worker worker;
    fullnode_impl fullnode;
    typedef std::function<void (fullnode_impl&,
        const incoming_message&, socket_ptr)> basic_command_handler;
    auto attach = [&worker, &fullnode](
        const std::string& command, basic_command_handler handler)
        {
            worker.attach(command,
                std::bind(handler, std::ref(fullnode), _1, _2));
        };
    attach("blockchain.fetch_history", blockchain_fetch_history);
    while (true)
    {
        worker.update();
        sleep(0.1);
    }
    return 0;
}
