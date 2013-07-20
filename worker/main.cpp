#include "worker.hpp"

int main()
{
    request_worker worker;
    while (true)
    {
        worker.update();
        sleep(0.1);
    }
    return 0;
}
