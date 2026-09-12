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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ESPLORA_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_ESPLORA_HPP

#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_http.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_esplora
  : public server::protocol_http,
    protected network::tracker<protocol_esplora>
{
public:
    typedef std::shared_ptr<protocol_esplora> ptr;
    using interface = server::interface::esplora;
    using dispatcher = network::rpc::dispatcher<interface>;
    using options_t = server::settings::esplora_server;
    using channel_t = channel_html;

    inline protocol_esplora(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_http(session, channel, options),
        network::tracker<protocol_esplora>(session->log),
        p2kh_(session->server_settings().wallet.p2kh_prefix),
        p2sh_(session->server_settings().wallet.p2sh_prefix),
        flags_(session->system_settings().flags()),
        witness_(session->server_settings().wallet.witness_prefix)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    template <class Derived, typename Method, typename... Args>
    inline void subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    /// Message handlers by http method.
    void handle_receive_get(const code& ec,
        const network::http::method::get::cptr& get) NOEXCEPT override;

    /// Dispatch.
    virtual bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT;
    void dispatch_websocket(
        const network::http::request& request) NOEXCEPT override;

    /// Senders.
    virtual void send_json(boost::json::value&& model, size_t size_hint,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_text(std::string&& text,
        const network::http::request& request={}) NOEXCEPT;
    virtual void send_chunk(system::data_chunk&& bytes,
        const network::http::request& request={}) NOEXCEPT;

    /// Interface handlers.
    /// -----------------------------------------------------------------------

    bool handle_get_tx(const code& ec, interface::tx,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_tx_status(const code& ec, interface::tx_status,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_tx_merkle_proof(const code& ec, interface::tx_merkle_proof,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_tx_outspend(const code& ec, interface::tx_outspend,
        uint8_t media, const system::hash_cptr& hash, uint32_t index) NOEXCEPT;
    bool handle_get_tx_outspends(const code& ec, interface::tx_outspends,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;

    bool handle_get_block(const code& ec, interface::block,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_block_txs(const code& ec, interface::block_txs,
        uint8_t media, const system::hash_cptr& hash, uint32_t start) NOEXCEPT;
    bool handle_get_block_header(const code& ec, interface::block_header,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_block_status(const code& ec, interface::block_status,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_block_txids(const code& ec, interface::block_txids,
        uint8_t media, const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_block_txid(const code& ec, interface::block_txid,
        uint8_t media, const system::hash_cptr& hash, uint32_t index) NOEXCEPT;
    bool handle_get_block_height(const code& ec, interface::block_height,
        uint8_t media, uint32_t height) NOEXCEPT;
    bool handle_get_blocks(const code& ec, interface::blocks,
        uint8_t media, std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_tip_height(const code& ec, interface::tip_height,
        uint8_t media) NOEXCEPT;
    bool handle_get_tip_hash(const code& ec, interface::tip_hash,
        uint8_t media) NOEXCEPT;

    bool handle_get_mempool(const code& ec, interface::mempool,
        uint8_t media) NOEXCEPT;
    bool handle_get_mempool_txids(const code& ec, interface::mempool_txids,
        uint8_t media) NOEXCEPT;
    bool handle_get_mempool_recent(const code& ec, interface::mempool_recent,
        uint8_t media) NOEXCEPT;
    bool handle_get_fee_estimates(const code& ec, interface::fee_estimates,
        uint8_t media) NOEXCEPT;

private:
    using media_type = network::http::media_type;
    static constexpr uint8_t json = to_value(media_type::application_json);

    static bool is_implemented(const std::string& method) NOEXCEPT;

    // Serializers.
    // ------------------------------------------------------------------------

    static std::string to_script_type(
        const system::chain::script& script) NOEXCEPT;
    boost::json::object to_output(
        const system::chain::output& output) NOEXCEPT;
    boost::json::object to_input(const system::chain::input& input,
        bool coinbase) NOEXCEPT;
    boost::json::object to_status(const database::tx_link& link) NOEXCEPT;
    bool to_tx(boost::json::object& out,
        const database::tx_link& link) NOEXCEPT;
    bool to_outspend(boost::json::object& out,
        const system::hash_digest& hash, uint32_t index) NOEXCEPT;
    bool to_block(boost::json::object& out,
        const database::header_link& link) NOEXCEPT;

    // Completion handlers (for asynchronous query).
    // ------------------------------------------------------------------------

    void next_estimate(size_t index) NOEXCEPT;
    void handle_estimate(const code& ec, uint64_t fee, size_t index) NOEXCEPT;
    void complete_estimate(const code& ec, uint64_t fee,
        size_t index) NOEXCEPT;
    // These are thread safe.
    const uint8_t p2kh_;
    const uint8_t p2sh_;
    const uint32_t flags_;
    const std::string witness_;

    // These are protected by strand.
    boost::json::object estimates_{};
    dispatcher dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
