#include "worker.hpp"

#include <bitcoin/format.hpp>
#include <bitcoin/utility/logger.hpp>
#include <obelisk/zmq_message.hpp>
#include "echo.hpp"

// Needed for the ZMQ version macros below.
#include <zmq.h>

namespace obelisk {

using namespace bc;
using std::placeholders::_1;
namespace posix_time = boost::posix_time;
using posix_time::milliseconds;
using posix_time::seconds;
using posix_time::microsec_clock;

const posix_time::time_duration heartbeat_interval = milliseconds(1000);

#if ZMQ_VERSION_MAJOR >= 3
    // Milliseconds
    constexpr long poll_sleep_interval = 500;
#elif ZMQ_VERSION_MAJOR == 2
    // Microseconds
    constexpr long poll_sleep_interval = 500000;
#else
    #error ZMQ_VERSION_MAJOR macro undefined.
#endif

auto now = []() { return microsec_clock::universal_time(); };

socket_factory::socket_factory()
  : context(1)
{
}
zmq_socket_ptr socket_factory::spawn_socket()
{
    zmq_socket_ptr socket =
        std::make_shared<zmq::socket_t>(context, ZMQ_ROUTER);
    // Set the socket identity name.
    if (!name.empty())
        socket->setsockopt(ZMQ_IDENTITY, name.c_str(), name.size());
    // Connect...
    socket->bind(connection.c_str());
    // Configure socket to not wait at close time
    int linger = 0;
    socket->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
    return socket;
}

send_worker::send_worker(zmq::context_t& context)
  : context_(context)
{
}
void send_worker::queue_send(const outgoing_message& message)
{
    zmq::socket_t queue_socket(context_, ZMQ_PUSH);
    queue_socket.connect("inproc://trigger-send");
    message.send(queue_socket);
}

request_worker::request_worker()
  : sender_(factory_.context),
    wakeup_socket_(factory_.context, ZMQ_PULL),
    heartbeat_socket_(factory_.context, ZMQ_PUB)
{
    wakeup_socket_.bind("inproc://trigger-send");
}
bool request_worker::start(config_type& config)
{
    // Load config values.
    factory_.connection = config.service;
    factory_.name = config.name;
    log_requests_ = config.log_requests;
    // Start ZeroMQ sockets.
    create_new_socket();
    log_debug(LOG_WORKER) << "Heartbeat: " << config.heartbeat;
    heartbeat_socket_.bind(config.heartbeat.c_str());
    // Timer stuff
    heartbeat_at_ = now() + heartbeat_interval;
}
void request_worker::stop()
{
}

void request_worker::create_new_socket()
{
    log_debug(LOG_WORKER) << "Listening: " << factory_.connection;
    socket_ = factory_.spawn_socket();
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
    zmq::pollitem_t items [] = {
        { *socket_,  0, ZMQ_POLLIN, 0 },
        { wakeup_socket_,  0, ZMQ_POLLIN, 0 } };
    int rc = zmq::poll(items, 2, poll_sleep_interval);
    BITCOIN_ASSERT(rc >= 0);

    // TODO: refactor this code.

    if (items[0].revents & ZMQ_POLLIN)
    {
        // Get message
        // - 6-part envelope + content -> request
        // - 1-part "HEARTBEAT" -> heartbeat
        incoming_message request;
        request.recv(*socket_);

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
    else if (items[1].revents & ZMQ_POLLIN)
    {
        // Send queued message.
        zmq_message message;
        message.recv(wakeup_socket_);
        message.send(*socket_);
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
    zmq_message message;
    message.append(uncast_type(counter));
    message.send(heartbeat_socket_);
    ++counter;
}

} // namespace obelisk

