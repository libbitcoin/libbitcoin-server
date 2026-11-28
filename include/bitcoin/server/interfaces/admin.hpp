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
#ifndef LIBBITCOIN_SERVER_INTERFACES_ADMIN_HPP
#define LIBBITCOIN_SERVER_INTERFACES_ADMIN_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/types.hpp>

namespace libbitcoin {
namespace server {
namespace interface {

struct admin_methods
{
    static constexpr std::tuple methods
    {
        method<"log_subscribe", uint8_t, optional<0_u64>>{ "version", "filter" },
        method<"event_subscribe", uint8_t, optional<0_u64>>{ "version", "filter" },
        method<"mining", uint8_t>{ "version" },
        method<"mining_subscribe", uint8_t>{ "version" }
    };

    template <typename... Args>
    using subscriber = network::subscriber<Args...>;

    template <size_t Index>
    using at = method_at<methods, Index>;

    // Derive this from above in c++26 using reflection.

    using log_subscribe = at<0>;
    using event_subscribe = at<1>;
    using mining = at<2>;
    using mining_subscribe = at<3>;
};

/// ?format=json|html (via query string).
/// ---------------------------------------------------------------------------

/// Filter parameter is a decimal integer bit mask, where bit position
/// corresponds to log level/event enum value. Zero matches nothing, clearing
/// subscription; omitted filter defaults to zero. Response is
/// {"previous":[filter]}, previous filter value as json number (masks are
/// constrained to exact json numbers).

/// /v1/log/subscribe?filter=[mask] {stream}
/// /v1/event/subscribe?filter=[mask] {stream}

/// The mining channels of the stratum services and the blocks they found.
/// Subscription responds with the same and pushes it periodically and as
/// channels open and close.

/// /v1/mining
/// /v1/mining/subscribe {stream}

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif