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
LRESULT CALLBACK executor::window_proc(HWND handle, UINT message,
    WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        // Reject session close until process completion, initiate stop, and
        // provide reason text that the operating system may show to the user.
        case WM_QUERYENDSESSION:
        {
            ::ShutdownBlockReasonCreate(handle, window_text);
            executor::handle_stop(possible_narrow_sign_cast<int>(message));
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
        const auto instance = ::GetModuleHandleW(NULL);
        const WNDCLASSEXW window_class
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = &executor::window_proc,
            .hInstance = instance,
            .hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(101)),
            .lpszClassName = window_name,
            .hIconSm = ::LoadIconW(instance, MAKEINTRESOURCEW(101))
        };

        // fault
        if (is_zero(::RegisterClassExW(&window_class)))
            return;

        // Zero sizing results in title bar only.
        // WS_EX_NOACTIVATE: prevents focus-stealing.
        // WS_VISIBLE: required to capture WM_QUERYENDSESSION.
        window_ = ::CreateWindowExW
        (
            WS_EX_NOACTIVATE,
            window_name,
            window_title,
            WS_VISIBLE,
            0, 0, 0, 0,
            NULL,
            NULL,
            ::GetModuleHandleW(NULL),
            NULL);

        // fault
        if (is_null(window_))
            return;

        MSG message{};
        BOOL result{};
        ready_.set_value(true);
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
    // Wait until window is accepting messages, so WM_QUIT isn't missed.
    ready_.get_future().wait();

    if (!is_null(window_))
        ::PostMessageW(window_, WM_QUIT, 0, 0);

    if (thread_.joinable())
        thread_.join();

    if (!is_null(window_))
    {
        ::DestroyWindow(window_);
        window_ = NULL;
    }

    const auto handle = ::GetConsoleWindow();
    if (!is_null(handle))
        ::ShutdownBlockReasonDestroy(handle);
}

#endif // HAVE_MSC

} // namespace server
} // namespace libbitcoin
