#include <obelisk/client/interface.hpp>

fullnode_interface::fullnode_interface(const std::string& connection)
  : backend_(connection), blockchain(backend_)
{
}
void fullnode_interface::update()
{
    backend_.update();
}

void fullnode_interface::stop(const std::string& secret)
{
    bc::data_chunk data(secret.begin(), secret.end());
    outgoing_message message("stop", data);
    backend_.send(message);
}

