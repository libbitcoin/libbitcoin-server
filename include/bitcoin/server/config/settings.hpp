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
#ifndef LIBBITCOIN_SERVER_SETTINGS_HPP
#define LIBBITCOIN_SERVER_SETTINGS_HPP

#include <cstdint>
#include <boost/filesystem.hpp>
#include <bitcoin/node.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

struct BCS_API settings
{
    config::endpoint query_endpoint;
    config::endpoint heartbeat_endpoint;
    config::endpoint block_publish_endpoint;
    config::endpoint transaction_publish_endpoint;
    bool publisher_enabled;
    bool queries_enabled;
    bool log_requests;
    uint32_t polling_interval_seconds;
    uint32_t heartbeat_interval_seconds;
    uint32_t subscription_expiration_minutes;
    uint32_t subscription_limit;
    boost::filesystem::path certificate_file;
    boost::filesystem::path client_certificates_path;
    config::authority::list whitelists;

    asio::duration polling_interval() const
    {
        // This should eventually be returned to milliseconds.
        // It was changed to seconds as a quick hack, since ms is harder.
        return asio::duration(0, 0, polling_interval_seconds);
    }

    asio::duration heartbeat_interval() const
    {
        return asio::duration(0, 0, heartbeat_interval_seconds);
    }

    asio::duration subscription_expiration() const
    {
        return asio::duration(0, subscription_expiration_minutes, 0);
    }
};

} // namespace server
} // namespace libbitcoin

#endif
