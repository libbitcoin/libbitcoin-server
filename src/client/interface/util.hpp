#ifndef OBELISK_WORKER_CLIENT_UTIL_HPP
#define OBELISK_WORKER_CLIENT_UTIL_HPP

#include <system_error>

template <typename Deserializer>
bool read_error_code(Deserializer& deserial,
    size_t data_size, std::error_code& ec)
{
    if (data_size < 4)
    {
        log_error() << "No error_code in response.";
        return false;
    }
    uint32_t val = deserial.read_4_bytes();
    ec = static_cast<error::error_code_t>(val);
    return true;
}

#endif

