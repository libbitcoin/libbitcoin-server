#include <bitcoin/bitcoin.hpp>
using namespace bc;

#include "interface.hpp"

bool stopped = false;

void history_fetched(const std::error_code& ec,
    const blockchain::history_list& history)
{
    if (ec)
    {
        log_error() << "Failed to fetch history: " << ec.message();
        return;
    }
#define LOG_RESULT "result"
    uint64_t total_recv = 0, balance = 0;
    for (const auto& row: history)
    {
        uint64_t value = row.value;
        BITCOIN_ASSERT(value >= 0);
        total_recv += value;
        if (row.spend.hash == null_hash)
            balance += value;
    }
    log_debug(LOG_RESULT) << "Queried " << history.size()
        << " outpoints, values and their spends.";
    log_debug(LOG_RESULT) << "Total received: " << total_recv;
    log_debug(LOG_RESULT) << "Balance: " << balance;
    log_info(LOG_RESULT) << "History fetched";
    stopped = true;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        log_info() << "Usage: balance ADDRESS";
        return -1;
    }
    std::string addr = argv[1];
    payment_address payaddr;
    if (!payaddr.set_encoded(addr))
    {
        log_error() << "Invalid address";
        return -1;
    }
    fullnode_interface fullnode;
    fullnode.blockchain.fetch_history(payaddr, history_fetched);
    while (!stopped)
    {
        fullnode.update();
        sleep(0.1);
    }
    return 0;
}

