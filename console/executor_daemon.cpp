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
#if defined(HAVE_LINUX)
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif
#include "localize.hpp"

namespace libbitcoin {
namespace server {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

using format = boost_format;

// TODO: register a systemd unit (linux) and launchd plist (osx), and notify
// TODO: readiness/stopping via sd_notify, in place of the manager below.

bool executor::service_{ false };
std::atomic_bool executor::exited_{ false };
std::atomic_bool executor::starting_{ true };

// Installation results, as reported by the platform service manager.
#if defined(HAVE_MSC)
constexpr uint32_t daemon_success = ERROR_SUCCESS;
constexpr uint32_t daemon_denied = ERROR_ACCESS_DENIED;
constexpr uint32_t daemon_exists = ERROR_SERVICE_EXISTS;
constexpr uint32_t daemon_absent = ERROR_SERVICE_DOES_NOT_EXIST;
constexpr uint32_t daemon_unknown_user = ERROR_INVALID_SERVICE_ACCOUNT;
#elif defined(HAVE_POSIX)
constexpr uint32_t daemon_success = 0;
constexpr uint32_t daemon_denied = EACCES;
constexpr uint32_t daemon_exists = EEXIST;
constexpr uint32_t daemon_absent = ENOENT;
#endif

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
    const auto name = system::to_utf16(name_);
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
    static std::ostream discard{ nullptr };
    auto& host = executor::factory(*service_metadata_, *service_input_,
        discard, *service_error_);

    // Reports running from handle_running, and blocks until the node stops.
    const auto result = host.dispatch();

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
uint32_t executor::create_service(const std::filesystem::path& config,
    const std::string& account, const std::string& password) NOEXCEPT
{
    using namespace system;
    const auto command = command_line(config);
    if (command.empty())
        return ERROR_BAD_PATHNAME;

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
uint32_t executor::delete_service() NOEXCEPT
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

#endif // HAVE_MSC

// Posix service installation.
// ----------------------------------------------------------------------------

#if defined(HAVE_POSIX)

// The drain exceeds the default stop timeout on both platforms.
constexpr auto stop_timeout_seconds = 600;

#if defined(HAVE_LINUX)

// A systemd unit, and the symlink by which the unit is enabled.
static std::filesystem::path unit_path(const std::string& name) NOEXCEPT
{
    return { "/etc/systemd/system/" + name + ".service" };
}

static std::filesystem::path enabled_path(const std::string& name) NOEXCEPT
{
    return { "/etc/systemd/system/multi-user.target.wants/" + name + ".service" };
}

// Type=notify defers dependent units until the node reports ready, which is
// the posix counterpart to reporting SERVICE_RUNNING to the manager.
static std::string unit_text(const std::string& command,
    const std::string& account) NOEXCEPT
{
    return
        "[Unit]\n"
        "Description=" BS_SERVICE_DESCRIPTION "\n"
        "After=network-online.target\n"
        "Wants=network-online.target\n"
        "\n"
        "[Service]\n"
        "Type=notify\n"
        "ExecStart=" + command + "\n" +
        (account.empty() ? "" : "User=" + account + "\n") +
        "TimeoutStopSec=" + std::to_string(stop_timeout_seconds) + "\n"
        "Restart=on-failure\n"
        "\n"
        "[Install]\n"
        "WantedBy=multi-user.target\n";
}

#else // HAVE_APPLE

static std::filesystem::path unit_path(const std::string&) NOEXCEPT
{
    return { "/Library/LaunchDaemons/" BS_SERVICE_LABEL ".plist" };
}

// launchd has no readiness protocol, so the job is simply run at load.
static std::string unit_text(const std::string& command,
    const std::string& account) NOEXCEPT
{
    return
        R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
        R"(<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" )"
        R"("http://www.apple.com/DTDs/PropertyList-1.0.dtd">)" "\n"
        R"(<plist version="1.0">)" "\n"
        "<dict>\n"
        "    <key>Label</key><string>" BS_SERVICE_LABEL "</string>\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n" + command +
        "    </array>\n" +
        (account.empty() ? "" :
            "    <key>UserName</key><string>" + account + "</string>\n") +
        "    <key>RunAtLoad</key><true/>\n"
        "    <key>KeepAlive</key><true/>\n"
        "    <key>ExitTimeOut</key><integer>" +
            std::to_string(stop_timeout_seconds) + "</integer>\n"
        "</dict>\n"
        "</plist>\n";
}

#endif // HAVE_LINUX

// Returns an errno value, where zero is success.
uint32_t executor::create_service(const std::filesystem::path& config,
    const std::string& account, const std::string&) NOEXCEPT
{
    const auto command = command_line(config);
    if (command.empty())
        return EINVAL;

    code ec{};
    const auto unit = unit_path(name_);
    if (std::filesystem::exists(unit, ec))
        return EEXIST;

    // The directory is absent when the service manager is not installed.
    if (!std::filesystem::is_directory(unit.parent_path(), ec))
        return ENOENT;

    system::ofstream file{ unit };
    if (!file.good())
        return EACCES;

    file << unit_text(command, account);
    file.flush();
    if (!file.good())
        return EIO;

#if defined(HAVE_LINUX)
    // This is what enabling a unit does, avoiding a systemctl invocation.
    // A reload is required before the unit can be started without a reboot.
    file.close();
    const auto enabled = enabled_path(name_);
    std::filesystem::create_directories(enabled.parent_path(), ec);
    if (ec)
        return EACCES;

    std::filesystem::create_symlink(unit, enabled, ec);
    if (ec)
        return EACCES;
#endif

    return {};
}

// Returns an errno value, where zero is success.
uint32_t executor::delete_service() NOEXCEPT
{
    const auto unit = unit_path(name_);
    code ec{};
    if (!std::filesystem::exists(unit, ec))
        return ENOENT;

#if defined(HAVE_LINUX)
    std::filesystem::remove(enabled_path(name_), ec);
#endif

    return std::filesystem::remove(unit, ec) ? 0_u32 : EACCES;
}

#endif // HAVE_POSIX

// Command line.
// ----------------------------------------------------------------------------

#if defined(HAVE_APPLE)

// launchd requires the arguments as discrete elements, not a command line.
std::string executor::command_line(const std::filesystem::path& config) NOEXCEPT
{
    using namespace system;
    const auto module = module_path();
    if (module.empty())
        return {};

    const auto element = [](const std::string& text) NOEXCEPT
    {
        return "        <string>" + text + "</string>\n";
    };

    return config.empty() ? element(from_path(module)) :
        element(from_path(module)) + element("--" BS_CONFIG_VARIABLE) +
            element(from_path(qualified_path(config)));
}

#else

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

#endif // HAVE_APPLE

// Supervisor notification (noop unless running as a service).
// ----------------------------------------------------------------------------

#if defined(HAVE_LINUX)

// Notify the service manager, as defined by sd_notify(3). The socket is unset
// unless the unit is Type=notify, in which case this is a noop. Implemented
// here to avoid a libsystemd dependency for two datagrams.
static void notify_manager(const std::string& state) NOEXCEPT
{
    const auto path = std::getenv("NOTIFY_SOCKET");
    if (is_null(path))
        return;

    const auto descriptor = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (system::is_negative(descriptor))
        return;

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, path, sizeof(address.sun_path) - 1u);

    // A leading '@' denotes the abstract namespace, encoded as a null.
    if (address.sun_path[0] == '@')
        address.sun_path[0] = '\0';

    ::sendto(descriptor, state.data(), state.size(), MSG_NOSIGNAL,
        system::pointer_cast<const sockaddr>(&address), sizeof(address));

    ::close(descriptor);
}

#endif // HAVE_LINUX

void executor::notify_starting()
{
    // Reported only while starting, as this merely extends the timeout.
    if (!starting_.load())
        return;

#if defined(HAVE_MSC)
    report_status(SERVICE_START_PENDING);
#elif defined(HAVE_LINUX)
    notify_manager("EXTEND_TIMEOUT_USEC=30000000");
#endif
}

void executor::notify_running()
{
    starting_.store(false);

#if defined(HAVE_MSC)
    report_status(SERVICE_RUNNING);
#elif defined(HAVE_LINUX)
    notify_manager("READY=1");
#endif
}

void executor::notify_stopping()
{
#if defined(HAVE_MSC)
    report_status(SERVICE_STOP_PENDING);
#elif defined(HAVE_LINUX)
    notify_manager("STOPPING=1\nEXTEND_TIMEOUT_USEC=30000000");
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

// A posix daemon runs in the foreground under its manager, so there is no
// dispatcher to connect. This only detects that a manager is supervising.
bool executor::service(parser&, std::istream&, std::ostream&)
{
#if defined(HAVE_LINUX)
    // Set by systemd for a Type=notify unit.
    service_ = !is_null(std::getenv("NOTIFY_SOCKET"));
#elif defined(HAVE_APPLE)
    // Set by launchd for a managed job.
    service_ = !is_null(std::getenv("XPC_SERVICE_NAME"));
#endif

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

#if defined(HAVE_MSC) || defined(HAVE_POSIX)
    std::string account{}, password{};
    if (user.has_value())
    {
        account = user.value().username();
        password = user.value().password();
    }

    const auto install = metadata_.configured.daemon.value();
    const auto result = install ?
        create_service(metadata_.configured.file, account, password) :
        delete_service();

    switch (result)
    {
        case daemon_success:
        {
            logger(install ? BS_DAEMON_INSTALLED : BS_DAEMON_UNINSTALLED);
            return true;
        }
        case daemon_denied:
        {
            logger(BS_DAEMON_ELEVATION);
            return false;
        }
        case daemon_exists:
        {
            logger(BS_DAEMON_EXISTS);
            return false;
        }
        case daemon_absent:
        {
            logger(BS_DAEMON_ABSENT);
            return false;
        }
#if defined(HAVE_MSC)
        case daemon_unknown_user:
        {
            logger(BS_DAEMON_UNKNOWN_USER);
            return false;
        }
#endif
        default:
        {
            logger(format(install ? BS_DAEMON_INSTALL_FAILURE :
                BS_DAEMON_UNINSTALL_FAILURE) % result);
            return false;
        }
    }
#else
    logger(BS_DAEMON_UNSUPPORTED);
    return false;
#endif
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
