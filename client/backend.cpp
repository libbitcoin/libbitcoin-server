#include <obelisk/client/backend.hpp>

#include <bitcoin/utility/assert.hpp>

using namespace bc;

namespace posix_time = boost::posix_time;
using posix_time::seconds;
using posix_time::microsec_clock;

constexpr size_t request_retries = 3;
const posix_time::time_duration request_timeout_init = seconds(30);

backend_cluster::backend_cluster(const std::string& connection)
  : context_(1), socket_(context_, ZMQ_DEALER)
{
    socket_.connect(connection.c_str());
    // Configure socket to not wait at close time.
    int linger = 0;
    socket_.setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
}

void backend_cluster::request(const std::string& command,
    const data_chunk& data, response_handler handle)
{
    request_container request{
        microsec_clock::universal_time(), request_timeout_init,
        request_retries, outgoing_message(command, data)};
    request.message.send(socket_);
    handlers_[request.message.id()] = handle;
    retry_queue_[request.message.id()] = request;
}

void backend_cluster::update()
{
    //  Poll socket for a reply, with timeout
    zmq::pollitem_t items[] = { { socket_, 0, ZMQ_POLLIN, 0 } };
    zmq::poll(&items[0], 1, 0);
    //  If we got a reply, process it
    if (items[0].revents & ZMQ_POLLIN)
    {
        incoming_message response;
        if (response.recv(socket_))
            process(response);
    }
    // Finally resend any expired requests that we didn't get
    // a response to yet.
    resend_expired();
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

void backend_cluster::resend_expired()
{
    const posix_time::ptime now = microsec_clock::universal_time();
    for (auto& retry: retry_queue_)
    {
        request_container& request = retry.second;
        if (now - request.timestamp < request.timeout)
            continue;
        if (request.retries_left == 0)
        {
            // Unhandled... Appears there's a problem with the server.
            request.retries_left = request_retries;
            return;
        }
        request.timeout *= 2;
        --request.retries_left;
        // Resend request again.
        request.timestamp = now;
        request.message.send(socket_);
    }
}

