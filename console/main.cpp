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
#include <cstdlib>
#include <filesystem>
#include <bitcoin/server.hpp>
#include "executor.hpp"

////// This is some experimental code to explore emission of win32 stack dump.
////#ifdef HAVE_MSC
////#include "stack_trace.hpp"

namespace libbitcoin {
namespace system {
    std::istream& cin = cin_stream();
    std::ostream& cout = cout_stream();
    std::ostream& cerr = cerr_stream();
    int main(int argc, char* argv[]);
} // namespace system
} // namespace libbitcoin

namespace bc = libbitcoin;
////std::filesystem::path symbols_path{};
////
////int wmain(int argc, wchar_t* argv[])
////{
////    __try
////    {
////        return bc::system::call_utf8_main(argc, argv, &bc::system::main);
////    }
////    __except (dump_stack_trace(GetExceptionCode(), GetExceptionInformation()))
////    {
////        return -1;
////    }
////}
////
////// This is invoked by dump_stack_trace.
////void handle_stack_trace(const std::string& trace)
////{
////    if (trace.empty())
////    {
////        bc::system::cout << "<<unhandled exception>>" << std::endl;
////        return;
////    }
////
////    bc::system::cout << "<<unhandled exception - start trace>>" << std::endl;
////    bc::system::cout << trace << std::endl;
////    bc::system::cout << "<<unhandled exception - end trace>>" << std::endl;
////}
////
////// This is invoked by dump_stack_trace.
////std::wstring pdb_path()
////{
////    return bc::system::to_extended_path(symbols_path);
////}
////
////#else
BC_USE_LIBBITCOIN_MAIN
////#endif

/// Invoke this program with the raw arguments provided on the command line.
/// All console input and output streams for the application originate here.
int bc::system::main(int argc, char* argv[])
{
    using namespace bc;
    using namespace bc::server;
    using namespace bc::system;

    // en.cppreference.com/w/cpp/io/ios_base/sync_with_stdio
    std::ios_base::sync_with_stdio(false);

    set_utf8_stdio();
    parser metadata(chain::selection::mainnet);
    const auto& args = const_cast<const char**>(argv);

    if (!metadata.parse(argc, args, cerr))
        return -1;

////#if defined(HAVE_MSC)
////    symbols_path = metadata.configured.log.symbols;
////#endif

// requires _WIN32_WINNT set to 0x0602 (defaults 0x0602 in vc++ 2022).
#if defined(HAVE_MSC) && defined(MEMORY_PRIORITY_INFORMATION)

    // Set low memory priority on the current process.
    const MEMORY_PRIORITY_INFORMATION priority{ MEMORY_PRIORITY_LOW };
    SetProcessInformation(
        GetCurrentProcess(),
        ProcessMemoryPriority,
        &priority,
        sizeof(priority));

#endif

    executor host(metadata, cin, cout, cerr);
    ////return host.dispatch() ? 0 : -1;
    return host.menu() ? 0 : -1;
}
