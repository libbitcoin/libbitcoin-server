#ifndef OBELISK_WORKER_WORKER_HPP
#define OBELISK_WORKER_WORKER_HPP

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <zmq.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <obelisk/message.hpp>
#include "config.hpp"
#include "lockless_queue.hpp"
#include "service/util.hpp"

namespace obelisk {

struct socket_factory
{
    socket_factory();
    zmq_socket_ptr spawn_socket();

    zmq::context_t context;
    std::string connection;
    std::string name;
};

/**
 * We don't want to block the originating threads that execute a send
 * as that would slow down requests if they all have to sync access
 * to a single socket.
 *
 * Instead we have a lockless queue where send requests are pushed,
 * and then the send_worker is notified. The worker wakes up and pushes
 * all pending requests to the socket.
 *
 * We have to manage reconnects, so there's still the need to sync access
 * to the socket which is shared with the receiving request_worker.
 * This is however only for set_socket() and queue_send() so thread
 * contention is kept to the minimum and sending is (mostly) lockfree.
 */
class send_worker
{
public:
    send_worker(zmq::context_t& context);
    void queue_send(const outgoing_message& message);

private:
    typedef lockless_queue<outgoing_message> send_message_queue;

    zmq::context_t& context_;
    // When the send is ready, then the sending thread is woken up.
    send_message_queue send_queue_;
};

class request_worker
{
public:
    typedef std::function<void (
        const incoming_message&, queue_send_callback)> command_handler;

    request_worker();
    bool start(config_type& config);
    void stop();
    void attach(const std::string& command, command_handler handler);
    void update();

private:
    typedef std::unordered_map<std::string, command_handler> command_map;

    void create_new_socket();
    void poll();
    void publish_heartbeat();

    socket_factory factory_;
    // Main socket.
    zmq_socket_ptr socket_;
    // Socket to trigger wakeup for send.
    zmq::socket_t wakeup_socket_;
    // We publish a heartbeat every so often so clients
    // can know our availability.
    zmq::socket_t heartbeat_socket_;

    // Send out heartbeats at regular intervals
    boost::posix_time::ptime heartbeat_at_;

    command_map handlers_;
    send_worker sender_;

    bool log_requests_ = false;
};

} // namespace obelisk

#endif

