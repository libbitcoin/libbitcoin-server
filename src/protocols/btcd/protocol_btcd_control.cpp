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
#include <bitcoin/server/protocols/protocol_btcd_control.hpp>

#include <algorithm>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

using namespace system;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

std::string protocol_btcd_control::usage(
    const std::string& command) const NOEXCEPT
{
    auto out = base::usage(command);
    if (out.empty())
        find_usage<interface::btcd_methods>(out, command,
            interface_version());

    return out;
}

// btcd lists the usage of each served method, one per line, by name.
void protocol_btcd_control::send_help_list() NOEXCEPT
{
    auto names = split(help_names());
    std::sort(names.begin(), names.end());

    std::string out{};
    for (const auto& name: names)
    {
        auto line = usage(name);
        if (line.empty())
            line = name;

        if (!out.empty())
            out += '\n';

        out += line;
    }

    send_result(std::move(out));
}

void protocol_btcd_control::send_help_unknown(
    const std::string& command) NOEXCEPT
{
    send_error_message(error::btcd::invalid_parameter,
        "Unknown command: " + command);
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
