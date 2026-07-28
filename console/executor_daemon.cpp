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
#include "executor.hpp"
#include "localize.hpp"

namespace libbitcoin {
namespace server {

// TODO: register with the service control manager (CreateServiceW).
// TODO: install a systemd unit (linux) and launchd plist (osx).

// --[d]aemon
bool executor::do_daemon()
{
    log_.stop();
#if defined(HAVE_MSC)
    logger(BS_DAEMON_UNIMPLEMENTED);
#else
    logger(BS_DAEMON_UNSUPPORTED);
#endif
    return false;
}

} // namespace server
} // namespace libbitcoin
