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
#ifndef LIBBITCOIN_BS_STACK_TRACE_HPP
#define LIBBITCOIN_BS_STACK_TRACE_HPP

#include <bitcoin/system.hpp>

 // This is some temporary code to explore emission of win32 stack dump.
#if defined(HAVE_WINDOWS)

#include <windows.h>

/// Emit stack trace for given exception to handle_stack_trace. Safe on crashed
/// thread (no allocation, no throw); first concurrent caller wins.
DWORD dump_stack_trace(unsigned code, EXCEPTION_POINTERS* exception) NOEXCEPT;

/// Install process-wide crash coverage: unhandled exception filter (all
/// threads, where wmain seh frame covers only its own) and terminate handler
/// (throws escaping NOEXCEPT bypass seh entirely).
void install_stack_trace() NOEXCEPT;

#elif defined(HAVE_APPLE)

/// Install fault signal handlers, as apple ReportCrash does not fire for
/// process and its cores are unwritable at this map size.
void install_stack_trace() NOEXCEPT;

#endif
#endif
