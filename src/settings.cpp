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
#include <bitcoin/server/settings.hpp>

#include <filesystem>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/utilities/utilities.hpp>

using namespace bc::system;
using namespace bc::network;

namespace libbitcoin {

// log
// ----------------------------------------------------------------------------

namespace log {

// Log states default to network compiled states or explicit false.
settings::settings() NOEXCEPT
  : application{ levels::application_defined },
    news{ levels::news_defined },
    session{ levels::session_defined },
    protocol{ false /*levels::protocol_defined*/ },
    proxy{ false /*levels::proxy_defined*/ },
    remote{ levels::remote_defined },
    fault{ levels::fault_defined },
    quitting{ false /*levels::quitting_defined*/ },
    objects{ false /*levels::objects_defined*/ },
    verbose{ false /*levels::verbose_defined*/ },
    console{ true },
    events{ true },
    thread_priority{ false },
    limit{ 1'000'000_u32 }
{
}

settings::settings(chain::selection) NOEXCEPT
  : settings()
{
}

processing_priority settings::thread_priority_() const NOEXCEPT
{
    // medium is "normal" (os default).
    return thread_priority ? processing_priority::medium :
        processing_priority::low;
}

std::filesystem::path settings::log_file1() const NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return console_path / "console_new.log";
    BC_POP_WARNING()
}

std::filesystem::path settings::log_file2() const NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return console_path / "console_old.log";
    BC_POP_WARNING()
}

std::filesystem::path settings::events_file() const NOEXCEPT
{
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
    return events_path / "events.log";
    BC_POP_WARNING()
}

} // namespace log

// server
// ----------------------------------------------------------------------------

namespace server {

using namespace system::wallet;

// settings::settings
settings::settings(system::chain::selection context,
    const embedded_pages& native_pages,
    const embedded_pages& admin_pages) NOEXCEPT
  : network(context),
    wallet(context),
    admin("admin", admin_pages),
    native("native", native_pages)
{
    // Admin is enabled by default, for the local machine only.
    admin.connections = 10;
    admin.binds.emplace_back(system::ip::ipv4::loopback(), 0_u16);

    // Admin may accumulate a large amount of log backlog.
    admin.maximum_backlog = 1024 * network::megabyte;

    // Admin sessions are held open for monitoring, so they do not expire.
    admin.inactivity_minutes = 0;
    admin.expiration_minutes = 0;

    // The largest btcd notification is a filtered block.
    btcd.maximum_backlog = 20 * network::megabyte;

    // btcd reports json-rpc errors in the response body only.
    btcd.error_status = false;

    // Raw block publications drop beyond the bound, eight blocks deep.
    zmq.maximum_backlog = 32 * network::megabyte;

    // Subscribers send nothing after subscribing, so publishers do not expire.
    zmq.inactivity_minutes = 0;
    zmq.expiration_minutes = 0;

    // Miners hold channels open and may go silent, so they do not expire.
    stratum_v1.inactivity_minutes = 0;
    stratum_v1.expiration_minutes = 0;
    stratum_v2.inactivity_minutes = 0;
    stratum_v2.expiration_minutes = 0;

    // A submitted block is base16 encoded within a json envelope.
    constexpr auto encoded = 2 * network::max_payload + 4 * network::kilobyte;
    bitcoind.maximum_request = encoded;
    btcd.maximum_request = encoded;

    // A broadcast transaction package is base16 encoded within a json envelope.
    electrum.maximum_request = encoded;
    sparrow.maximum_request = encoded;
    esplora.maximum_request = encoded;
}

// The service contexts are read from configured files, once, at startup.
code settings::initialize() NOEXCEPT
{
    // Service ports are offset by chain.
    const auto port = [this](uint16_t base) NOEXCEPT
    {
        switch (network)
        {
            case system::chain::selection::testnet3:
                return possible_narrow_cast<uint16_t>(base + 10000_u32);
            case system::chain::selection::regtest:
                return possible_narrow_cast<uint16_t>(base + 20000_u32);
            case system::chain::selection::signet:
                return possible_narrow_cast<uint16_t>(base + 30000_u32);
            case system::chain::selection::testnet4:
                return possible_narrow_cast<uint16_t>(base + 40000_u32);
            case system::chain::selection::testnet5:
                return possible_narrow_cast<uint16_t>(base + 50000_u32);
            default:
                return base;
        }
    };

    admin.bind_port = port(8080);
    admin.safe_port = port(8043);
    native.bind_port = port(8180);
    native.safe_port = port(8143);
    bitcoind.bind_port = port(8280);
    bitcoind.safe_port = port(8243);
    zmq.bind_port = port(8380);
    zmq.safe_port = port(8343);
    btcd.bind_port = port(8480);
    btcd.safe_port = port(8443);
    electrum.bind_port = port(8580);
    electrum.safe_port = port(8543);
    sparrow.bind_port = port(8680);
    sparrow.safe_port = port(8643);
    esplora.bind_port = port(8780);
    esplora.safe_port = port(8743);
    stratum_v1.bind_port = port(8880);
    stratum_v1.safe_port = port(8843);
    stratum_v2.bind_port = port(8980);

    // Service bindings without a port take the service's default port.
    admin.default_ports();
    native.default_ports();
    bitcoind.default_ports();
    zmq.default_ports();
    btcd.default_ports();
    electrum.default_ports();
    sparrow.default_ports();
    esplora.default_ports();
    stratum_v1.default_ports();
    stratum_v2.default_ports();

    admin.default_buffers();
    native.default_buffers();
    bitcoind.default_buffers();
    zmq.default_buffers();
    btcd.default_buffers();
    electrum.default_buffers();
    sparrow.default_buffers();
    esplora.default_buffers();
    stratum_v1.default_buffers();
    stratum_v2.default_buffers();

    const auto initialize = [this](auto& service) NOEXCEPT
    {
        const auto ec = service.initialize_context();
        if (ec)
            failure = service.name;

        return ec;
    };

    if (const auto ec = initialize(admin)) return ec;
    if (const auto ec = initialize(native)) return ec;
    if (const auto ec = initialize(bitcoind)) return ec;
    if (const auto ec = initialize(btcd)) return ec;
    if (const auto ec = initialize(electrum)) return ec;
    if (const auto ec = initialize(sparrow)) return ec;
    if (const auto ec = initialize(esplora)) return ec;
    if (const auto ec = initialize(stratum_v1)) return ec;
    if (const auto ec = initialize(stratum_v2)) return ec;
    return initialize(zmq);
}

// settings::wallet_settings
settings::wallet_settings::wallet_settings() NOEXCEPT
  : wallet_settings(system::chain::selection::mainnet)
{
}

static const context& to_wallet_context(
    system::chain::selection context) NOEXCEPT
{
    switch (context)
    {
        case system::chain::selection::testnet3:
        case system::chain::selection::testnet4:
        case system::chain::selection::testnet5:
        case system::chain::selection::signet:
            return ctx::btc::test;
        case system::chain::selection::regtest:
            return ctx::btc::regtest;
        case system::chain::selection::mainnet:
        case system::chain::selection::none:
        default:
            return ctx::btc::main;
    }
}

settings::wallet_settings::wallet_settings(
    system::chain::selection context) NOEXCEPT
{
    const auto& value = to_wallet_context(context);
    p2kh_prefix = value.p2kh;
    p2sh_prefix = value.p2sh;
    wif_prefix = value.wif;
    witness_prefix = value.p2w;
    silent_prefix = value.sp;
    hd_private_prefix = value.hd.prv;
    hd_public_prefix = value.hd.pub;
}

context settings::wallet_settings::to_context() const NOEXCEPT
{
    return
    {
        { hd_private_prefix, hd_public_prefix },
        witness_prefix,
        p2kh_prefix,
        p2sh_prefix,
        wif_prefix,
        silent_prefix
    };
}

// settings::embedded_pages
span_value settings::embedded_pages::css()  const NOEXCEPT { return {}; }
span_value settings::embedded_pages::html() const NOEXCEPT { return {}; }
span_value settings::embedded_pages::ecma() const NOEXCEPT { return {}; }
span_value settings::embedded_pages::font() const NOEXCEPT { return {}; }
span_value settings::embedded_pages::icon() const NOEXCEPT { return {}; }
bool settings::embedded_pages::enabled() const NOEXCEPT
{
    return !html().empty();
}

// settings::html_server
settings::html_server::html_server(const std::string_view& logging_name,
    const embedded_pages& embedded) NOEXCEPT
  : network::settings::websocket_server(logging_name),
    pages(embedded)
{
}

// A weak tag, as each page may be sent in any content coding.
code settings::html_server::initialize_context() NOEXCEPT
{
    if (const auto ec = base::initialize_context())
        return ec;

    const auto tag = [](const span_value& page) NOEXCEPT -> std::string
    {
        if (page.empty())
            return {};

        const auto hash = accumulator<sha256>::hash(page.size(), page.data());
        return "W/\"" + encode_base16(hash) + "\"";
    };

    tags_ =
    {
        tag(pages.css()),
        tag(pages.html()),
        tag(pages.ecma()),
        tag(pages.font()),
        tag(pages.icon())
    };

    return network::error::success;
}

const settings::html_server::page_tags&
settings::html_server::tags() const NOEXCEPT
{
    return tags_;
}

// settings::bitcoind_server
code settings::bitcoind_server::initialize_context() NOEXCEPT
{
    using namespace bitcoind;
    if (const auto ec = base::initialize_context())
        return ec;

    // Range bounded by implemented versions,
    // maximum floors to implemented version within range.
    if (protocol_minimum < minimum_version ||
        protocol_maximum > maximum_version ||
        to_value(version_floor(protocol_maximum)) < protocol_minimum)
        return network::error::invalid_configuration;

    if (enable_utxo)
    {
        inactivity_minutes = 0;
        expiration_minutes = 0;
    }

    return network::error::success;
}

// Tag and extranonce are each a single push within the coinbase script, and
// the extranonce1 of channels is distinct below 2^32 channel identifiers.
code settings::stratum_v1_server::initialize_context() NOEXCEPT
{
    using namespace system::chain;
    if (const auto ec = base::initialize_context())
        return ec;

    const auto extranonce = add<size_t>(extranonce1_size, extranonce2_size);
    const auto script = coinbase_script_size(coinbase_tag.size(), extranonce);
    if (extranonce1_size < sizeof(uint32_t) || is_zero(extranonce2_size) ||
        coinbase_tag.size() > max_direct_push ||
        extranonce > max_direct_push || script > max_coinbase_size ||
        minimum_difficulty <= 0.0)
        return network::error::invalid_configuration;

    return network::error::success;
}

code settings::esplora_server::initialize_context() NOEXCEPT
{
    using namespace server::esplora;
    if (const auto ec = base::initialize_context())
        return ec;

    // The version floors to an implemented version.
    if (protocol_version < minimum_version ||
        protocol_version > maximum_version)
        return network::error::invalid_configuration;

    return network::error::success;
}

} // namespace server

// ----------------------------------------------------------------------------

} // namespace libbitcoin
