/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/server/parsers/bitcoind_script.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace system::chain;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

code output_script(script& out, const std::string& text, uint8_t p2kh,
    uint8_t p2sh, const std::string& witness) NOEXCEPT
{
    using namespace wallet;

    if (const payment_address payment{ text }; payment)
    {
        out = payment.output_script(p2kh, p2sh);
        return error::success;
    }

    // The parse accepts any prefix, so the configured one is a check.
    if (const witness_address payment{ text };
        payment && payment.prefix() == witness)
    {
        out = payment.script();
        return error::success;
    }

    return error::invalid_argument;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
