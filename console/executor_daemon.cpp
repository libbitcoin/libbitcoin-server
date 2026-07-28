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

#include <atomic>
#include <filesystem>
#include <iostream>
#if defined(HAVE_MSC)
    #include <ntsecapi.h>
#endif
#include "localize.hpp"

namespace libbitcoin {
namespace server {

using format = boost_format;

// TODO: register a systemd unit (linux) and launchd plist (osx), and notify
// TODO: readiness/stopping via sd_notify, in place of the manager below.

bool executor::service_{ false };
std::atomic_bool executor::exited_{ false };

#if defined(HAVE_MSC)

// Node drain exceeds the default shutdown window, so preshutdown is required.
// This has no effect until the service accepts SERVICE_ACCEPT_PRESHUTDOWN.
constexpr DWORD preshutdown_milliseconds = 600'000;

// The manager terminates a pending start that does not progress within this.
constexpr DWORD pending_milliseconds = 30'000;

std::atomic<DWORD> executor::state_{ SERVICE_START_PENDING };
std::atomic_bool executor::failed_{ false };
SERVICE_STATUS_HANDLE executor::status_handle_{};
parser* executor::service_metadata_{};
std::istream* executor::service_input_{};
std::ostream* executor::service_error_{};

// Service runtime.
// ----------------------------------------------------------------------------

// A null status handle implies console mode, in which this is a noop. Pending
// states must advance the checkpoint, as the manager otherwise assumes a hang.
// Reported from the manager, network, and poller threads, so holds no state
// apart from the atomics that the reported status is composed from.
void executor::report_status(DWORD state) NOEXCEPT
{
    if (is_null(status_handle_))
        return;

    static std::atomic<DWORD> checkpoint{};

    const auto starting = (state == SERVICE_START_PENDING);
    const auto stopping = (state == SERVICE_STOP_PENDING);
    const auto failed = failed_.load();
    const DWORD code = failed ? ERROR_SERVICE_SPECIFIC_ERROR : NO_ERROR;
    const DWORD specific = failed ? 1u : 0u;
    state_.store(state);

    SERVICE_STATUS status
    {
        .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
        .dwCurrentState = state,
        .dwControlsAccepted = (state == SERVICE_RUNNING) ?
            (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN) : 0u,
        .dwWin32ExitCode = code,
        .dwServiceSpecificExitCode = specific,
        .dwCheckPoint = (starting || stopping) ? ++checkpoint : 0u,
        .dwWaitHint = starting ? pending_milliseconds :
            (stopping ? preshutdown_milliseconds : 0u)
    };

    ::SetServiceStatus(status_handle_, &status);
}

// Every stop source converges on the one signal-safe latch.
DWORD WINAPI executor::service_handler(DWORD control, DWORD, LPVOID,
    LPVOID) NOEXCEPT
{
    switch (control)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_PRESHUTDOWN:
        {
            report_status(SERVICE_STOP_PENDING);
            handle_stop(system::possible_narrow_sign_cast<int>(control));
            return NO_ERROR;
        }
        case SERVICE_CONTROL_INTERROGATE:
        {
            report_status(state_.load());
            return NO_ERROR;
        }
        default:
        {
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
    }
}

// Invoked by the manager on its own thread, and returns when the node stops.
void WINAPI executor::service_main(DWORD, LPWSTR*) NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    const auto name = system::to_utf16(name_);
    BC_POP_WARNING()

    status_handle_ = ::RegisterServiceCtrlHandlerExW(name.c_str(),
        &executor::service_handler, NULL);

    if (is_null(status_handle_))
        return;

    // Suppresses the hidden window and console capture, which have no meaning
    // in session zero, and are otherwise created by the executor constructor.
    service_ = true;
    report_status(SERVICE_START_PENDING);

    // The service has no console, so console output is discarded. This must
    // outlive the executor, which retains a reference to it.
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    static std::ostream discard{ nullptr };
    auto& host = executor::factory(*service_metadata_, *service_input_,
        discard, *service_error_);

    // Reports running from handle_running, and blocks until the node stops.
    const auto result = host.dispatch();
    BC_POP_WARNING()

    // Releases the poller from reporting stop progress.
    exited_.store(true);
    failed_.store(!result);
    report_status(SERVICE_STOPPED);
}

// Grant the account the right to log on as a service, without which the
// service control manager cannot start the service. This is idempotent, and
// is not revoked on removal, as other services may rely upon it.
DWORD executor::grant_logon_right(const std::string& account) NOEXCEPT
{
    using namespace system;
    const auto wide_account = to_utf16(account);

    DWORD sid_size{};
    DWORD domain_size{};
    SID_NAME_USE use{};
    ::LookupAccountNameW(
        NULL,
        wide_account.c_str(),
        NULL,
        &sid_size,
        NULL,
        &domain_size,
        &use);

    const auto sized = ::GetLastError();
    if (sized != ERROR_INSUFFICIENT_BUFFER)
        return sized;

    std::vector<uint8_t> sid(sid_size);
    std::vector<wchar_t> domain(domain_size);
    const auto identifier = pointer_cast<void>(sid.data());
    if (is_zero(::LookupAccountNameW(
        NULL,
        wide_account.c_str(),
        identifier,
        &sid_size,
        domain.data(),
        &domain_size,
        &use))) return ::GetLastError();

    LSA_HANDLE policy{};
    LSA_OBJECT_ATTRIBUTES attributes{};
    const auto opened = ::LsaOpenPolicy(
        NULL,
        &attributes,
        POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES,
        &policy);

    if (!is_zero(opened))
        return ::LsaNtStatusToWinError(opened);

    std::wstring name{ SE_SERVICE_LOGON_NAME };
    const auto length = name.size() * sizeof(wchar_t);
    LSA_UNICODE_STRING right
    {
        .Length = possible_narrow_cast<USHORT>(length),
        .MaximumLength = possible_narrow_cast<USHORT>(length + sizeof(wchar_t)),
        .Buffer = name.data()
    };

    const auto added = ::LsaAddAccountRights(policy, identifier, &right, 1);
    ::LsaClose(policy);
    return is_zero(added) ? ERROR_SUCCESS : ::LsaNtStatusToWinError(added);
}

// Returns a win32 error code, where zero is success.
DWORD executor::create_service(const std::string& command,
    const std::string& account, const std::string& password) NOEXCEPT
{
    using namespace system;
    const auto manager = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (is_null(manager))
        return ::GetLastError();

    // An empty account implies the local system account (with null password).
    const auto service_name = to_utf16(executor::name_);
    const auto service = ::CreateServiceW(
        manager,
        service_name.c_str(),
        BS_SERVICE_TITLE,
        SERVICE_CHANGE_CONFIG,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        to_utf16(command).c_str(),
        NULL,
        NULL,
        NULL,
        account.empty() ? NULL : to_utf16(account).c_str(),
        account.empty() ? NULL : to_utf16(password).c_str());

    if (is_null(service))
    {
        const auto result = ::GetLastError();
        ::CloseServiceHandle(manager);
        return result;
    }

    // Delayed start defers the node until boot completes, so that its startup
    // does not contend with that of the operating system.
    constexpr auto text = SERVICE_CONFIG_DESCRIPTION;
    constexpr auto start = SERVICE_CONFIG_DELAYED_AUTO_START_INFO;
    constexpr auto info = SERVICE_CONFIG_PRESHUTDOWN_INFO;

    BC_PUSH_WARNING(NO_CONST_CAST)
    SERVICE_DESCRIPTIONW description{ const_cast<LPWSTR>(BS_SERVICE_TEXT) };
    SERVICE_DELAYED_AUTO_START_INFO delayed{ TRUE };
    SERVICE_PRESHUTDOWN_INFO preshutdown{ preshutdown_milliseconds };
    BC_POP_WARNING()

    const auto configured =
        to_bool(::ChangeServiceConfig2W(service, text, &description)) &&
        to_bool(::ChangeServiceConfig2W(service, start, &delayed)) &&
        to_bool(::ChangeServiceConfig2W(service, info, &preshutdown));

    // The local system account holds the logon right implicitly.
    auto result = configured ? ERROR_SUCCESS : ::GetLastError();
    if (configured && !account.empty())
        result = grant_logon_right(account);

    // A service that cannot start is worse than none, so remove it.
    if (to_bool(result))
        ::DeleteService(service);

    ::CloseServiceHandle(service);
    ::CloseServiceHandle(manager);
    return result;
}

// Returns a win32 error code, where zero is success.
DWORD executor::delete_service() NOEXCEPT
{
    const auto manager = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (is_null(manager))
        return ::GetLastError();

    const auto service_name = system::to_utf16(executor::name_);
    const auto service = ::OpenServiceW(manager, service_name.c_str(), DELETE);
    if (is_null(service))
    {
        const auto result = ::GetLastError();
        ::CloseServiceHandle(manager);
        return result;
    }

    // The service is removed once it stops and all handles are closed.
    const auto result = is_zero(::DeleteService(service)) ? ::GetLastError() :
        ERROR_SUCCESS;

    ::CloseServiceHandle(service);
    ::CloseServiceHandle(manager);
    return result;
}

std::string executor::command_line(const std::filesystem::path& config) NOEXCEPT
{
    using namespace system;
    const auto module = module_path();
    if (module.empty())
        return {};

    return config.empty() ?
        (format(R"("%1%")") % from_path(module)).str() :
        (format(R"("%1%" --%2% "%3%")") % from_path(module) %
            BS_CONFIG_VARIABLE % from_path(qualified_path(config))).str();
}

#endif // HAVE_MSC

// Supervisor notification (noop unless running as a service).
// ----------------------------------------------------------------------------

void executor::notify_starting()
{
#if defined(HAVE_MSC)
    // Reported only while starting, as this merely advances the checkpoint.
    if (state_.load() == SERVICE_START_PENDING)
        report_status(SERVICE_START_PENDING);
#endif
}

void executor::notify_running()
{
#if defined(HAVE_MSC)
    report_status(SERVICE_RUNNING);
#endif
}

void executor::notify_stopping()
{
#if defined(HAVE_MSC)
    report_status(SERVICE_STOP_PENDING);
#endif
}

// Service dispatch.
// ----------------------------------------------------------------------------

#if defined(HAVE_MSC)

bool executor::service(parser& metadata, std::istream& input,
    std::ostream& error)
{
    service_metadata_ = &metadata;
    service_input_ = &input;
    service_error_ = &error;

    auto name = system::to_utf16(name_);
    const SERVICE_TABLE_ENTRYW table[]
    {
        { name.data(), &executor::service_main },
        { NULL, NULL }
    };

    // Blocks until the service stops, and fails immediately (without any
    // connection attempt) when the process was not started by the manager.
    if (!is_zero(::StartServiceCtrlDispatcherW(table)))
        return true;

    return ::GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT;
}

#else

bool executor::service(parser&, std::istream&, std::ostream&)
{
    return false;
}

#endif // HAVE_MSC

// --[d]aemon [--[u]ser]
bool executor::do_daemon()
{
    log_.stop();
    const auto& user = metadata_.configured.user;
    if (user.has_value() && !user.value().methods().empty())
    {
        logger(BS_DAEMON_INVALID_USER);
        return false;
    }

#if defined(HAVE_MSC)
    std::string account{}, password{};
    if (user.has_value())
    {
        account = user.value().username();
        password = user.value().password();
    }

    std::string command{};
    if (metadata_.configured.daemon.value())
    {
        if (command = command_line(metadata_.configured.file); command.empty())
        {
            logger(format(BS_DAEMON_INSTALL_FAILURE) % ERROR_BAD_PATHNAME);
            return false;
        }
    }

    const auto result = command.empty() ? delete_service() :
        create_service(command, account, password);

    switch (result)
    {
        case ERROR_SUCCESS:
        {
            logger(command.empty() ? BS_DAEMON_UNINSTALLED : BS_DAEMON_INSTALLED);
            return true;
        }
        case ERROR_ACCESS_DENIED:
        {
            logger(BS_DAEMON_ELEVATION);
            return false;
        }
        case ERROR_SERVICE_EXISTS:
        {
            logger(BS_DAEMON_EXISTS);
            return false;
        }
        case ERROR_SERVICE_DOES_NOT_EXIST:
        {
            logger(BS_DAEMON_ABSENT);
            return false;
        }
        case ERROR_INVALID_SERVICE_ACCOUNT:
        {
            logger(BS_DAEMON_UNKNOWN_USER);
            return false;
        }
        default:
        {
            logger(format(command.empty() ?
                BS_DAEMON_UNINSTALL_FAILURE :
                BS_DAEMON_INSTALL_FAILURE) % result);
            return false;
        }
    }
#else
    logger(BS_DAEMON_UNSUPPORTED);
    return false;
#endif
}

} // namespace server
} // namespace libbitcoin
