#ifndef OBELISK_WORKER_WORKER
#define OBELISK_WORKER_WORKER

#include <zmq.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

bool send_string(zmq::socket_t& socket, const std::string& str);

class request_worker
{
public:
    request_worker();
    ~request_worker();

    void update();

private:
    void create_new_socket();

    zmq::context_t context_;
    zmq::socket_t* socket_;

    boost::posix_time::ptime last_heartbeat_;
    // Send out heartbeats at regular intervals
    boost::posix_time::ptime heartbeat_at_;
    size_t interval_;
};

#endif

