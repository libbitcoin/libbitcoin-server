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

bool send_string(zmq::socket_t& socket, const std::string& str);

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
    void set_socket(zmq_socket_ptr socket);
    void start();
    void stop();
    void queue_send(const outgoing_message& message);
    // Wait for pending sends and send them.
    void send_pending();

private:
    typedef lockless_queue<outgoing_message> send_message_queue;

    bool stopped_ = false;
    zmq_socket_ptr socket_;
    // When the send is ready, then the sending thread is woken up.
    send_message_queue send_queue_;
    std::thread send_thread_;
    // Only the sending thread uses this mutex.
    // Needed by send_condition_.wait(lk) 
    std::mutex mutex_;
    std::condition_variable send_condition_;
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

    zmq::context_t context_;
    std::string connection_;
    zmq_socket_ptr socket_;
    std::string name_;

    boost::posix_time::ptime last_heartbeat_;
    // Send out heartbeats at regular intervals
    boost::posix_time::ptime heartbeat_at_;
    size_t interval_;

    command_map handlers_;
    send_worker sender_;
};

} // namespace obelisk

#endif

