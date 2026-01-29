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

namespace libbitcoin {
namespace server {

#if defined(HAVE_MSC)

// TODO: use RegisterServiceCtrlHandlerEx for service registration.

using namespace system;

constexpr auto window_name = L"HiddenShutdownWindow";
constexpr auto window_text = L"Flushing tables...";
constexpr auto window_title = L"Libbitcoin Server";

// static
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
            executor::handle_stop(possible_narrow_sign_cast<int>(signal));
            return TRUE;

        ////// Only services receive this (*any* user is logging off).
        ////case CTRL_LOGOFF_EVENT:
        ////// Only services receive this (all users already logged off).
        ////case CTRL_SHUTDOWN_EVENT:
        default:
            return FALSE;
    }
}

// static
LRESULT CALLBACK executor::window_proc(HWND handle, UINT message, WPARAM wparam,
    LPARAM lparam)
{
    switch (message)
    {
        // Reject session close until process completion, initiate stop, and
        // provide reason text that the operating system may show to the user.
        case WM_QUERYENDSESSION:
        {
            ::ShutdownBlockReasonCreate(handle, window_text);
            stop(possible_narrow_sign_cast<int>(message));
            return FALSE;
        }
        default:
        {
            return ::DefWindowProcW(handle, message, wparam, lparam);
        }
    }
}

void executor::create_hidden_window()
{
    thread_ = std::thread([this]()
    {
        const WNDCLASSW window_class
        {
            .lpfnWndProc = &executor::window_proc,
            .hInstance = ::GetModuleHandleW(NULL),
            .lpszClassName = window_name
        };

        // fault
        if (is_zero(::RegisterClassW(&window_class)))
            return;

        // Window must be visible (?) to capture shutdown.
        window_ = ::CreateWindowExW
        (
            0u,
            window_name,
            window_title,
            WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            400,
            300,
            NULL,
            NULL,
            ::GetModuleHandleW(NULL),
            NULL);

        // fault
        if (is_null(window_))
            return;

        MSG message{};
        BOOL result{};
        while (!is_zero(result = ::GetMessageW(&message, NULL, 0, 0)))
        {
            // fault
            if (is_negative(result))
                return;

            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    });
}

void executor::destroy_hidden_window()
{
    if (!is_null(window_))
        ::PostMessageW(window_, WM_QUIT, 0, 0);

    if (thread_.joinable())
        thread_.join();

    if (!is_null(window_))
    {
        ::DestroyWindow(window_);
        window_ = nullptr;
    }

    const HWND handle = ::GetConsoleWindow();
    if (!is_null(handle))
        ::ShutdownBlockReasonDestroy(handle);
}

#endif // HAVE_MSC

} // namespace server
} // namespace libbitcoin
