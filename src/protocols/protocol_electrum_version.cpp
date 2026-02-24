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
#include <bitcoin/server/protocols/protocol_electrum_version.hpp>

#include <algorithm>
#include <variant>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/parsers/parsers.hpp>
#include <bitcoin/server/protocols/protocol_rpc.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_electrum_version

using namespace network;
using namespace interface;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Start/finished (handshake).
// ----------------------------------------------------------------------------

// Session resumes the channel following return from start().
// Sends are not precluded, but no messages can be received while paused.
void protocol_electrum_version::shake(result_handler&& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
    {
        handler(network::error::operation_failed);
        return;
    }

    handler_ = system::move_shared<result_handler>(std::move(handler));

    SUBSCRIBE_RPC(handle_server_version, _1, _2, _3, _4);
    protocol_rpc<channel_electrum>::start();
}

void protocol_electrum_version::finished(const code& ec,
    const code& shake) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // Calls after handshake completion are allowed and will skip this.
    if (handler_)
    {
        // Invoke handshake completion, error will result in stopped channel.
        (*handler_)(shake);
        handler_.reset();
    }
}

// Handler.
// ----------------------------------------------------------------------------

// Changed in version 1.6: server must tolerate and ignore extraneous args.
void protocol_electrum_version::handle_server_version(const code& ec,
    rpc_interface::server_version, const std::string& client_name,
    const value_t& protocol_version) NOEXCEPT
{
    if (stopped(ec))
        return;

    // v0_0 implies version has not been set (first call).
    if ((channel_->version() == electrum_version::v0_0) &&
        (!set_client(client_name) || !set_version(protocol_version)))
    {
        const auto reason = error::invalid_argument;
        send_code(reason, BIND(finished, _1, reason));
    }
    else
    {
        send_result(value_t
            {
                array_t
                {
                    { string_t{ server_name() } },
                    { string_t{ negotiated_version() } }
                }
            }, 70, BIND(finished, _1, error::success));
    }

    // Handshake must leave channel paused, before leaving stranded handler.
    if (handler_) pause();
}

// Client/server names.
// ----------------------------------------------------------------------------

std::string_view protocol_electrum_version::server_name() const NOEXCEPT
{
    return network_settings().user_agent;
}

std::string_view protocol_electrum_version::client_name() const NOEXCEPT
{
    return channel_->client();
}

bool protocol_electrum_version::set_client(const std::string& name) NOEXCEPT
{
    // Avoid excess, empty name is allowed.
    if (name.size() > max_client_name_length)
        return false;

    // Do not put to log without escaping.
    channel_->set_client(escape_client(name));
    return true;
}

std::string protocol_electrum_version::escape_client(
    const std::string& in) NOEXCEPT
{
    std::string out(in.size(), '*');
    std::transform(in.begin(), in.end(), out.begin(), [](char c) NOEXCEPT
    {
        using namespace system;
        return is_ascii_character(c) && !is_ascii_whitespace(c) ? c : '*';
    });

    return out;
}

// Negotiated version.
// ----------------------------------------------------------------------------

std::string_view protocol_electrum_version::negotiated_version() const NOEXCEPT
{
    return version_to_string(channel_->version());
}

bool protocol_electrum_version::set_version(const value_t& version) NOEXCEPT
{
    electrum_version client_min{};
    electrum_version client_max{};
    if (!get_versions(client_min, client_max, version))
        return false;

    const auto lower = std::max(client_min, minimum);
    const auto upper = std::min(client_max, maximum);
    if (lower > upper)
        return false;

    LOGA("Electrum [" << opposite() << "] version ("
        << version_to_string(client_max) << ") " << client_name());

    channel_->set_version(upper);
    return true;
}

bool protocol_electrum_version::get_versions(electrum_version& min,
    electrum_version& max, const interface::value_t& version) NOEXCEPT
{
    // Optional value_t can be string_t or array_t of two string_t.
    const auto& value = version.value();

    // Default version (null_t is the default of value_t).
    if (std::holds_alternative<null_t>(value))
    {
        // An interface default can't be set for optional<value_t>.
        max = min = electrum_version::v1_4;
        return true;
    }

    // One version.
    if (std::holds_alternative<string_t>(value))
    {
        // A single value implies minimum is the same as maximum.
        max = min = version_from_string(std::get<string_t>(value));
        return min != electrum_version::v0_0;
    }

    // Two versions.
    if (std::holds_alternative<array_t>(value))
    {
        const auto& versions = std::get<array_t>(value);
        if (versions.size() != two)
            return false;

        // First string is mimimum, second is maximum.
        const auto& min_version = versions.at(0).value();
        const auto& max_version = versions.at(1).value();
        if (!std::holds_alternative<string_t>(min_version) ||
            !std::holds_alternative<string_t>(max_version))
            return false;

        min = version_from_string(std::get<string_t>(min_version));
        max = version_from_string(std::get<string_t>(max_version));
        return min != electrum_version::v0_0
            && max != electrum_version::v0_0;
    }

    return false;
}

// private/static
std::string_view protocol_electrum_version::version_to_string(
    electrum_version version) NOEXCEPT
{
    static const std::unordered_map<electrum_version, std::string_view> map
    {
        { electrum_version::v0_0,   "0.0" },
        { electrum_version::v0_6,   "0.6" },
        { electrum_version::v0_8,   "0.8" },
        { electrum_version::v0_9,   "0.9" },
        { electrum_version::v0_10,  "0.10" },
        { electrum_version::v1_0,   "1.0" },
        { electrum_version::v1_1,   "1.1" },
        { electrum_version::v1_2,   "1.2" },
        { electrum_version::v1_3,   "1.3" },
        { electrum_version::v1_4,   "1.4" },
        { electrum_version::v1_4_1, "1.4.1" },
        { electrum_version::v1_4_2, "1.4.2" },
        { electrum_version::v1_6,   "1.6" }
    };

    const auto it = map.find(version);
    return it != map.end() ? it->second : "0.0";
}

// private/static
electrum_version protocol_electrum_version::version_from_string(
    const std::string_view& version) NOEXCEPT
{
    static const std::unordered_map<std::string_view, electrum_version> map
    {
        { "0.0",   electrum_version::v0_0 },
        { "0.6",   electrum_version::v0_6 },
        { "0.8",   electrum_version::v0_8 },
        { "0.9",   electrum_version::v0_9 },
        { "0.10",  electrum_version::v0_10 },
        { "1.0",   electrum_version::v1_0 },
        { "1.1",   electrum_version::v1_1 },
        { "1.2",   electrum_version::v1_2 },
        { "1.3",   electrum_version::v1_3 },
        { "1.4",   electrum_version::v1_4 },
        { "1.4.1", electrum_version::v1_4_1 },
        { "1.4.2", electrum_version::v1_4_2 },
        { "1.6",   electrum_version::v1_6 }
    };

    const auto it = map.find(version);
    return it != map.end() ? it->second : electrum_version::v0_0;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
