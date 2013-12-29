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

const posix_time::time_duration heartbeat_interval = milliseconds(1000);
constexpr size_t interval_init = 4, interval_max = 32;
constexpr long poll_sleep_interval = 50000;

auto now = []() { return microsec_clock::universal_time(); };

void send_worker::set_socket(zmq_socket_ptr socket)
{
    // Ensure exclusive access to socket device.
    std::unique_lock<std::mutex> lk(mutex_);
    socket_ = socket;
}
void send_worker::start()
{
    // Start thread for processing sends.
    auto process_sends = [this]
    {
        while (!stopped_)
        {
            std::unique_lock<std::mutex> lk(mutex_);
            send_condition_.wait(lk);
            send_pending();
        }
    };
    send_thread_ = std::thread(process_sends);
}
void send_worker::stop()
{
    // Stop send watching thread.
    stopped_ = true;
    send_condition_.notify_one();
    send_thread_.join();
}
void send_worker::queue_send(const outgoing_message& message)
{
    send_queue_.produce(message);
    send_condition_.notify_one();
}
void send_worker::send_pending()
{
    BITCOIN_ASSERT(socket_);
    for (const outgoing_message& message: lockless_iterable(send_queue_))
        message.send(*socket_);
}

request_worker::request_worker()
  : context_(1)
{
}
bool request_worker::start(config_type& config)
{
    connection_ = config.service;
    name_ = config.name;
    create_new_socket();
    last_heartbeat_ = now();
    heartbeat_at_ = now() + heartbeat_interval;
    interval_ = interval_init;
    sender_.start();
}
void request_worker::stop()
{
    sender_.stop();
}

void request_worker::create_new_socket()
{
    socket_ = std::make_shared<zmq::socket_t>(context_, ZMQ_DEALER);
    log_debug(LOG_WORKER) << "Connecting: " << connection_;
    // Set the socket identity name.
    if (!name_.empty())
        socket_->setsockopt(ZMQ_IDENTITY, name_.c_str(), name_.size());
    // Connect...
    socket_->connect(connection_.c_str());
    // Configure socket to not wait at close time
    int linger = 0;
    socket_->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
    // Tell queue we're ready for work
    log_info(LOG_WORKER) << "worker ready";
    send_control_message("READY");
    sender_.set_socket(socket_);
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
    zmq::pollitem_t items [] = { { *socket_,  0, ZMQ_POLLIN, 0 } };
    int rc = zmq::poll(items, 1, poll_sleep_interval);
    BITCOIN_ASSERT(rc >= 0);

    if (items[0].revents & ZMQ_POLLIN)
    {
        // Get message
        // - 6-part envelope + content -> request
        // - 1-part "HEARTBEAT" -> heartbeat
        incoming_message request;
        request.recv(*socket_);

        if (!request.is_signal())
        {
            last_heartbeat_ = now();
            auto it = handlers_.find(request.command());
            // Perform request if found.
            if (it != handlers_.end())
            {
                // TODO: Slows down queries!
                //log_debug(LOG_WORKER)
                //    << request.command() << " from " << request.origin();
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
        else if (request.command() == "HEARTBEAT")
        {
            log_debug(LOG_WORKER) << "Received heartbeat";
            last_heartbeat_ = now();
        }
        else
        {
            log_warning(LOG_WORKER) << "invalid message";
        }
        interval_ = interval_init;
    }
    else if (now() - last_heartbeat_ > seconds(interval_))
    {
        log_warning(LOG_WORKER) << "heartbeat failure, can't reach queue";
        log_warning(LOG_WORKER) << "reconnecting in "
            << interval_ << " seconds...";

        if (interval_ < interval_max)
        {
            interval_ *= 2;
        }
        create_new_socket();
        last_heartbeat_ = now();
    }

    // Send heartbeat to queue if it's time
    if (now() > heartbeat_at_)
    {
        heartbeat_at_ = now() + heartbeat_interval;
        log_debug(LOG_WORKER) << "Sending heartbeat";
        send_control_message("HEARTBEAT");
    }
}

void request_worker::send_control_message(const std::string& command)
{
    outgoing_message message(command);
    sender_.queue_send(message);
}

} // namespace obelisk

