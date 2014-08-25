#ifndef OBELISK_WORKER_ECHO_HPP
#define OBELISK_WORKER_ECHO_HPP

#include <sstream>
#include <obelisk/define.hpp>

#define LOG_WORKER "worker"
#define LOG_REQUEST "request"

namespace obelisk {

class stdout_wrapper
{
public:
    BCS_API stdout_wrapper();
    BCS_API stdout_wrapper(stdout_wrapper&& other);
    BCS_API ~stdout_wrapper();

    template <typename T>
    stdout_wrapper& operator<<(T const& value)
    {
        stream_ << value;
        return *this;
    }

private:
    std::ostringstream stream_;
};

BCS_API stdout_wrapper echo();

} // namespace obelisk

#endif

