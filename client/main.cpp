#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/interface.hpp>

using namespace bc;

bool stopped = false;

#define LOG_RESULT "result"

void history_fetched(const std::error_code& ec,
    const blockchain::history_list& history)
{
    if (ec)
    {
        log_error() << "Failed to fetch history: " << ec.message();
        return;
    }
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

void tx_fetched(const std::error_code& ec, const transaction_type& tx)
{
    if (ec)
    {
        log_error() << "Failed to fetch history: " << ec.message();
        return;
    }
    log_debug(LOG_RESULT) << "Fetched tx: " << hash_transaction(tx);
}

void last_height_fetched(const std::error_code& ec, size_t last_height)
{
    if (ec)
    {
        log_error() << "Failed to fetch history: " << ec.message();
        return;
    }
    log_debug(LOG_RESULT) << "Block #" << last_height;
}

void block_header_fetched(const std::error_code& ec,
    const block_header_type& blk)
{
    if (ec)
    {
        log_error() << "Failed to fetch block header: " << ec.message();
        return;
    }
    log_debug(LOG_RESULT) << "Block " << hash_block_header(blk);
}

int main(int argc, char** argv)
{
    //if (argc != 2)
    //{
    //    log_info() << "Usage: balance ADDRESS";
    //    return -1;
    //}
    //std::string addr = argv[1];
    //payment_address payaddr;
    //if (!payaddr.set_encoded(addr))
    //{
    //    log_error() << "Invalid address";
    //    return -1;
    //}
    fullnode_interface fullnode("tcp://localhost:5555");
    //fullnode.stop("password");
    //sleep(1);
    //return 0;
    //fullnode.blockchain.fetch_history(payaddr, history_fetched);
    //fullnode.blockchain.fetch_transaction(
    //    decode_hex_digest<hash_digest>("fb13bc04ac2d701caa630ce7a8588b69c13120a8083aad568a17d341943e978e"),
    //    tx_fetched);
    //fullnode.blockchain.fetch_last_height(last_height_fetched);
    fullnode.blockchain.fetch_block_header(
        decode_hex_digest<hash_digest>("010000000000006a4c0127f26e6e57f9db53924d6f94919edb519faf68099092"),
        block_header_fetched);
    while (!stopped)
    {
        fullnode.update();
        sleep(0.1);
    }
    return 0;
}

