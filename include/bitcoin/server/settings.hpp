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
#ifndef LIBBITCOIN_SERVER_SETTINGS_HPP
#define LIBBITCOIN_SERVER_SETTINGS_HPP

#include <filesystem>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace log {

/// [log] settings.
class BCS_API settings
{
public:
    DEFAULT_COPY_MOVE_DESTRUCT(settings);

    settings() NOEXCEPT;
    settings(system::chain::selection context) NOEXCEPT;

    bool application;
    bool news;
    bool session;
    bool protocol;
    bool proxy;
    bool remote;
    bool fault;
    bool quitting;
    bool objects;
    bool verbose;

    uint32_t maximum_size;
    std::filesystem::path path;

#if defined (HAVE_MSC)
    std::filesystem::path symbols;
#endif

    virtual std::filesystem::path log_file1() const NOEXCEPT;
    virtual std::filesystem::path log_file2() const NOEXCEPT;
    virtual std::filesystem::path events_file() const NOEXCEPT;
};

} // namespace log

namespace server {

// HACK: must cast writer to non-const.
using span_value = network::http::span_body::value_type;

/// [server] settings.
class BCS_API settings
{
public:
    /// Names of settings queried for explicit configuration.
    static constexpr auto milestone = "bitcoin.milestone";

    /// References to process embeded resources for html_server.
    struct embedded_pages
    {
        DEFAULT_COPY_MOVE_DESTRUCT(embedded_pages);
        embedded_pages() = default;

        virtual span_value css() const NOEXCEPT;
        virtual span_value html() const NOEXCEPT;
        virtual span_value ecma() const NOEXCEPT;
        virtual span_value font() const NOEXCEPT;
        virtual span_value icon() const NOEXCEPT;

        /// At least the html page is required to load embedded site.
        virtual bool enabled() const NOEXCEPT;
    };

    struct electrum_server
      : public network::settings::tls_server
    {
        using base = network::settings::tls_server;
        using base::base;

        /// Maximum number of headers the server will return in single request.
        /// Recommended to be a multiple of the difficulty retarget period.
        uint32_t maximum_headers{ 10 * 2016 };

        /// Maximum number of address history entries upon one subscription.
        uint32_t maximum_history{ 1'000'000 };

        /// Maximum cumulative number of address subscriptions per channel.
        uint32_t maximum_subscriptions{ 1'000'000 };

        /// Minimum protocol version.
        system::config::version protocol_minimum{ 1, 0, 0, 0 };

        /// Maximum protocol version.
        system::config::version protocol_maximum{ 1, 7, 0, 0 };

        /// Arbitrary name returned by server.version and server.features.
        std::string server_name{ BC_USER_AGENT };

        /// Arbitrary string returned by server.donation_address.
        std::string donation_address{};

        /// Arbitrary string returned by server.banner.
        std::string banner_message{};

        /// Advertised self via server.features (limit one each per host).
        network::config::endpoints self_binds{};
        network::config::endpoints self_safes{};

        /// Advertised servers via server.peers.subscribe.
        network::config::endpoints more_binds{};
        network::config::endpoints more_safes{};
    };

    /// html (http/s) document server settings (has directory/default).
    /// This is for web servers that expose a local file system directory.
    struct html_server
      : public network::settings::websocket_server
    {
        // tls_server and embedded_pages reference preclude copy.
        DELETE_COPY(html_server);

        html_server(const std::string_view& logging_name,
            const embedded_pages& embedded) NOEXCEPT;

        /// Embeded single page with html, css, js, favicon resource.
        /// This is a reference to the caller's resource (retained instance).
        const embedded_pages& pages;

        /// Set false to disable http->websocket http upgrade processing.
        bool websocket{ true };

        /// Directory to serve.
        std::filesystem::path path{};

        /// Default page for default URL (recommended).
        std::string default_{ "index.html" };

        /// !path.empty() && http_server::enabled() [hidden, not virtual]
        virtual bool enabled() const NOEXCEPT;
    };

    /// Address encoding, implied by coin and network (not by forks).
    struct wallet_settings
    {
        DEFAULT_COPY_MOVE_DESTRUCT(wallet_settings);

        wallet_settings() NOEXCEPT;
        wallet_settings(system::chain::selection context) NOEXCEPT;

        system::config::byte p2kh_prefix;
        system::config::byte p2sh_prefix;
        std::string witness_prefix;
    };

    struct bitcoind_server
      : public network::settings::http_server
    {
        using base = network::settings::http_server;
        using base::base;

        /// Version identity returned by getnetworkinfo.
        system::config::version version{};
        std::string subversion{ "/libbitcoin:server/" };
    };

    struct btcd_server
      : public network::settings::http_server
    {
        using base = network::settings::http_server;
        using base::base;

        /// Maximum cumulative number of loadtxfilter watches per channel.
        uint32_t maximum_filters{ 1'000'000 };

        /// Maximum number of address history entries upon one index walk.
        uint32_t maximum_history{ 1'000'000 };
    };

    // html_server precludes copy.
    DELETE_COPY(settings);

    settings(system::chain::selection context, const embedded_pages& native,
        const embedded_pages& admin) NOEXCEPT;

    /// address encoding (coin/network identity)
    wallet_settings wallet;

    /// admin web interface, isolated (http/s, stateless html)
    server::settings::html_server admin;

    /// RESTful block explorer (http/s, stateless html/websocket)
    server::settings::html_server native;

    /// bitcoind compat interface (http/s, stateless json-rpc-v2)
    bitcoind_server bitcoind{ "bitcoind" };

    /// btcd compat interface (http/s + websocket, json-rpc-v2 [no positional])
    btcd_server btcd{ "btcd" };

    /// electrum compat interface (tcp/s, json-rpc-v2)
    electrum_server electrum{ "electrum" };

    /// stratum v1 compat interface (tcp/s, json-rpc-v1, auth handshake)
    network::settings::tls_server stratum_v1{ "stratum_v1" };

    /// stratum vs is not TLS, but normalized for session_server usage.
    /// stratum v2 compat interface (tcp[/s], binary, auth/privacy handshake)
    network::settings::tls_server stratum_v2{ "stratum_v2" };
};

} // namespace server
} // namespace libbitcoin

#endif
