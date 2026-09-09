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

    /// The silent payment (bip352) protocol version served (as frigate).
    static constexpr uint32_t silent_payments_version{ 0 };

    inline protocol_sparrow(const auto& session,
        const network::channel::ptr& channel,
        const options_t& options) NOEXCEPT
      : server::protocol_electrum(session, channel, options),
        network::tracker<protocol_sparrow>(session->log)
    {
    }

    void start() NOEXCEPT override;
    void stopping(const code& ec) NOEXCEPT override;

protected:
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

    // This is protected by strand.
    sparrow_dispatcher sparrow_dispatcher_{};
};

} // namespace server
} // namespace libbitcoin

#endif
