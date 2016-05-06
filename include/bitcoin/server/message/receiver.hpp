/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_SERVER_RECEIVIER_HPP
#define LIBBITCOIN_SERVER_RECEIVIER_HPP

#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <boost/date_time.hpp>
#include <czmq++/czmqpp.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/message/incoming.hpp>
#include <bitcoin/server/message/outgoing.hpp>
#include <bitcoin/server/message/sender.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>

namespace libbitcoin {
namespace server {

class BCS_API receiver
  : public enable_shared_from_base<receiver>
{
public:
    typedef std::shared_ptr<receiver> ptr;
    typedef std::function<void(const incoming&, send_handler)> command_handler;

    receiver(server_node::ptr node);

    /// This class is not copyable.
    receiver(const receiver&) = delete;
    void operator=(const receiver&) = delete;

    bool start();
    void poll();
    void attach(const std::string& command, command_handler handler);

private:
    typedef std::unordered_map<std::string, command_handler> command_map;

    void whitelist();
    bool enable_crypto();
    bool create_new_socket();
    void publish_heartbeat();

    uint32_t counter_;
    sender sender_;
    command_map handlers_;
    boost::posix_time::ptime deadline_;
    const settings& settings_;

    czmqpp::context context_;
    czmqpp::socket socket_;
    czmqpp::socket wakeup_socket_;
    czmqpp::socket heartbeat_socket_;
    czmqpp::certificate certificate_;
    czmqpp::authenticator authenticate_;
};

} // namespace server
} // namespace libbitcoin

#endif
