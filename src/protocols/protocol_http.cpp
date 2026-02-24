/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/protocols/protocol_http.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace network::http;

// Cache request for serialization, keeping it out of dispatch.
void protocol_http::set_request(const request_cptr& request) NOEXCEPT
{
    ////BC_ASSERT(stranded());
    BC_ASSERT(request);
    request_ = request;
}

// Returns default if not set, for safety (asserts correctness).
request_cptr protocol_http::reset_request() NOEXCEPT
{
    ////BC_ASSERT(stranded());
    BC_ASSERT(request_);

    if (request_)
    {
        auto copy = request_;
        request_.reset();
        return copy;
    }

    return system::to_shared<request>();
}

} // namespace server
} // namespace libbitcoin
