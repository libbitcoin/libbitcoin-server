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
#include <bitcoin/server/protocols/protocol_btcd.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/serializers/serializers.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_btcd

// protocol_bitcoind declares 'using post = network::http::method::post',
// which shadows network::protocol::post<Derived>. Qualify explicitly.
#define POST_BTCD(method, ...) \
    this->network::protocol::template post<CLASS>(&CLASS::method, __VA_ARGS__)

using namespace system;
using namespace network;
using namespace network::rpc;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

// Handlers (getters).
// ----------------------------------------------------------------------------

bool protocol_btcd::handle_estimate_fee(const code& ec,
    btcd_interface::estimate_fee, double numblocks) NOEXCEPT
{
    if (stopped(ec))
        return false;

    size_t target{};
    if (!to_integer(target, numblocks) || is_zero(target) ||
        target > node::estimator::maximum_horizon)
    {
        send_error(error::btcd::invalid_params);
        return true;
    }

    // The estimator targets the next block as zero.
    estimate(sub1(target), node::estimator::mode::basic,
        BIND(handle_estimate, _1, _2));
    return true;
}

void protocol_btcd::handle_estimate(const code& ec, uint64_t fee) NOEXCEPT
{
    POST_BTCD(complete_estimate, ec, fee);
}

void protocol_btcd::complete_estimate(const code& ec, uint64_t fee) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    const auto unavailable =
        ec == node::error::estimate_false ||
        ec == node::error::estimate_disabled ||
        ec == node::error::estimate_premature;

    // btcd reports estimate unavailability as an error (-1).
    if (unavailable)
    {
        send_error(error::btcd::misc_error);
        return;
    }

    if (ec)
    {
        // node::error::estimates_failed, implies store fault.
        send_error(error::btcd::internal_error);
        return;
    }

    // sats/vbyte to btc/kvbyte.
    constexpr double fee_scale = 100'000.0;
    send_result(fee / fee_scale, 20);
}

// Required by btcwallet during wallet chain-sync bootstrap.
bool protocol_btcd::handle_get_best_block(const code& ec,
    btcd_interface::get_best_block) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    send_result(object_t
    {
        { "hash", encode_hash(query.get_top_confirmed_hash()) },
        { "height", query.get_top_confirmed() }
    }, 96);
    return true;
}

// Overrides the bitcoind method (btcd is attached first, so it claims the
// request). Softfork activation is configured, not assumable. Taproot is
// reported when configured active, with its configured activation height --
// lnd's backendSupportsTaproot requires the key's presence (not its field
// values) before treating any btcd backend as usable.
bool protocol_btcd::handle_get_block_chain_info(const code& ec,
    btcd_interface::get_block_chain_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    object_t out{};
    if (!chain_info(out, archive(), system_settings(),
        node_settings().limited_blocks, is_current_chain(true)))
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    const auto& settings = system_settings();
    object_t soft_forks{};
    if (settings.forks.bip341 && settings.forks.bip342)
    {
        soft_forks.emplace("taproot", object_t
        {
            { "status", std::string{ "active" } },
            { "bit", 2 },
            { "startTime", -1 },
            { "timeout", -1 },
            { "since", settings.bip9_bit2_active_checkpoint.height() },
            { "min_activation_height", 0 }
        });
    }

    out.emplace("bip9_softforks", std::move(soft_forks));
    send_result(std::move(out), 512);
    return true;
}

bool protocol_btcd::handle_get_cfilter(const code& ec,
    btcd_interface::get_cfilter, const std::string& hash,
    double filtertype) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest key{};
    if (!decode_hash(key, hash))
    {
        send_error(error::btcd::deserialization_error);
        return true;
    }

    uint8_t type{};
    data_chunk filter{};
    const auto& query = archive();
    if (!to_integer(type, filtertype) || !is_zero(type) ||
        !query.filter_enabled() ||
        !query.get_filter_body(filter, query.to_header(key)))
    {
        send_error(error::btcd::invalid_address_or_key);
        return true;
    }

    send_result(encode_base16(filter), two * filter.size());
    return true;
}

bool protocol_btcd::handle_get_cfilter_header(const code& ec,
    btcd_interface::get_cfilter_header, const std::string& hash,
    double filtertype) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hash_digest key{};
    if (!decode_hash(key, hash))
    {
        send_error(error::btcd::deserialization_error);
        return true;
    }

    uint8_t type{};
    hash_digest head{};
    const auto& query = archive();
    if (!to_integer(type, filtertype) || !is_zero(type) ||
        !query.filter_enabled() ||
        !query.get_filter_head(head, query.to_header(key)))
    {
        send_error(error::btcd::invalid_address_or_key);
        return true;
    }

    send_result(encode_hash(head), two * hash_size);
    return true;
}

// p2p magic (checked once by btcwallet/lnd to confirm network).
bool protocol_btcd::handle_get_current_net(const code& ec,
    btcd_interface::get_current_net) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(network_settings().identifier, 20);
    return true;
}

bool protocol_btcd::handle_get_difficulty(const code& ec,
    btcd_interface::get_difficulty) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    send_result(header->difficulty(), 20);
    return true;
}

bool protocol_btcd::handle_get_headers(const code& ec,
    btcd_interface::get_headers, const array_t& blocklocators,
    const std::string& hashstop) NOEXCEPT
{
    if (stopped(ec))
        return false;

    hashes locator{};
    hash_digest hash{};
    for (const auto& item: blocklocators)
    {
        if (!std::holds_alternative<string_t>(item.value()) ||
            !decode_hash(hash, std::get<string_t>(item.value())))
        {
            send_error(error::btcd::deserialization_error);
            return true;
        }

        locator.push_back(hash);
    }

    auto stop = null_hash;
    if (!hashstop.empty() && !decode_hash(stop, hashstop))
    {
        send_error(error::btcd::deserialization_error);
        return true;
    }
    
    constexpr auto max = messages::peer::max_get_headers;
    const auto headers = archive().get_headers(locator, stop, max);

    array_t out{};
    out.reserve(headers.size());
    for (const auto& header: headers)
        out.emplace_back(encode_base16(header->to_data()));

    send_result(std::move(out), add1(headers.size()) * 164);
    return true;
}

bool protocol_btcd::handle_get_info(const code& ec,
    btcd_interface::get_info) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    const auto header = query.get_header(query.to_confirmed(top));
    if (!header)
    {
        send_error(error::btcd::internal_error);
        return true;
    }

    constexpr auto version =
        1'000'000 * to_value(btcd::version::major) +
        10'000 * to_value(btcd::version::minor) +
        100 * to_value(btcd::version::patch);

    send_result(object_t
    {
        { "version", version },
        { "protocolversion", network_settings().protocol_maximum },
        { "blocks", top },
        { "timeoffset", 0 },
        { "connections", channel_count() },
        { "proxy", std::string{} },
        { "difficulty", header->difficulty() },
        { "testnet", chain_name(query) != "main" },
        { "relayfee", node_settings().minimum_fee_rate },
        { "errors", std::string{} }
    }, 256);
    return true;
}

bool protocol_btcd::handle_get_net_totals(const code& ec,
    btcd_interface::get_net_totals) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(object_t
    {
        { "totalbytesrecv", 0 },
        { "totalbytessent", 0 },
        { "timemillis", possible_wide_cast<int64_t>(zulu_time()) * 1'000 }
    }, 64);
    return true;
}

bool protocol_btcd::handle_version(const code& ec,
    btcd_interface::version) NOEXCEPT
{
    if (stopped(ec))
        return false;

    send_result(object_t
    {
        { "btcdjsonrpcapi", object_t
            {
                { "versionstring", std::string{ "1.3.0" } },
                { "major", 1 },
                { "minor", 3 },
                { "patch", 0 },
                { "prerelease", std::string{} },
                { "buildmetadata", std::string{} }
            } }
    }, 160);
    return true;
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
