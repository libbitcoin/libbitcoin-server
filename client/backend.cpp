#include "backend.hpp"

#include <bitcoin/utility/assert.hpp>

using namespace bc;

backend_cluster::backend_cluster()
  : context_(1), socket_(context_, ZMQ_DEALER)
{
    socket_.connect("tcp://localhost:5555");
    // Configure socket to not wait at close time.
    int linger = 0;
    socket_.setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
}

void backend_cluster::request(const std::string& command,
    const data_chunk& data, response_handler handle)
{
    constexpr size_t request_retries = 3;
    request_container request{
        time(nullptr), request_retries, outgoing_message(command, data)};
    request.message.send(socket_);
    handlers_[request.message.id()] = handle;
    retry_queue_[request.message.id()] = request;
}

void backend_cluster::update()
{
    constexpr size_t request_timeout = 2500;
    //  Poll socket for a reply, with timeout
    zmq::pollitem_t items[] = { { socket_, 0, ZMQ_POLLIN, 0 } };
    zmq::poll(&items[0], 1, request_timeout * 1000);

    //  If we got a reply, process it
    if (items[0].revents & ZMQ_POLLIN)
    {
        incoming_message response;
        if (response.recv(socket_))
            process(response);
    }

    // check timestamp of requests
    // if retries left then resend, update timestamp
    // else server seems to be offline...
}

bool backend_cluster::process(const incoming_message& response)
{
    auto handle_it = handlers_.find(response.id());
    // Unknown response. Not in our map.
    if (handle_it == handlers_.end())
        return false;
    handle_it->second(response.data());
    handlers_.erase(handle_it);
    size_t n_erased = retry_queue_.erase(response.id());
    BITCOIN_ASSERT(n_erased == 1);
    return true;
}

