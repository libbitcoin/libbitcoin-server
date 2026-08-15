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
#include <bitcoin/server/protocols/protocol_http.hpp>

#include <array>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_http

using namespace network::http;
using namespace std::placeholders;

// Shared pointers required in handler parameters so closures control lifetime.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)

// Serialization.
// ----------------------------------------------------------------------------

// The measure of an unmaterialized json body is a second serialization, as
// beast requires the length before the body is written. Serializing once into
// the buffer that is written obtains the same length from the bytes that carry
// it, and releases the model before the write.
bool protocol_http::materialize(std::string& text,
    const boost::json::value& model, size_t size_hint) NOEXCEPT
{
    // The hint reserves the buffer, so a sufficient hint does not reallocate.
    // The scratch is a serialization window, not a bound on the body.
    constexpr size_t window = 4096;
    std::string out{};

    try
    {
        // Reserved within the guard, as a hint that exceeds max_size or
        // cannot be allocated throws, which must not escape.
        out.reserve(size_hint);

        std::array<char, window> scratch{};
        boost::json::serializer serializer{ model.storage() };
        serializer.reset(&model);

        while (!serializer.done())
        {
            const auto view = serializer.read(scratch.data(), scratch.size());

            // No progress (edge case), as guarded by the json body writer.
            if (view.empty())
                return false;

            out.append(view.data(), view.size());
        }
    }
    catch (...)
    {
        return false;
    }

    text = std::move(out);
    return true;
}

// Websocket dispatch.
// ----------------------------------------------------------------------------

void protocol_http::handle_receive_unknown(const code& ec,
    const method::unknown::cptr& unknown) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    dispatch_websocket(*unknown);
}

void protocol_http::dispatch_websocket(const request& ) NOEXCEPT
{
    BC_ASSERT(stranded());
    stop(error::not_implemented);
}

// Caching for http response abstraction.
// ----------------------------------------------------------------------------

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

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
