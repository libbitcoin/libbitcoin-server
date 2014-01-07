#ifndef OBELISK_WORKER_ECHO_HPP
#define OBELISK_WORKER_ECHO_HPP

#include <sstream>

#define LOG_WORKER "worker"
#define LOG_REQUEST "request"

namespace obelisk {

class stdout_wrapper
{
public:
    stdout_wrapper();
    stdout_wrapper(stdout_wrapper&& other);
    ~stdout_wrapper();

    template <typename T>
    stdout_wrapper& operator<<(T const& value)
    {
        stream_ << value;
        return *this;
    }

private:
    std::ostringstream stream_;
};

stdout_wrapper echo();

} // namespace obelisk

#endif

