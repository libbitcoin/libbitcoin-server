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
#ifndef LIBBITCOIN_BS_STACK_TRACE_HPP
#define LIBBITCOIN_BS_STACK_TRACE_HPP

#include <bitcoin/system.hpp>

 // This is some temporary code to explore emission of win32 stack dump.
#if defined(HAVE_MSC)

#include <windows.h>

DWORD dump_stack_trace(unsigned code, EXCEPTION_POINTERS* exception) THROWS;

#endif
#endif
