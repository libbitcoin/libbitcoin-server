#ifndef OBELISK_WORKER_SERVICE_UTIL_HPP
#define OBELISK_WORKER_SERVICE_UTIL_HPP

#include <system_error>
#include <functional>
#include <obelisk/message.hpp>

namespace obelisk {

typedef std::function<void (const outgoing_message&)> queue_send_callback;

template <typename Serializer>
void write_error_code(Serializer& serial, const std::error_code& ec)
{
    uint32_t val = ec.value();
    serial.write_4_bytes(val);
}

} // namespace obelisk

#endif

