/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "executor.hpp"
#include "localize.hpp"

namespace libbitcoin {
namespace server {

using boost::format;
using namespace network;
using namespace system;

// arbitrary testing (const).

void executor::read_test(const hash_digest&) const
{
    logger("Wire size computation.");
    const auto start = fine_clock::now();
    const auto top = query_.get_top_associated();
    const auto concurrency = metadata_.configured.node.maximum_concurrency_();

    size_t size{};
    for (auto height = zero; !canceled() && height <= top; ++height)
    {
        const auto link = query_.to_candidate(height);
        if (link.is_terminal())
        {
            logger(format("Max candidate height is (%1%).") % sub1(height));
            return;
        }

        const auto bytes = query_.get_block_size(link, true);
        if (is_zero(bytes))
        {
            logger(format("Block (%1%) is not associated.") % height);
            return;
        }

        size += bytes;
        if (is_zero(height % concurrency))
        {
            const auto span = duration_cast<milliseconds>(fine_clock::now() - start);
            logger(format("Wire size (%1%) at (%2%) in (%3%) ms.") %
                size % height % span.count());
        }
    }

    const auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Wire size (%1%) at (%2%) in (%3%) ms.") %
        size % top % span.count());
}

#if defined(UNDEFINED)

void executor::read_test(const hash_digest&) const
{
    logger(format("Point table body searches: %1% / (%2% + %1%)") %
        store_.point.positive_search_count() %
        store_.point.negative_search_count());
}

void executor::read_test(const hash_digest&) const
{
    using namespace database;
    constexpr auto start_tx = 1'000'000_u32;
    constexpr auto target_count = 100_size;
    constexpr auto dump{ false };

    // Set ensures unique addresses.
    std::set<hash_digest> keys{};
    auto tx = start_tx;

    logger(format("Getting first [%1%] output address hashes.") % target_count);

    auto start = fine_clock::now();
    while (!canceled() && keys.size() < target_count)
    {
        const auto outputs = query_.get_outputs(tx++);
        if (is_null(outputs))
        {
            // fault, tx with no outputs.
            return;
        }

        for (const auto& put: *outputs)
        {
            keys.emplace(put->script().hash());
            if (canceled() || keys.size() == target_count)
                break;
        }
    }

    auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Got first [%1%] unique addresses above tx [%2%] in [%3%] ms.") %
        keys.size() % start_tx % span.count());

    struct out
    {
        hash_digest address;

        uint32_t bk_fk;
        uint32_t bk_height;
        hash_digest bk_hash;

        uint32_t tx_fk;
        size_t tx_position;
        hash_digest tx_hash;

        uint32_t pt_tx_fk;
        hash_digest pt_tx_hash;

        uint64_t input_fk;
        chain::input::cptr input{};

        uint64_t output_fk;
        chain::output::cptr output{};
    };

    std_vector<out> outs{};
    outs.reserve(target_count);

    start = fine_clock::now();
    for (auto& key: keys)
    {
        if (canceled())
            return;

        ////size_t found{};
        auto address_it = store_.address.it(key);
        if (address_it.get().is_terminal())
        {
            // fault, missing address.
            return;
        }

        do
        {
            if (canceled())
                break;

            table::address::record address{};
            if (!store_.address.get(address_it.get(), address))
            {
                // fault, missing address.
                return;
            }

            const auto out_fk = address.output_fk;
            table::output::get_parent output{};
            if (!store_.output.get(out_fk, output))
            {
                // fault, missing output.
                return;
            }

            const auto tx_fk = output.parent_fk;
            if (!store_.tx.exists(query_.get_tx_key(tx_fk)))
            {
                // fault, missing tx.
                return;
            }

            // There may be not-strong txs but we just won't count those.
            const auto block_fk = query_.to_block(tx_fk);
            if (block_fk.is_terminal())
                continue;

            table::header::get_height header{};
            if (!store_.header.get(block_fk, header))
            {
                // fault, missing block.
                return;
            }

            table::txs::get_position txs{ {}, tx_fk };
            if (!store_.txs.get(query_.to_txs(block_fk), txs))
            {
                // fault, missing txs.
                return;
            }

            point_link pt_fk{};
            input_link in_fk{};
            tx_link pt_tx_fk{};

            // Get first spender only (may or may not be confirmed).
            const auto points = query_.to_spenders(out_fk);
            if (!points.empty())
            {
                pt_fk = points.front();
                table::ins::record ins{};
                if (!store_.ins.get(pt_fk, ins))
                {
                    // fault, missing ins.
                    return;
                }

                in_fk = ins.input_fk;
                pt_tx_fk = ins.parent_fk;
            }

            ////++found;
            outs.push_back(out
            {
                key,

                block_fk,
                header.height,
                query_.get_header_key(block_fk),

                tx_fk,
                txs.position,
                query_.get_tx_key(tx_fk),

                pt_tx_fk,
                query_.get_tx_key(pt_tx_fk),

                in_fk,
                query_.get_input(pt_fk),

                out_fk,
                query_.get_output(out_fk)
            });
        }
        while (address_it.advance());

        // This affects the clock, so disabled.
        ////logger(format("Fetched [%1%] unique payments to address [%2%].") %
        ////    found % encode_hash(key));
    }

    span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Got all [%1%] payments to [%2%] addresses in [%3%] ms.") %
        outs.size() % keys.size() % span.count());

    if (!dump)
        return;

    // Write it all...
    logger(
        "output_script_hash, "
    
        "ouput_bk_fk, "
        "ouput_bk_height, "
        "ouput_bk_hash, "

        "ouput_tx_fk, "
        "ouput_tx_position, "
        "ouput_tx_hash, "

        "input_tx_fk, "
        "input_tx_hash, "

        "output_fk, "
        "output_script, "

        "input_fk, "
        "input_script"
    );

    for (const auto& row: outs)
    {
        if (canceled())
            break;

        const auto output = !row.output ? "{error}" :
            row.output->script().to_string(chain::flags::all_rules);

        const auto input = !row.input ? "{unspent}" :
            row.input->script().to_string(chain::flags::all_rules);
    
        logger(format("%1%, %2%, %3%, %4%, %5%, %6%, %7%, %8%, %9%, %10%, %11%, %12%, %13%") %
            encode_hash(row.address) %

            row.bk_fk %
            row.bk_height %
            encode_hash(row.bk_hash) %

            row.tx_fk %
            row.tx_position %
            encode_hash(row.tx_hash) %

            row.pt_tx_fk %
            encode_hash(row.pt_tx_hash) %

            row.output_fk %
            output %

            row.input_fk %
            input);
    }
}

void executor::read_test(const hash_digest&) const
{
    database::header_link link{ 350'017_u32 };
    const auto ec = query_.block_confirmable(link);
    logger(format("block_confirmable [%1%] at height [%2%].") % ec.message() %
        query_.get_height(link));
}

void executor::read_test(const hash_digest&) const
{
    auto start = fine_clock::now();
    auto count = query_.header_records();
    uint32_t block{ one };

    logger("Find strong blocks.");
    while (!canceled() && (block < count) && query_.is_strong_block(block))
    {
        ++block;
    }

    auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Top strong block is [%1%] in [%2%] ms.") % sub1(block) % span.count());
    start = fine_clock::now();
    count = query_.header_records();
    uint32_t milestone{ 295'001 };

    logger("Find milestone blocks.");
    while (!canceled() && (milestone < count) && query_.is_milestone(milestone))
    {
        ++milestone;
    }

    span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Top milestone block is [%1%] in [%2%] ms.") % sub1(milestone) % span.count());
    start = fine_clock::now();
    uint32_t tx{ one };

    logger("Find strong txs.");
    count = query_.tx_records();
    while (!canceled() && (tx < count) && query_.is_strong_tx(tx))
    {
        ++tx;
    }

    span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Top strong tx is [%1%] in [%2%] ms.") % sub1(tx) % span.count());
}

void executor::read_test(const hash_digest&) const
{
    const auto from = 481'824_u32;
    const auto top = 840'000_u32; ////query_.get_top_associated();
    const auto start = fine_clock::now();

    // segwit activation
    uint32_t block{ from };
    size_t total{};

    logger("Get all coinbases.");
    while (!canceled() && (block <= top))
    {
        const auto count = query_.get_tx_count(query_.to_candidate(block++));
        if (is_zero(count))
            return;

        total += system::ceilinged_log2(count);
    }

    const auto average = total / (top - from);
    const auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Total block depths [%1%] to [%2%] avg [%3%] in [%4%] ms.")
        % total % top % average % span.count());
}

void executor::read_test(const hash_digest&) const
{
    // Binance wallet address with 1,380,169 transaction count.
    // blockstream.info/address/bc1qm34lsc65zpw79lxes69zkqmk6ee3ewf0j77s3h
    const auto data = base16_array("0014dc6bf86354105de2fcd9868a2b0376d6731cb92f");
    const chain::script output_script{ data, false };
    const auto mnemonic = output_script.to_string(chain::flags::all_rules);
    logger(format("Getting payments to {%1%}.") % mnemonic);

    const auto start = fine_clock::now();
    database::output_links outputs{};
    if (!query_.to_address_outputs(outputs, output_script.hash()))
        return;

    const auto span = duration_cast<milliseconds>(fine_clock::now() - start);
    logger(format("Found [%1%] outputs of {%2%} in [%3%] ms.") %
        outputs.size() % mnemonic % span.count());
}

// This was caused by concurrent redundant downloads at tail following restart.
// The earlier transactions were marked as confirmed and during validation the
// most recent are found via point.hash association priot to to_block() test.
void executor::read_test(const hash_digest&) const
{
    const auto height = 839'287_size;
    const auto block = query_.to_confirmed(height);
    if (block.is_terminal())
    {
        logger("!block");
        return;
    }

    const auto txs = query_.to_transactions(block);
    if (txs.empty())
    {
        logger("!txs");
        return;
    }

    database::tx_link spender_link{};
    const auto hash_spender = system::base16_hash("1ff970ec310c000595929bd290bbc8f4603ee18b2b4e3239dfb072aaca012b28");
    for (auto position = zero; !canceled() && position < txs.size(); ++position)
    {
        const auto temp = txs.at(position);
        if (query_.get_tx_key(temp) == hash_spender)
        {
            spender_link = temp;
            break;
        }
    }

    auto spenders = store_.tx.it(hash_spender);
    if (spenders.get().is_terminal())
        return;

    // ...260, 261
    size_t spender_count{};
    do
    {
        const auto foo = spenders.get();
        ++spender_count;
    } while(spenders.advance());

    if (is_zero(spender_count))
    {
        logger("is_zero(spender_count)");
        return;
    }

    // ...260
    if (spender_link.is_terminal())
    {
        logger("spender_link.is_terminal()");
        return;
    }

    const auto spender_link1 = query_.to_tx(hash_spender);
    if (spender_link != spender_link1)
    {
        logger("spender_link != spender_link1");
        ////return;
    }

    database::tx_link spent_link{};
    const auto hash_spent = system::base16_hash("85f65b57b88b74fd945a66a6ba392a5f3c8a7c0f78c8397228dece885d788841");
    for (auto position = zero; !canceled() && position < txs.size(); ++position)
    {
        const auto temp = txs.at(position);
        if (query_.get_tx_key(temp) == hash_spent)
        {
            spent_link = temp;
            break;
        }
    }

    auto spent = store_.tx.it(hash_spent);
    if (spent.get().is_terminal())
        return;

    // ...255, 254
    size_t spent_count{};
    do
    {
        const auto bar = spent.get();
        ++spent_count;
    } while (spent.advance());

    if (is_zero(spent_count))
    {
        logger("is_zero(spent_count)");
        return;
    }

    // ...254 (not ...255)
    if (spent_link.is_terminal())
    {
        logger("spent_link.is_terminal()");
        return;
    }

    const auto spent_link1 = query_.to_tx(hash_spent);
    if (spent_link != spent_link1)
    {
        logger("spent_link != spent_link1");
        ////return;
    }

    const auto tx = query_.to_tx(hash_spender);
    if (tx.is_terminal())
    {
        logger("!tx");
        return;
    }

    if (tx != spender_link)
    {
        logger("tx != spender_link");
        return;
    }

    if (spender_link <= spent_link)
    {
        logger("spender_link <= spent_link");
        return;
    }

    // ...254
    const auto header1 = query_.to_block(spender_link);
    if (header1.is_terminal())
    {
        logger("header1.is_terminal()");
        return;
    }

    // ...255 (the latter instance is not confirmed)
    const auto header11 = query_.to_block(add1(spender_link));
    if (!header11.is_terminal())
    {
        logger("!header11.is_terminal()");
        return;
    }

    // ...260
    const auto header2 = query_.to_block(spent_link);
    if (header2.is_terminal())
    {
        logger("auto.is_terminal()");
        return;
    }

    // ...261 (the latter instance is not confirmed)
    const auto header22 = query_.to_block(add1(spent_link));
    if (!header22.is_terminal())
    {
        logger("!header22.is_terminal()");
        return;
    }

    if (header1 != header2)
    {
        logger("header1 != header2");
        return;
    }

    if (header1 != block)
    {
        logger("header1 != block");
        return;
    }

    const auto ec = query_.block_confirmable(query_.to_confirmed(height));
    logger(format("Confirm [%1%] test (%2%).") % height % ec.message());
}

void executor::read_test(const hash_digest&) const
{
    const auto bk_link = query_.to_candidate(804'001_size);
    const auto block = query_.get_block(bk_link);
    if (!block)
    {
        logger("!query_.get_block(link)");
        return;
    }

    ////const auto tx = query_.get_transaction({ 980'984'671_u32 });
    ////if (!tx)
    ////{
    ////    logger("!query_.get_transaction(tx_link)");
    ////    return;
    ////}
    ////
    ////chain::context ctx{};
    ////if (!query_.get_context(ctx, bk_link))
    ////{
    ////    logger("!query_.get_context(ctx, bk_link)");
    ////    return;
    ////}
    ////
    ////if (!query_.populate_with_metadata(*tx))
    ////{
    ////    logger("!query_.populate_with_metadata(*tx)");
    ////    return;
    ////}
    ////
    ////if (const auto ec = tx->confirm(ctx))
    ////    logger(format("Error confirming tx [980'984'671] %1%") % ec.message());
    ////
    ////// Does not compute spent metadata, assumes coinbase spent and others not.
    ////if (!query_.populate_with_metadata(*block))
    ////{
    ////    logger("!query_.populate_with_metadata(*block)");
    ////    return;
    ////}
    ////
    ////const auto& txs = *block->transactions_ptr();
    ////if (txs.empty())
    ////{
    ////    logger("txs.empty()");
    ////    return;
    ////}
    ////
    ////for (auto index = one; index < txs.size(); ++index)
    ////    if (const auto ec = txs.at(index)->confirm(ctx))
    ////        logger(format("Error confirming tx [%1%] %2%") % index % ec.message());
    ////
    ////logger("Confirm test 1 complete.");

    const auto ec = query_.block_confirmable(bk_link);
    logger(format("Confirm test 2 complete (%1%).") % ec.message());
}

void executor::read_test(const hash_digest&) const
{
    using namespace database;
    constexpr auto frequency = 100'000u;
    const auto start = fine_clock::now();
    auto tx = 664'400'000_size;

    // Read all data except genesis (ie. for validation).
    while (!canceled() && (++tx < query_.tx_records()))
    {
        const tx_link link{
            system::possible_narrow_cast<tx_link::integer>(tx) };

        ////const auto ptr = query_.get_header(link);
        ////if (!ptr)
        ////{
        ////    logger("Failure: get_header");
        ////    break;
        ////}
        ////else if (is_zero(ptr->bits()))
        ////{
        ////    logger("Failure: zero bits");
        ////    break;
        ////}

        ////const auto txs = query_.to_transactions(link);
        ////if (txs.empty())
        ////{
        ////    logger("Failure: to_txs");
        ////    break;
        ////}

        const auto ptr = query_.get_transaction(link);
        if (!ptr)
        {
            logger("Failure: get_transaction");
            break;
        }
        else if (!ptr->is_valid())
        {
            logger("Failure: is_valid");
            break;
        }

        if (is_zero(tx % frequency))
            logger(format("get_transaction" BS_READ_ROW) % tx %
                duration_cast<seconds>(fine_clock::now() - start).count());
    }

    if (canceled())
        logger(BS_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("get_transaction" BS_READ_ROW) % tx % span.count());
}

void executor::read_test(const hash_digest&) const
{
    constexpr auto hash492224 = base16_hash(
        "0000000000000000003277b639e56dffe2b4e60d18aeedb1fe8b7e4256b2a526");

    logger("HIT <enter> TO START");
    std::string line{};
    std::getline(input_, line);
    const auto start = fine_clock::now();

    for (size_t height = 492'224; (height <= 492'224) && !canceled(); ++height)
    {
        // 2s 0s
        const auto link = query_.to_header(hash492224);
        if (link.is_terminal())
        {
            logger("to_header");
            return;
        }

        ////const auto link = query_.to_confirmed(height);
        ////if (link.is_terminal())
        ////{
        ////    logger("to_confirmed");
        ////    return;
        ////}

        // 109s 111s
        const auto block = query_.get_block(link);
        if (!block || !block->is_valid() || block->hash() != hash492224)
        {
            logger("get_block");
            return;
        }
        
        // 125s 125s
        code ec{};
        if ((ec = block->check()))
        {
            logger(format("Block [%1%] check1: %2%") % height % ec.message());
            return;
        }

        // 117s 122s
        if (chain::checkpoint::is_conflict(
            metadata_.configured.bitcoin.checkpoints, block->hash(), height))
        {
            logger(format("Block [%1%] checkpoint conflict") % height);
            return;
        }

        ////// ???? 125s/128s
        ////block->populate();

        // 191s 215s/212s/208s [independent]
        // ???? 228s/219s/200s [combined]
        if (!query_.populate_with_metadata(*block))
        {
            logger("populate");
            return;
        }

        // 182s
        database::context ctx{};
        if (!query_.get_context(ctx, link) || ctx.height != height)
        {
            logger("get_context");
            return;
        }

        // Fabricate chain_state context from store context.
        chain::context state{};
        state.flags = ctx.flags;
        state.height = ctx.height;
        state.median_time_past = ctx.mtp;
        state.timestamp = block->header().timestamp();

        // split from accept.
        if ((ec = block->check(state)))
        {
            logger(format("Block [%1%] check2: %2%") % height % ec.message());
            return;
        }

        // 199s
        const auto& coin = metadata_.configured.bitcoin;
        if ((ec = block->accept(state, coin.subsidy_interval_blocks,
            coin.initial_subsidy())))
        {
            logger(format("Block [%1%] accept: %2%") % height % ec.message());
            return;
        }

        // 1410s
        if ((ec = block->connect(state)))
        {
            logger(format("Block [%1%] connect: %2%") % height % ec.message());
            return;
        }

        ////for (size_t index = one; index < block->transactions_ptr()->size(); ++index)
        ////{
        ////    const auto& tx = *block->transactions_ptr()->at(index);
        ////    if ((ec = tx.connect(state)))
        ////    {
        ////        logger(format("Tx (%1%) [%2%] %3%")
        ////            % index
        ////            % encode_hash(tx.hash(false))
        ////            % ec.message());
        ////    }
        ////}

        // +10s for all.
        logger(format("block:%1%") % height);
        ////logger(format("block:%1% flags:%2% mtp:%3%") %
        ////    ctx.height % ctx.flags % ctx.mtp);
    }

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("STOP (%1% secs)") % span.count());
}

// TODO: create a block/tx dumper.
void executor::read_test(const hash_digest&) const
{
    constexpr auto link = 600'000_size;
    const auto start = logger::now();

    const auto height = query_.get_height(link);
    if (height.is_terminal())
    {
        logger("height.is_terminal()");
        return;
    }

    if (height != link)
    {
        logger("height != link");
        return;
    }

    code ec{};
    if ((ec = query_.block_confirmable(link)))
    {
        logger(format("query_.block_confirmable: %1%") % ec.message());
        return;
    }

    const auto block = query_.get_block(link);
    if (!block)
    {
        logger("!block");
        return;
    }

    if (!block->is_valid())
    {
        logger("!block->is_valid()");
        return;
    }

    database::context ctx{};
    if (!query_.get_context(ctx, link))
    {
        logger("!query_.get_context(ctx, link)");
        return;
    }

    logger(format("flags:%1% height:%2% mtp:%3%") %
        ctx.flags % ctx.height % ctx.mtp);

    // minimum_block_version and work_required are only for header validate.
    chain::context state{};
    state.flags = ctx.flags;
    state.height = ctx.height;
    state.median_time_past = ctx.mtp;
    state.timestamp = block->header().timestamp();
    state.minimum_block_version = 0;
    state.work_required = 0;

    block->populate();

    if (!query_.populate_with_metadata(*block))
    {
        logger("!query_.populate(*block)");
        return;
    }

    if ((ec = block->check()))
    {
        logger(format("Block check(): %1%") % ec.message());
        return;
    }

    if ((ec = block->check(state)))
    {
        logger(format("Block check(state): %1%") % ec.message());
        return;
    }

    if ((ec = block->accept(state,
        metadata_.configured.bitcoin.subsidy_interval_blocks,
        metadata_.configured.bitcoin.initial_subsidy())))
    {
        logger(format("Block accept(state): %1%") % ec.message());
        return;
    }

    if ((ec = block->connect(state)))
    {
        logger(format("Block connect: %1%") % ec.message());
        return;
    }

    logger(format("segregated [%1%]") % to_int(block->is_segregated()));
    logger(format("segregated count [%1%]") % block->segregated());

    const auto span = duration_cast<milliseconds>(logger::now() - start);
    logger(format("Validated block [%1%] in %2% msec.") % link % span.count());

    ////constexpr auto tx_hash = base16_hash(
    ////    "eb2179db6c40bceb02cebcc5c99cf783ed6385b00767c7a5419fe530eaba8bff");
    ////const auto tx_link = query_.to_tx(tx_hash);
    ////const auto tx = query_.get_transaction(tx_link);
    ////const auto result = query_.populate_without_metadata(*tx);
    ////const auto size = tx->serialized_size(false);
    ////const auto weight = tx->serialized_size(true);
    ////const auto version = tx->version();
    ////const auto locktime = tx->locktime();
    ////const auto fee = tx->fee();
    ////const auto sequence = tx->inputs_ptr()->at(0)->sequence();
    ////const auto& in = tx->inputs_ptr()->at(0)->script();
    ////const auto& out = tx->inputs_ptr()->at(0)->prevout->script();
    ////const auto connect = tx->connect(state);

    ////logger(format("tx_hash  [%1%]") % encode_hash(tx_hash));
    ////logger(format("tx_link  [%1%]") % tx_link.value);
    ////logger(format("result   [%1%]") % to_int(result));
    ////logger(format("size     [%1%]") % size);
    ////logger(format("weight   [%1%]") % weight);
    ////logger(format("version  [%1%]") % version);
    ////logger(format("locktime [%1%]") % locktime);
    ////logger(format("fee      [%1%]") % fee);
    ////logger(format("sequence [%1%]") % sequence);
    ////logger(format("in       [%1%]") % in.to_string(chain::flags::no_rules));
    ////logger(format("out      [%1%]") % out.to_string(chain::flags::no_rules));
    ////logger(format("connect  [%1%]") % connect.message());
}

#endif // UNDEFINED

} // namespace server
} // namespace libbitcoin
