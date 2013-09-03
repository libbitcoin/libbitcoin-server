#include "echo.hpp"

#include <iostream>

namespace obelisk {

stdout_wrapper::stdout_wrapper()
{
}
stdout_wrapper::stdout_wrapper(stdout_wrapper&& other)
  : stream_(other.stream_.str())
{
}
stdout_wrapper::~stdout_wrapper()
{
    std::cout << stream_.str() << std::endl;
}

stdout_wrapper echo()
{
    return stdout_wrapper();
}

} // namespace obelisk

