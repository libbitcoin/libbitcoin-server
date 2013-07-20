#include "worker.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <bitcoin/utility/logger.hpp>

#include <shared/message.hpp>
#include <shared/zmq_message.hpp>

using namespace bc;
namespace posix_time = boost::posix_time;
using posix_time::milliseconds;
using posix_time::seconds;
using posix_time::microsec_clock;

const posix_time::time_duration heartbeat_interval = milliseconds(1000);
constexpr size_t interval_init = 4, interval_max = 32;

auto now = []() { return microsec_clock::universal_time(); };

bool send_string(zmq::socket_t& socket, const std::string& str)
{
    zmq::message_t message(str.size());
    char* msg_begin = reinterpret_cast<char*>(message.data());
    std::copy(str.begin(), str.end(), msg_begin);
    return socket.send(message);
}

request_worker::request_worker()
  : context_(1)
{
    create_new_socket();
    last_heartbeat_ = now();
    heartbeat_at_ = now() + heartbeat_interval;
    interval_ = interval_init;
}
request_worker::~request_worker()
{
    delete socket_;
}

void request_worker::create_new_socket()
{
    socket_ = new zmq::socket_t(context_, ZMQ_DEALER);
    socket_->connect("tcp://localhost:5556");
    // Configure socket to not wait at close time
    int linger = 0;
    socket_->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
    // Tell queue we're ready for work
    log_info() << "I: worker ready";
    send_string(*socket_, "READY");
}

void request_worker::update()
{
    zmq::pollitem_t items [] = { { *socket_,  0, ZMQ_POLLIN, 0 } };
    zmq::poll(items, 1, 0);

    if (items [0].revents & ZMQ_POLLIN)
    {
        // Get message
        // - 6-part envelope + content -> request
        // - 1-part "HEARTBEAT" -> heartbeat
        incoming_message request;
        request.recv(*socket_);

        if (!request.is_signal())
        {
            last_heartbeat_ = now();
            log_info() << "I: normal reply";
            outgoing_message response(request, bc::data_chunk());
            response.send(*socket_);
            // Do some heavy work
            sleep(1);
        }
        else if (request.command() == "HEARTBEAT")
        {
            last_heartbeat_ = now();
        }
        else
        {
            log_warning() << "E: invalid message";
        }
        interval_ = interval_init;
    }
    else if (now() - last_heartbeat_ > seconds(interval_))
    {
        log_warning() << "W: heartbeat failure, can't reach queue";
        log_warning() << "W: reconnecting in " << interval_ << " seconds...";

        if (interval_ < interval_max)
        {
            interval_ *= 2;
        }
        delete socket_;
        create_new_socket();
        last_heartbeat_ = now();
    }

    // Send heartbeat to queue if it's time
    if (now() > heartbeat_at_)
    {
        heartbeat_at_ = now() + heartbeat_interval;
        log_info() << "I: worker heartbeat";
        send_string(*socket_, "HEARTBEAT");
    }
}

