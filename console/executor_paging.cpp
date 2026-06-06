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

#if defined(HAVE_LINUX)
#include <charconv>
#include <fcntl.h>
#include <unistd.h>

namespace {

// Read a single decimal integer from a /proc/sys/vm sysctl entry.
std::optional<uint64_t> read_vm_param(const char* path) noexcept
{
    char buffer[24]{};
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0)
        return std::nullopt;

    const auto count = ::read(fd, buffer, sizeof(buffer) - 1u);
    ::close(fd);

    if (count <= 0)
        return std::nullopt;

    uint64_t value{};
    const auto [ptr, ec] = std::from_chars(buffer, buffer + count, value);
    if (ec != std::errc{})
        return std::nullopt;

    return value;
}

} // namespace
#endif

namespace libbitcoin {
namespace server {

using boost::format;

// Paging dump.
// ----------------------------------------------------------------------------

void executor::dump_paging() const
{
#if defined(HAVE_LINUX)
    const auto dirty_background_bytes    = read_vm_param("/proc/sys/vm/dirty_background_bytes");
    const auto dirty_bytes               = read_vm_param("/proc/sys/vm/dirty_bytes");
    const auto dirty_background_ratio    = read_vm_param("/proc/sys/vm/dirty_background_ratio");
    const auto dirty_ratio               = read_vm_param("/proc/sys/vm/dirty_ratio");
    const auto dirty_expire_centisecs    = read_vm_param("/proc/sys/vm/dirty_expire_centisecs");
    const auto dirty_writeback_centisecs = read_vm_param("/proc/sys/vm/dirty_writeback_centisecs");

    if (!dirty_background_bytes || !dirty_bytes || !dirty_background_ratio ||
        !dirty_ratio || !dirty_expire_centisecs || !dirty_writeback_centisecs)
        return;

    const auto& log = metadata_.configured.log;
    const auto bytes_mode = (*dirty_bytes != 0u) || (*dirty_background_bytes != 0u);
    if (!bytes_mode)
    {
        if (log.dirty_ratio_minimum != 0u &&
            *dirty_ratio >= log.dirty_ratio_minimum &&
            log.dirty_background_ratio_minimum != 0u &&
            *dirty_background_ratio >= log.dirty_background_ratio_minimum)
            return;
    }

    logger(format(BS_PAGING_TABLE)
        % (bytes_mode ? "bytes" : "ratio")
        % *dirty_bytes
        % *dirty_ratio
        % *dirty_background_bytes
        % *dirty_background_ratio
        % *dirty_expire_centisecs
        % *dirty_writeback_centisecs);
#endif
}

} // namespace server
} // namespace libbitcoin
