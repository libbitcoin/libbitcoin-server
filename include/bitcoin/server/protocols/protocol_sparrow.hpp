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
#ifndef LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SPARROW_HPP
#define LIBBITCOIN_SERVER_PROTOCOLS_PROTOCOL_SPARROW_HPP

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/interfaces/interfaces.hpp>
#include <bitcoin/server/protocols/protocol_electrum.hpp>

namespace libbitcoin {
namespace server {

/// The sparrow interface, added to the inherited electrum interface, as
/// protocol_btcd adds the btcd interface to bitcoind.
class BCS_API protocol_sparrow
  : public server::protocol_electrum,
    protected network::tracker<protocol_sparrow>
{
public:
    typedef std::shared_ptr<protocol_sparrow> ptr;
    using sparrow_interface = interface::sparrow;
    using sparrow_dispatcher = network::rpc::dispatcher<sparrow_interface>;

    /// The silent payment (bip352) protocol version served.
    static constexpr uint32_t silent_payments_version{ 0 };

    inline protocol_sparrow(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_electrum(session, channel, options),
        network::tracker<protocol_sparrow>(session->log),
        prefix_(session->server_settings().wallet.silent_prefix)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
    using tx_link_t = system::silent::batch::tx_link_t;
    using silent_payment = system::wallet::silent_payment;
    using matches = std::map<tx_link_t, system::ec_compressed>;

    // Confirmed matches below start (height or time) or above stop are cut.
    struct silent_cut final
    {
        size_t start{};
        size_t stop{ max_size_t };
        bool time{};
    };

    // Subscription to a silent payment address.
    struct silent_subscription final
    {
        silent_payment scanner;
        object_t subscription{};
        silent_cut cut{};
        size_t cursor{};
    };

    /// Events, adds silent payment notification.
    bool handle_chase(const code& ec,
        node::event_value value) NOEXCEPT override;

    /// Dispatched from the electrum miss, so that interface is unaffected.
    void handle_unclaimed(
        const network::rpc::request_t& request) NOEXCEPT override;

    /// Advertise the supported silent payment protocol versions (bip352).
    void add_features(
        network::rpc::object_t& features) const NOEXCEPT override;

    /// Handlers.
    void handle_blockchain_block_stats(const code& ec,
        sparrow_interface::blockchain_block_stats, double height) NOEXCEPT;
    void handle_blockchain_silent_payments_subscribe(const code& ec,
        sparrow_interface::blockchain_silent_payments_subscribe,
        const std::string& scan_private_key,
        const std::string& spend_public_key,
        const interface::value_t& start,
        const interface::array_t& labels) NOEXCEPT;
    void handle_blockchain_silent_payments_unsubscribe(const code& ec,
        sparrow_interface::blockchain_silent_payments_unsubscribe,
        const std::string& scan_private_key,
        const std::string& spend_public_key) NOEXCEPT;

private:
    template <class Derived, typename Method, typename... Args>
    inline void sparrow_subscribe(Method&& method, Args&&... args) NOEXCEPT
    {
        sparrow_dispatcher_.subscribe(BIND_SHARED(method, args));
    }

    /// Notification event handlers.
    /// -----------------------------------------------------------------------

    void do_silent(node::header_t link) NOEXCEPT;

    /// Silent payment.
    /// -----------------------------------------------------------------------

    void do_silent_subscribe(const std::string& address,
        const silent_subscription& subscription,
        const gate_t::ptr& gate) NOEXCEPT;
    void complete_silent_subscribe(const code& ec, const object_t& result,
        const gate_t::ptr& gate) NOEXCEPT;
    void do_silent_unsubscribe(const std::string& address) NOEXCEPT;
    void complete_silent_unsubscribe(
        const network::rpc::value_t& result) NOEXCEPT;
    void silent_notify(const object_t& subscription, double progress,
        const array_t& history) NOEXCEPT;

    void scan_silent(silent_subscription& subscription, size_t last,
        bool initial) NOEXCEPT;
    array_t confirm_silent(const silent_subscription& subscription,
        const matches& found) const NOEXCEPT;
    bool is_cut(const silent_cut& cut, const tx_link_t& link,
        size_t height) const NOEXCEPT;

    /// Utility.
    /// -----------------------------------------------------------------------

    static bool to_cut(silent_cut& out,
        const interface::value_t& value) NOEXCEPT;
    static bool to_labels(std::vector<uint32_t>& out,
        const interface::array_t& labels) NOEXCEPT;
    std::string to_address(const system::ec_secret& scan,
        const system::ec_compressed& spend) const NOEXCEPT;

    // These are thread safe.
    const std::string prefix_;
    std::atomic_bool cancel_{};
    std::atomic_bool queued_silent_{};
    std::atomic_bool subscribed_silent_{};

    // This is protected by strand.
    sparrow_dispatcher sparrow_dispatcher_{};

    // These are protected by notification strand.
    std::map<std::string, silent_subscription> silent_subscriptions_{};
};

} // namespace server
} // namespace libbitcoin

#endif
