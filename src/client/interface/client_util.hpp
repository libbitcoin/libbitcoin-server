#ifndef OBELISK_WORKER_CLIENT_UTIL_HPP
#define OBELISK_WORKER_CLIENT_UTIL_HPP

#include <system_error>
#include <bitcoin/bitcoin.hpp>
#include <obelisk/define.hpp>

namespace obelisk {

template <typename Deserializer>
bool read_error_code(Deserializer& deserial,
    size_t data_size, std::error_code& ec)
{
    if (data_size < 4)
    {
        bc::log_error() << "No error_code in response.";
        return false;
    }
    uint32_t val = deserial.read_4_bytes();
    if (val)
        ec = static_cast<bc::error::error_code_t>(val);
    return true;
}

} // namespace obelisk

#endif

