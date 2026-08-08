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
#include "stack_trace.hpp"

#include <bitcoin/system.hpp>

// This is some experimental code to explore emission of win32 stack dump.
#if defined(HAVE_MSC)

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>

#include <windows.h>
#include <dbghelp.h>

// This pulls in libs required for APIs used below.
#pragma comment(lib, "dbghelp.lib")

// Must define pdb_path() and handle_stack_trace when using dump_stack_trace.
extern std::wstring pdb_path();
extern void handle_stack_trace(const std::string& trace);

constexpr size_t depth_limit{ 32 };

using namespace bc;
using namespace system;

// The dump runs on a crashed thread, so it must not allocate (the fault may
// be heap corruption), must not throw (it may run inside an seh filter or a
// terminate handler), and must serialize (a fault storm crashes many threads
// at once). All state is static and the first crasher wins.
static std::atomic_bool dumping{};
static char tracer[16384];
static size_t tracer_at{};

static void trace_append(const char* format, ...) NOEXCEPT
{
    if (tracer_at >= sizeof(tracer))
        return;

    va_list args;
    va_start(args, format);
    const auto wrote = std::vsnprintf(&tracer[tracer_at],
        sizeof(tracer) - tracer_at, format, args);
    va_end(args);

    if (wrote > 0)
        tracer_at = std::min(tracer_at + wrote, sizeof(tracer));
}

inline STACKFRAME64 get_stack_frame(const CONTEXT& context) NOEXCEPT
{
    STACKFRAME64 frame{};

#if defined(HAVE_X64)
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
#else
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
#endif

    return frame;
}

// Static symbol buffer (no allocation on the crashed thread). SYMOPT_UNDNAME
// has dbghelp undecorate in place, so no separate undecorate pass or buffer.
static const char* get_name(HANDLE process, DWORD64 address) NOEXCEPT
{
    // Including null terminator.
    constexpr DWORD maximum_characters{ 1024 };
    static uint8_t symbol_buffer[sizeof(SYMBOL_INFOW) +
        maximum_characters * sizeof(wchar_t)];
    static char name[2 * maximum_characters];

    auto& symbol = *pointer_cast<SYMBOL_INFOW>(&symbol_buffer[0]);
    symbol = {};
    symbol.SizeOfStruct = sizeof(SYMBOL_INFOW);
    symbol.MaxNameLen = maximum_characters;

    DWORD64 displace{};
    if (SymFromAddrW(process, address, &displace, &symbol) == FALSE ||
        is_zero(symbol.NameLen))
        return nullptr;

    if (WideCharToMultiByte(CP_UTF8, 0, symbol.Name, -1, name, sizeof(name),
        NULL, NULL) == 0)
        return nullptr;

    return name;
}

// Walks and emits every frame it can: a frame without line information (crt,
// system module, missing pdb) reports symbol or raw address and the walk
// CONTINUES, as breaking there was the main source of empty dumps.
DWORD dump_stack_trace(unsigned code, EXCEPTION_POINTERS* exception) NOEXCEPT
{
    if (is_null(exception) || is_null(exception->ContextRecord))
        return EXCEPTION_EXECUTE_HANDLER;

    // First crasher wins; concurrent faulting threads pass through.
    if (dumping.exchange(true))
        return EXCEPTION_EXECUTE_HANDLER;

    const auto process = GetCurrentProcess();
    const auto thread = GetCurrentThread();

    // NULL for defaults, otherwise semicolon seperated directories.
    const auto path = pdb_path();
    const auto search = path.empty() ? NULL : path.c_str();

    // Failure is not fatal: the walk proceeds with raw addresses.
    const auto symbolized = (SymInitializeW(process, search, TRUE) != FALSE);
    if (symbolized)
        SymSetOptions(SymGetOptions()
            | SYMOPT_DEFERRED_LOADS
            | SYMOPT_LOAD_LINES
            | SYMOPT_UNDNAME);

#if defined(HAVE_X64)
    constexpr DWORD machine{ IMAGE_FILE_MACHINE_AMD64 };
#else
    constexpr DWORD machine{ IMAGE_FILE_MACHINE_I386 };
#endif

    trace_append("exception 0x%08x on thread %lu%s\n", code,
        GetCurrentThreadId(), symbolized ? "" : " ((sym init failed))");

    // StackWalk64 mutates the context, so walk a local copy.
    CONTEXT context = *exception->ContextRecord;
    auto it = get_stack_frame(context);

    for (size_t iteration{}; iteration < depth_limit; ++iteration)
    {
        const auto address = it.AddrPC.Offset;
        const auto name = symbolized ? get_name(process, address) : nullptr;

        DWORD displace{};
        IMAGEHLP_LINE64 line{ sizeof(IMAGEHLP_LINE64) };
        const auto lined = symbolized && (SymGetLineFromAddr64(process,
            address, &displace, &line) != FALSE);

        if (!is_null(name) && lined)
            trace_append("%s -> %s(%lu)\n", name, line.FileName,
                line.LineNumber);
        else if (!is_null(name))
            trace_append("%s @ 0x%016llx\n", name, address);
        else
            trace_append("0x%016llx\n", address);

        if (StackWalk64(machine, process, thread, &it, &context, NULL,
            SymFunctionTableAccess64, SymGetModuleBase64, NULL) == FALSE ||
            is_zero(it.AddrReturn.Offset))
            break;
    }

    handle_stack_trace(tracer);

    if (symbolized)
        SymCleanup(process);

    return EXCEPTION_EXECUTE_HANDLER;
}

// Process-wide coverage: seh frames are per-thread, so the wmain __except
// never sees a worker thread fault (the dominant cause of missing dumps).
static LONG WINAPI unhandled_stack_trace(EXCEPTION_POINTERS* exception) NOEXCEPT
{
    const auto record = exception->ExceptionRecord;
    dump_stack_trace(is_null(record) ? 0u : record->ExceptionCode, exception);
    return EXCEPTION_EXECUTE_HANDLER;
}

// An exception escaping a NOEXCEPT boundary invokes terminate directly (no
// seh search phase), bypassing both filters. Capture here and walk from the
// handler: the throwing frames remain below on an unwound-free msvc stack.
static void terminate_stack_trace() NOEXCEPT
{
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    EXCEPTION_RECORD record{};
    EXCEPTION_POINTERS exception{ &record, &context };
    dump_stack_trace(0u, &exception);
    std::abort();
}

void install_stack_trace() NOEXCEPT
{
    SetUnhandledExceptionFilter(&unhandled_stack_trace);
    std::set_terminate(&terminate_stack_trace);
}

#elif defined(HAVE_APPLE)

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <execinfo.h>
#include <unistd.h>

constexpr size_t depth_limit{ 64 };

using namespace bc;
using namespace system;

static std::atomic_bool dumping{};
static void* frames[depth_limit]{};

// Async-signal-safe: backtrace/backtrace_symbols_fd are the only walk apple
// documents for a signal handler, and write is the only safe emitter.
static void emit(const char* text) NOEXCEPT
{
    ::write(STDOUT_FILENO, text, std::char_traits<char>::length(text));
}

static void dump_stack_trace(int signal) NOEXCEPT
{
    auto expected = false;
    if (!dumping.compare_exchange_strong(expected, true))
        return;

    emit("<<fault - start trace>>\n");
    emit(::strsignal(signal));
    emit("\n");
    ::backtrace_symbols_fd(frames, ::backtrace(frames, depth_limit),
        STDOUT_FILENO);
    emit("<<fault - end trace>>\n");

    // Restore the default and re-raise, so the wait status reports the signal.
    ::signal(signal, SIG_DFL);
    ::raise(signal);
}

static void terminate_stack_trace() NOEXCEPT
{
    dump_stack_trace(SIGABRT);
    std::abort();
}

void install_stack_trace() NOEXCEPT
{
    ::signal(SIGBUS, &dump_stack_trace);
    ::signal(SIGSEGV, &dump_stack_trace);
    ::signal(SIGILL, &dump_stack_trace);
    ::signal(SIGFPE, &dump_stack_trace);
    std::set_terminate(&terminate_stack_trace);
}

#endif
