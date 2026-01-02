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
#include "stack_trace.hpp"

#include <bitcoin/system.hpp>

// This is some experimental code to explore emission of win32 stack dump.
#if defined(HAVE_MSC)

#include <algorithm>
#include <exception>
#include <iterator>
#include <new>
#include <sstream>

#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>

// This pulls in libs required for APIs used below.
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")

// Must define pdb_path() and handle_stack_trace when using dump_stack_trace.
extern std::wstring pdb_path();
extern void handle_stack_trace(const std::string& trace);

constexpr size_t depth_limit{ 10 };

using namespace bc;
using namespace system;

struct module_data
{
    std::wstring image;
    std::wstring module;
    DWORD dll_size;
    void* dll_base;
};

inline STACKFRAME64 get_stack_frame(const CONTEXT& context)
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

static std::string get_undecorated(HANDLE process, DWORD64 address)
{
    // Including null terminator.
    constexpr DWORD maximum_characters{ 1024 };

    // First character in the name is accounted for in structure size.
    constexpr size_t struct_size{ sizeof(SYMBOL_INFOW) +
        sub1(maximum_characters) * sizeof(wchar_t) };

    BC_PUSH_WARNING(NO_NEW_OR_DELETE)
    const auto symbol = std::unique_ptr<SYMBOL_INFOW>(
        pointer_cast<SYMBOL_INFOW>(operator new(struct_size)));
    BC_POP_WARNING()

    if (!symbol)
        return {};

    symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
    symbol->TypeIndex = 0;
    symbol->Reserved[0] = 0;
    symbol->Reserved[1] = 0;
    symbol->Index = 0;
    symbol->Size = 0;
    symbol->ModBase = 0;
    symbol->Flags = 0;
    symbol->Value = 0;
    symbol->Address = 0;
    symbol->Register = 0;
    symbol->Scope = 0;
    symbol->Tag = 0;
    symbol->NameLen = 0;
    symbol->MaxNameLen = maximum_characters;
    symbol->Name[0] = wchar_t{ '\0' };

    DWORD64 displace{};
    if (SymFromAddrW(process, address, &displace, symbol.get()) == FALSE ||
        is_zero(symbol->MaxNameLen))
    {
        return {};
    }

    std::wstring undecorated{};
    undecorated.resize(maximum_characters);
    undecorated.resize(UnDecorateSymbolNameW(symbol->Name,
        undecorated.data(), maximum_characters, UNDNAME_COMPLETE));

    return to_utf8(undecorated);
}

inline DWORD get_machine(HANDLE process) THROWS
{
    constexpr size_t module_buffer_size{ 4096 };

    // Get required bytes by passing zero.
    DWORD bytes{};
    if (EnumProcessModules(process, NULL, 0u, &bytes) == FALSE)
        throw(std::logic_error("EnumProcessModules"));

    std_vector<HMODULE> handles{};
    handles.resize(bytes / sizeof(HMODULE));
    const auto handles_buffer_size = possible_narrow_cast<DWORD>(
        handles.size() * sizeof(HMODULE));

    // Get all elements into contiguous byte vector.
    if (EnumProcessModules(process, &handles.front(), handles_buffer_size,
        &bytes) == FALSE)
        throw(std::logic_error("EnumProcessModules"));

    std_vector<module_data> modules{};
    std::transform(handles.begin(), handles.end(),
        std::back_inserter(modules), [process](const auto& handle) THROWS
        {
            MODULEINFO info{};
            if (GetModuleInformation(process, handle, &info,
                sizeof(MODULEINFO)) == FALSE)
                throw(std::logic_error("GetModuleInformation"));

            module_data data{};
            data.dll_base = info.lpBaseOfDll;
            data.dll_size = info.SizeOfImage;

            data.image.resize(module_buffer_size);
            if (GetModuleFileNameExW(process, handle, data.image.data(),
                module_buffer_size) == FALSE)
                throw(std::logic_error("GetModuleFileNameExA"));

            data.module.resize(module_buffer_size);
            if (GetModuleBaseNameW(process, handle, data.module.data(),
                module_buffer_size) == FALSE)
                throw(std::logic_error("GetModuleBaseNameA"));

            SymLoadModuleExW(process, NULL, data.image.data(), data.module.data(),
                reinterpret_cast<DWORD64>(data.dll_base), data.dll_size, NULL,
                SLMFLAG_NO_SYMBOLS);

            return data;
        });

    const auto header = ImageNtHeader(modules.front().dll_base);

    if (is_null(header))
        throw(std::logic_error("ImageNtHeader"));

    return header->FileHeader.Machine;
}

DWORD dump_stack_trace(unsigned, EXCEPTION_POINTERS* exception) THROWS
{
    if (is_null(exception) || is_null(exception->ContextRecord))
        return EXCEPTION_EXECUTE_HANDLER;

    // NULL for defaults, otherwise semicolon seperated directories.
    const auto search = pdb_path().empty() ? NULL : pdb_path().c_str();
    const auto process = GetCurrentProcess();
    const auto thread = GetCurrentThread();

    if (SymInitializeW(process, search, TRUE) == FALSE)
        throw(std::logic_error("SymInitialize"));

    SymSetOptions(SymGetOptions()
        | SYMOPT_DEFERRED_LOADS
        | SYMOPT_LOAD_LINES
        | SYMOPT_UNDNAME);

    DWORD displace{};
    std::ostringstream tracer{};
    IMAGEHLP_LINE64 line{ sizeof(IMAGEHLP_LINE64) };
    const auto machine = get_machine(process);
    const auto context = exception->ContextRecord;
    auto it = get_stack_frame(*context);
    auto iteration = zero;

    do
    {
        // Get undecorated function name.
        const auto name = get_undecorated(process, it.AddrPC.Offset);

        // When this?
        if (name == "main")
        {
            tracer << "((no symbols))";
            break;
        }

        // Compiled in release mode.
        if (name == "RaiseException")
        {
            tracer << "[[no symbols]]";
            break;
        }

        // Get line.
        if (SymGetLineFromAddr64(process, it.AddrPC.Offset, &displace,
            &line) == FALSE)
            break;
        
        // Write line.
        tracer
            << name << " -> " << line.FileName << "(" << line.LineNumber << ")"
            << std::endl;
        
        // Advance iterator.
        if (StackWalk64(machine, process, thread, &it, context, NULL,
            SymFunctionTableAccess64, SymGetModuleBase64, NULL) == FALSE)
            break;

    } while ((iteration++ < depth_limit) && !is_zero(it.AddrReturn.Offset));

    handle_stack_trace(tracer.str());

    if (SymCleanup(process) == FALSE)
        throw(std::logic_error("SymCleanup"));

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif
