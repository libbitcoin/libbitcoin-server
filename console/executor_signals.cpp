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

#include <csignal>

namespace libbitcoin {
namespace server {

// TODO: use RegisterServiceCtrlHandlerEx for service registration.
    
#if defined(HAVE_MSC)
BOOL WINAPI executor::control_handler(DWORD signal)
{
    switch (signal)
    {
        // Keyboard events. These prevent exit altogether when TRUE returned.
        // handle_stop(signal) therefore shuts down gracefully/completely.
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:

        // A signal that the system sends to all processes attached to a
        // console when the user closes the console (by clicking Close on the
        // console window's window menu). Returning TRUE here does not
        // materially delay exit, so aside from capture this is a noop.
        case CTRL_CLOSE_EVENT:
        {
            const auto value = system::possible_narrow_sign_cast<int>(signal);
            executor::handle_stop(value);
            return TRUE;
        }

        ////// Only services receive this (*any* user is logging off).
        ////case CTRL_LOGOFF_EVENT:
        ////// Only services receive this (all users already logged off).
        ////case CTRL_SHUTDOWN_EVENT:
        default:
            return FALSE;
    }
}
#endif

// static
void executor::set_signal_handlers()
{
#if defined(HAVE_MSC)
    ::SetConsoleCtrlHandler(&executor::control_handler, TRUE);
#else
    // struct keywork avoids name conflict with posix function sigaction.
    struct sigaction action{};

    // Restart interrupted system calls.
    action.sa_flags = SA_RESTART;

    // sa_handler is actually a macro :o
    action.sa_handler = handle_stop;

    // Set masking.
    sigemptyset(&action.sa_mask);

    // Block during handling to prevent reentrancy.
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaddset(&action.sa_mask, SIGHUP);
    sigaddset(&action.sa_mask, SIGUSR2);

    sigaction(SIGINT,  &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGHUP,  &action, nullptr);
    sigaction(SIGUSR2, &action, nullptr);

#if defined(HAVE_LINUX)
    sigaction(SIGPWR,  &action, nullptr);
#endif
#endif
}

} // namespace server
} // namespace libbitcoin
