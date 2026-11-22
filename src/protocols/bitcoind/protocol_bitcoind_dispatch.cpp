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
#include <bitcoin/server/protocols/protocol_bitcoind_dispatch.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>

namespace libbitcoin {
namespace server {

#define TEMPLATE template <typename Interface>
#define CLASS protocol_bitcoind_dispatch<Interface>

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// A notification of a query is not executed, as its reply is empty.
TEMPLATE
bool CLASS::is_ignored(const network::rpc::request_t& message) NOEXCEPT
{
    return (message.jsonrpc == network::rpc::version::v2) &&
        !message.id.has_value() &&
        !network::rpc::is_command<Interface::methods>(message.method);
}

// bitcoind accepts the members of these options objects as named arguments
// (RPCArg::Type::OBJ_NAMED_PARAMS), collected into the options parameter.
struct named_options
{
    std::string_view method;
    std::array<std::string_view, 3> members;
};

static constexpr std::array<named_options, 4> options_members
{{
    { "dumptxoutset", { "rollback", "in_memory" } },
    { "gettxspendingprevout", { "mempool_only", "return_spending_tx" } },
    { "importmempool", { "use_current_time", "apply_fee_delta_priority",
        "apply_unbroadcast_set" } },
    { "scanblocks", { "filter_false_positives" } }
}};

// True if any alias of the parameter names a member.
static bool is_named(const std::string_view& name,
    const network::rpc::object_t& object) NOEXCEPT
{
    for (auto aliases = name; !aliases.empty();)
    {
        const auto bar = aliases.find('|');
        if (object.contains(std::string{ aliases.substr(zero, bar) }))
            return true;

        aliases = (bar == std::string_view::npos) ? std::string_view{} :
            aliases.substr(add1(bar));
    }

    return false;
}

// Named arguments are transformed as by bitcoind: options members are collected
// into options parameter, and positional arguments of "args" are named by
// leading parameters. Otherwise out is not set.
TEMPLATE
code CLASS::transform(std::optional<network::rpc::request_t>& out,
    const network::rpc::request_t& in) NOEXCEPT
{
    using namespace network::rpc;
    if (!in.params.has_value() ||
        !std::holds_alternative<object_t>(in.params.value()))
        return error::success;

    const auto& object = std::get<object_t>(in.params.value());
    const auto entry = std::ranges::find(options_members, in.method,
        &named_options::method);
    const auto member = [&](const std::string_view& name) NOEXCEPT
    {
        return !name.empty() && object.contains(std::string{ name });
    };

    const auto optioned = (entry != options_members.end()) &&
        std::ranges::any_of(entry->members, member);
    const auto positioned = object.contains("args");
    if (!optioned && !positioned)
        return error::success;

    out.emplace(in);
    auto& params = std::get<object_t>(out->params.value());

    if (optioned)
    {
        if (params.contains("options"))
            return network::error::extra_named;

        object_t options{};
        for (const auto& name: entry->members)
        {
            if (const auto it = params.find(std::string{ name });
                !name.empty() && it != params.end())
            {
                options.emplace(it->first, std::move(it->second));
                params.erase(it);
            }
        }

        params.emplace("options", std::move(options));
    }

    // A non-array "args" remains, refused as an unknown named argument.
    const auto it = params.find("args");
    if (it == params.end() ||
        !std::holds_alternative<array_t>(it->second.value()))
        return error::success;

    auto args = std::get<array_t>(std::move(it->second.value()));
    params.erase(it);

    const auto names = parameter_names<Interface::methods>(in.method);
    if (args.size() > names.size())
        return network::error::extra_positional;

    for (size_t index{}; index < args.size(); ++index)
    {
        const auto& name = names[index];
        if (is_named(name, params))
            return network::error::extra_named;

        params.emplace(std::string{ primary_name(name) },
            std::move(args[index]));
    }

    return error::success;
}

// Subscribe to the post and websocket transports of the interface.
TEMPLATE
void CLASS::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    register_names();

    using websocket = network::http::method::websocket;
    using stream = network::http::method::stream;
    subscribe_channel<CLASS, post>(&CLASS::handle_receive_post,
        std::placeholders::_1, std::placeholders::_2);
    subscribe_channel<CLASS, websocket>(&CLASS::handle_receive_websocket,
        std::placeholders::_1, std::placeholders::_2);
    subscribe_channel<CLASS, stream>(&CLASS::handle_receive_stream,
        std::placeholders::_1, std::placeholders::_2);
    network::protocol::start();
}

TEMPLATE
void CLASS::register_names() NOEXCEPT
{
    for (const auto& name: system::split(Interface::names))
        if (is_active(name))
            register_methods(name);
}

TEMPLATE
void CLASS::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    rpc_dispatcher_.stop(ec);
    protocol_bitcoind::stopping(ec);
}

// The post transport of the interface subgroup.
TEMPLATE
void CLASS::handle_receive_post(const code& ec,
    const post::cptr& post) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return;

    // A protocol attached earlier owns the method (overrides this one).
    if (claimed())
        return;

    // Silently defer invalidated requests to the terminal responder.
    if (!is_allowed_host(*post, post->version()) ||
        !is_allowed_origin(*post, post->version()) ||
        !post->body().template contains<network::rpc::request>())
        return;

    // Get the parsed json-rpc request object.
    const auto& message =
        post->body().template get<network::rpc::request>().message;

    // Silently defer methods not defined by interface subgroup, or not
    // introduced at interface version of channel.
    if (!rpc_dispatcher::contains(message.method) ||
        !is_active(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();

    // Post is saved off during asynchronous handling and used by senders to
    // formulate response headers, isolating handlers from http.
    set_rpc_request(message.jsonrpc, message.id, post);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        send_forbidden();
        return;
    }

    if (is_ignored(message))
    {
        send_result({});
        return;
    }

    std::optional<network::rpc::request_t> transformed{};
    if (const auto code = transform(transformed, message))
    {
        handle_dispatch_fault(code);
        return;
    }

    // Dispatch the request to the interface dispatcher.
    const auto& dispatched = transformed ? *transformed : message;
    if (const auto code = rpc_dispatcher_.notify(dispatched))
        handle_dispatch_fault(code);
}

// The websocket transport of the interface subgroup.
TEMPLATE
void CLASS::dispatch_websocket(
    const network::http::request& request) NOEXCEPT
{
    BC_ASSERT(stranded());

    // A protocol attached earlier owns the method (overrides this one).
    if (claimed())
        return;

    // ws frames carry no headers, so ws authorization is enforced here, not
    // by channel. Unauthorized and non-rpc frames are silently
    // deferred to terminal responder.
    if (!authorized() ||
        !request.body().template contains<network::rpc::request>())
        return;

    const auto& message =
        request.body().template get<network::rpc::request>().message;

    // Silently defer methods not defined by interface subgroup, or not
    // introduced at interface version of channel.
    if (!rpc_dispatcher::contains(message.method) ||
        !is_active(message.method))
        return;

    // Claim the request (informs the terminal responder).
    set_claimed();

    // Cache request context for response building (version + id).
    set_rpc_request(message);

    // The credential may be restricted to a subset of interface methods.
    if (!permitted(message.method))
    {
        const code unauthorized{ network::error::unauthorized };
        send_error(unauthorized, unauthorized);
        return;
    }

    if (is_ignored(message))
    {
        send_result({});
        return;
    }

    std::optional<network::rpc::request_t> transformed{};
    if (const auto code = transform(transformed, message))
    {
        handle_dispatch_fault(code);
        return;
    }

    // Dispatch the request to the interface dispatcher.
    const auto& dispatched = transformed ? *transformed : message;
    if (const auto code = rpc_dispatcher_.notify(dispatched))
        handle_dispatch_fault(code);
}

BC_POP_WARNING()

#undef CLASS
#undef TEMPLATE

// Explicit instantiation of subgroup dispatch protocols, isolating dispatch
// metaprogramming to this translation unit.
template class protocol_bitcoind_dispatch<interface::bitcoind_blockchain>;
template class protocol_bitcoind_dispatch<interface::bitcoind_control>;
template class protocol_bitcoind_dispatch<interface::bitcoind_handshake>;
template class protocol_bitcoind_dispatch<interface::bitcoind_mining>;
template class protocol_bitcoind_dispatch<interface::bitcoind_network>;
template class protocol_bitcoind_dispatch<interface::bitcoind_notifications>;
template class protocol_bitcoind_dispatch<interface::bitcoind_test>;
template class protocol_bitcoind_dispatch<interface::bitcoind_transaction>;
template class protocol_bitcoind_dispatch<interface::bitcoind_utility>;
template class protocol_bitcoind_dispatch<interface::bitcoind_wallet>;

} // namespace server

// Explicit instantiation of subgroup dispatchers, from a namespace enclosing
// network::rpc (as required and declared in subgroup headers).
template class network::rpc::dispatcher<
    server::interface::bitcoind_blockchain>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_control>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_handshake>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_mining>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_network>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_notifications>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_test>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_transaction>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_utility>;
template class network::rpc::dispatcher<
    server::interface::bitcoind_wallet>;

} // namespace libbitcoin
