#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/backend.hpp>

namespace obelisk {

using namespace bc;

namespace posix_time = boost::posix_time;
using posix_time::seconds;
using posix_time::microsec_clock;

constexpr size_t request_retries = 3;
const posix_time::time_duration request_timeout_init = seconds(30);

backend_cluster::backend_cluster(threadpool& pool,
    czmqpp::context& context, const std::string& connection,
    const std::string& cert_filename, const std::string& server_pubkey)
  : context_(context), socket_(context_, ZMQ_DEALER), strand_(pool)
{
    BITCOIN_ASSERT(socket_.self());
    if (!server_pubkey.empty())
        enable_crypto(cert_filename, server_pubkey);
    // Connect
    socket_.connect(connection);
    // Configure socket to not wait at close time.
    socket_.set_linger(0);
}

void backend_cluster::enable_crypto(
    const std::string& cert_filename, const std::string& server_pubkey)
{
    cert_.reset(czmqpp::load_cert(cert_filename));
    BITCOIN_ASSERT(cert_.self());
    cert_.apply(socket_);
    socket_.set_curve_serverkey(server_pubkey);
}

void backend_cluster::request(
    const std::string& command, const data_chunk& data,
    response_handler handle, const worker_uuid& dest)
{
    request_container request{
        microsec_clock::universal_time(), request_timeout_init,
        request_retries, outgoing_message(dest, command, data)};
    auto insert_request = [this, handle, request]
    {
        handlers_[request.message.id()] = handle;
        retry_queue_[request.message.id()] = request;
        send(request.message);
    };
    strand_.randomly_queue(insert_request);
}
void backend_cluster::send(const outgoing_message& message)
{
    BITCOIN_ASSERT(socket_.self());
    message.send(socket_);
}

void backend_cluster::update()
{
    // Poll socket for a reply, with timeout
    czmqpp::poller poller(socket_);
    BITCOIN_ASSERT(poller.self());
    czmqpp::socket which = poller.wait(0);
    // If we got a reply, process it
    receive_incoming();
    // Finally resend any expired requests that we didn't get
    // a response to yet.
    strand_.randomly_queue(
        &backend_cluster::resend_expired, this);
}

void backend_cluster::append_filter(
    const std::string& command, response_handler filter)
{
    strand_.randomly_queue(
        [this, command, filter]
        {
            filters_[command] = filter;
        });
}

void backend_cluster::receive_incoming()
{
    incoming_message response;
    if (!response.recv(socket_))
        return;
    strand_.randomly_queue(
        &backend_cluster::process, this, response);
}

void backend_cluster::process(const incoming_message& response)
{
    if (process_filters(response))
        return;
    if (process_as_reply(response))
        return;
}
bool backend_cluster::process_filters(const incoming_message& response)
{
    auto filter_it = filters_.find(response.command());
    if (filter_it == filters_.end())
        return false;
    filter_it->second(response.data(), response.origin());
    return true;
}
bool backend_cluster::process_as_reply(const incoming_message& response)
{
    auto handle_it = handlers_.find(response.id());
    // Unknown response. Not in our map.
    if (handle_it == handlers_.end())
        return false;
    handle_it->second(response.data(), response.origin());
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
        send(request.message);
    }
}

} // namespace obelisk

