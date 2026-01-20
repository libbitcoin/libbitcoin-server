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

// arbitrary testing (non-const).
void executor::write_test(const hash_digest&)
{
    logger("No write test implemented.");
}

#if defined(UNDEFINED)

void executor::write_test(const system::hash_digest&)
{
    for (database::header_link link{ 793'008_u32 }; link < 885'000_u32; ++link)
    {
        if (!query_.set_block_unknown(link))
        {
            logger(format("set_block_unknown fault [%1%].") % link.value);
            return;
        }
    }

    logger(format("set_block_unknown complete."));
}

void executor::write_test(const system::hash_digest&)
{
    code ec{};
    size_t count{};
    const auto start = fine_clock::now();

    const auto fork = query_.get_fork();
    const auto top_associated = query_.get_top_associated_from(fork);

    for (auto height = fork; !cancel_ && !ec && height <= top_associated;
        ++height, ++count)
    {
        const auto block = query_.to_candidate(height);
        if (!query_.set_strong(block))
        {
            logger(format("set_strong [%1%] fault.") % height);
            return;
        }

        ////if (ec = query_.block_confirmable(block))
        ////{
        ////    logger(format("block_confirmable [%1%] fault (%2%).") % height % ec.message());
        ////    return;
        ////}
        ////
        ////if (!query_.set_block_confirmable(block))
        ////{
        ////    logger(format("set_block_confirmable [%1%] fault.") % height);
        ////    return;
        ////}

        if (!query_.push_confirmed(block, true))
        {
            logger(format("push_confirmed [%1%] fault.") % height);
            return;
        }

        if (is_zero(height % 1000_size))
            logger(format("write_test [%1%].") % height);
    }

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("Set confirmation of %1% blocks in %2% secs.") % count %
        span.count());
}

void executor::write_test(const system::hash_digest&)
{
    using namespace database;
    constexpr auto frequency = 10'000;
    const auto start = fine_clock::now();
    logger(BS_OPERATION_INTERRUPT);

    auto height = query_.get_top_candidate();
    while (!cancel_ && (++height < query_.header_records()))
    {
        // Assumes height is header link.
        const header_link link{ possible_narrow_cast<header_link::integer>(height) };

        if (!query_.push_confirmed(link, true))
        {
            logger("!query_.push_confirmed(link)");
            return;
        }

        if (!query_.push_candidate(link))
        {
            logger("!query_.push_candidate(link)");
            return;
        }

        if (is_zero(height % frequency))
            logger(format("block" BS_WRITE_ROW) % height %
                duration_cast<seconds>(fine_clock::now() - start).count());
    }
            
    if (cancel_)
        logger(BS_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("block" BS_WRITE_ROW) % height % span.count());
}

void executor::write_test(const system::hash_digest&)
{
    using namespace database;
    ////constexpr uint64_t fees = 99;
    constexpr auto frequency = 10'000;
    const auto start = fine_clock::now();
    code ec{};

    logger(BS_OPERATION_INTERRUPT);

    auto height = zero;//// query_.get_top_confirmed();
    const auto records = query_.header_records();
    while (!cancel_ && (++height < records))
    {
        // Assumes height is header link.
        const header_link link{ possible_narrow_cast<header_link::integer>(height) };

        if (!query_.set_strong(link))
        {
            // total sequential chain cost: 18.7 min (now 6.6).
            logger("Failure: set_strong");
            break;
        }
        else if ((ec = query_.block_confirmable(link)))
        {
            // must set_strong before each (no push, verifies non-use).
            logger(format("Failure: block_confirmed, %1%") % ec.message());
            break;
        }
        ////if (!query_.set_txs_connected(link))
        ////{
        ////    // total sequential chain cost: 21 min.
        ////    logger("Failure: set_txs_connected");
        ////    break;
        ////}
        ////if (!query_.set_block_confirmable(link, fees))
        ////{
        ////    // total chain cost: 1 sec.
        ////    logger("Failure: set_block_confirmable");
        ////    break;
        ////    break;
        ////}
        ////else if (!query_.push_candidate(link))
        ////{
        ////    // total chain cost: 1 sec.
        ////    logger("Failure: push_candidate");
        ////    break;
        ////}
        ////else if (!query_.push_confirmed(link))
        ////{
        ////    // total chain cost: 1 sec.
        ////    logger("Failure: push_confirmed");
        ////    break;
        ////}

        if (is_zero(height % frequency))
            logger(format("block" BS_WRITE_ROW) % height %
                duration_cast<seconds>(fine_clock::now() - start).count());
    }
    
    if (cancel_)
        logger(BS_OPERATION_CANCELED);

    const auto span = duration_cast<seconds>(fine_clock::now() - start);
    logger(format("block" BS_WRITE_ROW) % height % span.count());
}


void executor::write_test(const system::hash_digest& hash)
{
    const auto id = encode_hash(hash);
    const auto link = query_.to_header(hash);
    if (link.is_terminal())
    {
        logger(format("Block [%1%] not found.") % id);
    }
    else if (query_.set_block_unknown(link))
    {
        logger(format("Successfully reset block [%1%].") % id);
    }
    else
    {
        logger(format("Failed to reset block [%1%].") % id);
    }
}

#endif // UNDEFINED

} // namespace server
} // namespace libbitcoin
