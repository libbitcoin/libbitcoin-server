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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_NATIVE_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_NATIVE_HPP

#include <atomic>
#include <memory>
#include <optional>
#include <bitcoin/server/channels/channels.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_html.hpp>

namespace libbitcoin {
namespace server {

class BCS_API protocol_native
  : public protocol_html,
    protected network::tracker<protocol_native>
{
public:
    typedef std::shared_ptr<protocol_native> ptr;
    using interface = server::interface::native;
    using dispatcher = network::rpc::dispatcher<interface>;

    inline protocol_native(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : protocol_html(session, channel, options),
        network::tracker<protocol_native>(session->log)
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

    /// Dispatch.
    bool try_dispatch_object(
        const network::http::request& request) NOEXCEPT override;

    /// REST interface handlers.
    /// -----------------------------------------------------------------------

    bool handle_get_configuration(const code& ec, interface::configuration,
        uint8_t version, uint8_t media) NOEXCEPT;

    bool handle_get_top(const code& ec, interface::top,
        uint8_t version, uint8_t media) NOEXCEPT;

    bool handle_get_block(const code& ec, interface::block,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height, bool witness) NOEXCEPT;
    bool handle_get_block_header(const code& ec, interface::block_header,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_header_context(const code& ec, interface::block_header_context,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_details(const code& ec, interface::block_details,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_txs(const code& ec, interface::block_txs,
        uint8_t version, uint8_t media, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_filter(const code& ec, interface::block_filter,
        uint8_t version, uint8_t media, uint8_t type,
        std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_filter_hash(const code& ec,
        interface::block_filter_hash, uint8_t version, uint8_t media,
        uint8_t type, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_filter_header(const code& ec,
        interface::block_filter_header, uint8_t version, uint8_t media,
        uint8_t type, std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height) NOEXCEPT;
    bool handle_get_block_tx(const code& ec, interface::block_tx,
        uint8_t version, uint8_t media, uint32_t position,
        std::optional<system::hash_cptr> hash,
        std::optional<uint32_t> height, bool witness) NOEXCEPT;

    bool handle_get_tx(const code& ec, interface::tx,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        bool witness) NOEXCEPT;
    bool handle_get_tx_header(const code& ec, interface::tx_header,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_tx_details(const code& ec, interface::tx_details,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash) NOEXCEPT;

    bool handle_get_inputs(const code& ec, interface::inputs,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        bool witness) NOEXCEPT;
    bool handle_get_input(const code& ec, interface::input,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index, bool witness) NOEXCEPT;
    bool handle_get_input_script(const code& ec, interface::input_script,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_input_witness(const code& ec, interface::input_witness,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;

    bool handle_get_outputs(const code& ec, interface::outputs,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash) NOEXCEPT;
    bool handle_get_output(const code& ec, interface::output,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_output_script(const code& ec, interface::output_script,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_output_spender(const code& ec, interface::output_spender,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;
    bool handle_get_output_spenders(const code& ec, interface::output_spenders,
        uint8_t version, uint8_t media, const system::hash_cptr& hash,
        uint32_t index) NOEXCEPT;

    bool handle_get_address(const code& ec, interface::address,
        uint8_t version, uint8_t media,
        const system::hash_cptr& hash, bool turbo) NOEXCEPT;
    bool handle_get_address_confirmed(const code& ec,
        interface::address_confirmed, uint8_t version, uint8_t media,
        const system::hash_cptr& hash, bool turbo) NOEXCEPT;
    bool handle_get_address_unconfirmed(const code& ec,
        interface::address_unconfirmed, uint8_t version, uint8_t media,
        const system::hash_cptr& hash, bool turbo) NOEXCEPT;
    bool handle_get_address_balance(const code& ec,
        interface::address_balance, uint8_t version, uint8_t media,
        const system::hash_cptr& hash, bool turbo) NOEXCEPT;

private:
    // Serializers.
    // ------------------------------------------------------------------------
    // private/static

    template <typename Object, typename ...Args>
    inline static system::data_chunk to_bin(const Object& object, size_t size,
        Args&&... args) NOEXCEPT;
    template <typename Object, typename ...Args>
    inline static std::string to_hex(const Object& object, size_t size,
        Args&&... args) NOEXCEPT;
    template <typename Collection, typename ...Args>
    inline static system::data_chunk to_bin_array(const Collection& collection,
        size_t size, Args&&... args) NOEXCEPT;
    template <typename Collection, typename ...Args>
    inline static std::string to_hex_array(const Collection& collection,
        size_t size, Args&&... args) NOEXCEPT;
    template <typename Collection, typename ...Args>
    inline static system::data_chunk to_bin_ptr_array(
        const Collection& collection, size_t size, Args&&... args) NOEXCEPT;
    template <typename Collection, typename ...Args>
    inline static std::string to_hex_ptr_array(const Collection& collection,
        size_t size, Args&&... args) NOEXCEPT;

    // Completion handlers (for asynchronous query).
    // ------------------------------------------------------------------------

    void do_get_address(uint8_t media, bool turbo,
        const system::hash_cptr& hash) NOEXCEPT;
    void do_get_address_confirmed(uint8_t media, bool turbo,
        const system::hash_cptr& hash) NOEXCEPT;
    ////void do_get_address_unconfirmed(uint8_t media, bool turbo,
    ////    const system::hash_cptr& hash) NOEXCEPT;
    void complete_get_address(const code& ec, uint8_t media,
        const database::outpoints& set) NOEXCEPT;

    void do_get_address_balance(uint8_t media, bool turbo,
        const system::hash_cptr& hash) NOEXCEPT;
    void complete_get_address_balance(const code& ec, uint8_t media,
        const uint64_t balance) NOEXCEPT;

    // Utilities.
    // ------------------------------------------------------------------------

    void inject(boost::json::value& out, std::optional<uint32_t> height,
        const database::header_link& link) const NOEXCEPT;

    database::header_link to_header(const std::optional<uint32_t>& height,
        const std::optional<system::hash_cptr>& hash) NOEXCEPT;

    // This is thread safe.
    std::atomic_bool stopping_{};

    // This is protected by strand.
    dispatcher dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#include <bitcoin/server/impl/protocols/protocol_native.ipp>

#endif
