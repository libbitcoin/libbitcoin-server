#include "worker.hpp"

#include <bitcoin/format.hpp>
#include <bitcoin/utility/logger.hpp>
#include "echo.hpp"

namespace obelisk {

using namespace bc;
using std::placeholders::_1;
namespace posix_time = boost::posix_time;
using posix_time::milliseconds;
using posix_time::seconds;
using posix_time::microsec_clock;

const posix_time::time_duration heartbeat_interval = milliseconds(4000);
// Milliseconds
constexpr long poll_sleep_interval = 1000;

auto now = []() { return microsec_clock::universal_time(); };

send_worker::send_worker(czmqpp::context& context)
  : socket_(context, ZMQ_PUSH)
{
    BITCOIN_ASSERT(socket_.self());
    int rc = socket_.connect("inproc://trigger-send");
    BITCOIN_ASSERT(rc == 0);
}
void send_worker::queue_send(const outgoing_message& message)
{
    message.send(socket_);
}

request_worker::request_worker()
  : auth_(context_),
    sender_(context_),
    socket_(context_, ZMQ_ROUTER),
    wakeup_socket_(context_, ZMQ_PULL),
    heartbeat_socket_(context_, ZMQ_PUB)
{
    BITCOIN_ASSERT(socket_.self());
    BITCOIN_ASSERT(wakeup_socket_.self());
    BITCOIN_ASSERT(heartbeat_socket_.self());
    int rc = wakeup_socket_.bind("inproc://trigger-send");
    BITCOIN_ASSERT(rc != -1);
}
bool request_worker::start(config_type& config)
{
    // Load config values.
    log_requests_ = config.log_requests;
    if (log_requests_)
        auth_.set_verbose(true);
    if (!config.whitelist.empty())
        whitelist(config.whitelist);
    if (config.certificate.empty())
        socket_.set_zap_domain("global");
    else
        enable_crypto(config);
    // Start ZeroMQ sockets.
    create_new_socket(config);
    log_debug(LOG_WORKER) << "Heartbeat: " << config.heartbeat;
    heartbeat_socket_.bind(config.heartbeat);
    // Timer stuff
    heartbeat_at_ = now() + heartbeat_interval;
    return true;
}
void request_worker::stop()
{
}

void request_worker::whitelist(config_type::ipaddress_list& addrs)
{
    for (const std::string& ip_address: addrs)
        auth_.allow(ip_address);
}
void request_worker::enable_crypto(config_type& config)
{
    if (config.client_allowed_certs == "ALLOW_ALL_CERTS")
        auth_.configure_curve("*", CURVE_ALLOW_ANY);
    else
        auth_.configure_curve("*", config.client_allowed_certs);
    cert_.reset(czmqpp::load_cert(config.certificate));
    cert_.apply(socket_);
    socket_.set_curve_server(1);
}
void request_worker::create_new_socket(config_type& config)
{
    log_debug(LOG_WORKER) << "Listening: " << config.service;
    // Set the socket identity name.
    if (!config.name.empty())
        socket_.set_identity(config.name.c_str());
    // Connect...
    socket_.bind(config.service);
    // Configure socket to not wait at close time
    socket_.set_linger(0);
    // Tell queue we're ready for work
    log_info(LOG_WORKER) << "worker ready";
}

void request_worker::attach(
    const std::string& command, command_handler handler)
{
    handlers_[command] = handler;
}

void request_worker::update()
{
    poll();
}

void request_worker::poll()
{
    // Poll for network updates.
    czmqpp::poller poller(socket_, wakeup_socket_);
    BITCOIN_ASSERT(poller.self());
    czmqpp::socket which = poller.wait(poll_sleep_interval);

    BITCOIN_ASSERT(socket_.self() && wakeup_socket_.self());
    if (which == socket_)
    {
        // Get message
        // 6-part envelope + content -> request
        incoming_message request;
        request.recv(socket_);

        auto it = handlers_.find(request.command());
        // Perform request if found.
        if (it != handlers_.end())
        {
            if (log_requests_)
                log_debug(LOG_REQUEST)
                    << request.command() << " from " << request.origin();
            it->second(request,
                std::bind(&send_worker::queue_send, &sender_, _1));
        }
        else
        {
            log_warning(LOG_WORKER)
                << "Unhandled request: " << request.command()
                << " from " << request.origin();
        }
    }
    else if (which == wakeup_socket_)
    {
        // Send queued message.
        czmqpp::message message;
        message.receive(wakeup_socket_);
        message.send(socket_);
    }

    // Publish heartbeat.
    if (now() > heartbeat_at_)
    {
        heartbeat_at_ = now() + heartbeat_interval;
        log_debug(LOG_WORKER) << "Sending heartbeat";
        publish_heartbeat();
    }
}

void request_worker::publish_heartbeat()
{
    static uint32_t counter = 0;
    czmqpp::message message;
    message.append(uncast_type(counter));
    message.send(heartbeat_socket_);
    ++counter;
}

} // namespace obelisk

