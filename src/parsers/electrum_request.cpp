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
#include <bitcoin/server/parsers/electrum_request.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network::rpc;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

void electrum_request(request_t& message, electrum::version version,
    const system::config::version& minimum,
    const system::config::version& maximum) NOEXCEPT
{
    using server_version = interface::electrum_handshake::server_version;
    using transaction_get = interface::electrum::blockchain_transaction_get;

    if (!message.params)
    {
        if (message.method != server_version::name)
            return;

        message.params = array_t{};
    }

    auto& params = *message.params;
    const auto positional = std::get_if<array_t>(&params);
    const auto named = std::get_if<object_t>(&params);

    if (message.method == server_version::name)
    {
        // The default protocol_version is the minimum served (1.0 ignores it).
        const auto& methods = interface::electrum_handshake::methods;
        const auto& names = std::get<zero>(methods).parameter_names();
        const string_t lowest{ minimum.to_string() };
        const value_t* requested{};

        if (!positional && !named)
        {
            params = array_t{ std::move(std::get<value_t>(params)), lowest };
            requested = &std::get<array_t>(params).at(one);
        }
        else if (positional)
        {
            if (positional->empty())
                positional->push_back(string_t{});

            if (positional->size() == one)
                positional->push_back(lowest);

            requested = &positional->at(one);
        }
        else
        {
            requested = &named->try_emplace(string_t{ names.at(one) }, lowest).
                first->second;
        }

        auto negotiated = electrum::negotiate(*requested, minimum, maximum);
        if (negotiated < electrum::version::v1_6)
            return;

        // Extraneous arguments are tolerated at 1.6+, so purge them.
        if (positional && positional->size() > server_version::size)
        {
            positional->resize(server_version::size);
        }
        else if (named)
        {
            std::erase_if(*named, [&names](const auto& pair) NOEXCEPT
            {
                return !system::contains(names, pair.first);
            });
        }

        return;
    }

    if (message.method == transaction_get::name)
    {
        if (version < electrum::version::v1_1)
        {
            if (positional && positional->size() == two)
                positional->resize(one);
            else if (named)
                named->erase("height");
        }
    }
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
