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
#include "executor.hpp"
#include "localize.hpp"

namespace libbitcoin {
namespace server {

// Linux paging info dump when less than configured.
// ----------------------------------------------------------------------------

void executor::dump_paging() const
{
#if !defined(HAVE_LINUX)
    using namespace network::config;
    const auto back_bytes   = get_memory_setting("dirty_background_bytes");
    const auto fore_bytes   = get_memory_setting("dirty_bytes");
    const auto back_ratio   = get_memory_setting("dirty_background_ratio");
    const auto fore_ratio   = get_memory_setting("dirty_ratio");
    const auto expire_csecs = get_memory_setting("dirty_expire_centisecs");
    const auto write_csecs  = get_memory_setting("dirty_writeback_centisecs");

    // Don't dump if did not read any one paramter.
    if (!(back_bytes && fore_bytes && back_ratio && fore_ratio &&
        expire_csecs && write_csecs))
        return;

    const auto& node = metadata_.configured.node;
    const auto ratio = is_zero(fore_bytes.value()) && is_zero(back_bytes.value());
    const auto back_ok = back_ratio.value() >= node.warn_dirty_background_ratio;
    const auto fore_ok = fore_ratio.value() >= node.warn_dirty_ratio;
    const auto require = to_bool(node.warn_dirty_ratio) &&
        to_bool(node.warn_dirty_background_ratio);

    // Don't dump if machine is in ratio mode and above both confgured ratios.
    if (ratio && fore_ok && back_ok && require)
        return;

    logger(boost::format(BS_PAGING_TABLE)
        % (ratio ? "ratio" : "bytes")
        % fore_bytes.value()
        % fore_ratio.value()
        % back_bytes.value()
        % back_ratio.value()
        % expire_csecs.value()
        % write_csecs.value());
#endif
}

} // namespace server
} // namespace libbitcoin
