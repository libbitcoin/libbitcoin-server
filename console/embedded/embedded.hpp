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
#ifndef LIBBITCOIN_BS_EMBEDDED_HPP
#define LIBBITCOIN_BS_EMBEDDED_HPP

#include "../executor.hpp"

#include <iterator>
#include <bitcoin/server.hpp>

#define DECLARE_EMBEDDED_PAGE(page) \
span_value page() const NOEXCEPT override

#define DECLARE_EMBEDDED_PAGES(container) \
struct container : public server::settings::embedded_pages \
{ \
    DECLARE_EMBEDDED_PAGE(css); \
    DECLARE_EMBEDDED_PAGE(html); \
    DECLARE_EMBEDDED_PAGE(ecma); \
    DECLARE_EMBEDDED_PAGE(font); \
    DECLARE_EMBEDDED_PAGE(icon); \
}

#define DEFINE_EMBEDDED_PAGE(container, type, name, ...) \
span_value container::name() const NOEXCEPT \
{ \
    static constexpr type name##_[] = __VA_ARGS__; \
    static const span_value out \
    ( \
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&name##_[0])), \
        literal_length(name##_) \
    ); \
    return out; \
}

namespace libbitcoin {
namespace server {

DECLARE_EMBEDDED_PAGES(admin_pages);
DECLARE_EMBEDDED_PAGES(native_pages);

} // namespace server
} // namespace libbitcoin

#endif
