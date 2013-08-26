#ifndef OBELISK_CLIENT_BACKEND
#define OBELISK_CLIENT_BACKEND

#include <functional>
#include <unordered_map>
#include <system_error>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <zmq.hpp>
#include <obelisk/message.hpp>

typedef bc::data_chunk worker_uuid;

class backend_cluster
{
public:
    typedef std::function<void (
        const bc::data_chunk&, const worker_uuid&)> response_handler;

    backend_cluster(zmq::context_t& context, const std::string& connection);

    void request(
        const std::string& command, const bc::data_chunk& data,
        response_handler handle, const worker_uuid& dest=worker_uuid());
    void update();

private:
    struct request_container
    {
        boost::posix_time::ptime timestamp;
        boost::posix_time::time_duration timeout;
        size_t retries_left;
        outgoing_message message;
    };

    typedef std::unordered_map<uint32_t, response_handler>
        response_handler_map;
    typedef std::unordered_map<uint32_t, request_container>
        request_retry_queue;

    void send(const outgoing_message& message);
    void receive_incoming();
    bool process(const incoming_message& response);
    void resend_expired();

    zmq::context_t& context_;
    zmq::socket_t socket_;
    // Requests
    response_handler_map handlers_;
    request_retry_queue retry_queue_;
    // Subscriptions
};

#endif

