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

#include <future>
#include <optional>
#include <thread>
#include "localize.hpp"

namespace libbitcoin {
namespace server {

#if defined(HAVE_MSC)
LRESULT CALLBACK executor::window_proc(HWND handle, UINT message,
    WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        // Reject session close until process completion, initiate stop, and
        // provide reason text that the operating system may show to the user.
        case WM_QUERYENDSESSION:
        {
            ::ShutdownBlockReasonCreate(handle, BS_WINDOW_TEXT);
            const auto value = system::possible_narrow_sign_cast<int>(message);
            executor::handle_stop(value);
            return FALSE;
        }
        default:
        {
            return ::DefWindowProcW(handle, message, wparam, lparam);
        }
    }
}
#endif

// static initializers.
#if defined(HAVE_MSC)
HWND executor::window_handle_{};
std::promise<bool> executor::window_ready_{};
std::optional<std::thread> executor::window_thread_{};
#endif

// static
void executor::create_hidden_window()
{
#if defined(HAVE_MSC)
    static constexpr auto window_name = L"HiddenShutdownWindow";

    window_thread_.emplace(std::thread([]()
    {
        const auto instance = ::GetModuleHandleW(NULL);
        const WNDCLASSEXW window_class
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = &window_proc,
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
        window_handle_ = ::CreateWindowExW
        (
            WS_EX_NOACTIVATE,
            window_name,
            BS_WINDOW_TITLE,
            WS_VISIBLE,
            0, 0, 0, 0,
            NULL,
            NULL,
            ::GetModuleHandleW(NULL),
            NULL);

        // fault
        if (is_null(window_handle_))
            return;

        MSG message{};
        BOOL result{};
        window_ready_.set_value(true);
        while (!is_zero(result = ::GetMessageW(&message, NULL, 0, 0)))
        {
            // fault
            if (system::is_negative(result))
                return;

            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }));
#endif
}

void executor::destroy_hidden_window()
{
#if defined(HAVE_MSC)
    // Wait until window is accepting messages, so WM_QUIT isn't missed.
    window_ready_.get_future().wait();

    if (!is_null(window_handle_))
        ::PostMessageW(window_handle_, WM_QUIT, 0, 0);

    if (window_thread_.has_value() && window_thread_.value().joinable())
    {
        window_thread_.value().join();
        window_thread_.reset();
    }

    if (!is_null(window_handle_))
    {
        ::DestroyWindow(window_handle_);
        window_handle_ = NULL;
    }

    const auto handle = ::GetConsoleWindow();
    if (!is_null(handle))
        ::ShutdownBlockReasonDestroy(handle);
#endif
}

} // namespace server
} // namespace libbitcoin
