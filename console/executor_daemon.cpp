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

// TODO: register with the service control manager (CreateServiceW/DeleteService).
// TODO: install a systemd unit (linux) and launchd plist (osx).

// --[d]aemon [--[u]ser]
bool executor::do_daemon()
{
    log_.stop();

    // The methods element of a credential does not apply to a service logon.
    if (metadata_.configured.user.has_value() &&
        !metadata_.configured.user.value().methods().empty())
    {
        logger(BS_DAEMON_INVALID_USER);
        return false;
    }

#if defined(HAVE_MSC)
    logger(metadata_.configured.daemon.value() ? BS_DAEMON_INSTALL :
        BS_DAEMON_UNINSTALL);
#else
    logger(BS_DAEMON_UNSUPPORTED);
#endif
    return false;
}

} // namespace server
} // namespace libbitcoin
