/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_ROUTE
#define LIBBITCOIN_SERVER_ROUTE

#include <cstddef>
#include <string>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// This class is not thread safe.
/// This simple route is limited in accordance with v2/v3 design.
/// It allows for only one address, optionally delimited for support of REQ
/// (delimited) and DEALER clients (with or without a delimiter). A REQ client
/// is synchronous so cannot receive notifications. A DEALER is asynchronous
/// and can be delimited or otherwise. We must match the delimiter so that the
/// undelimited DEALER (vs) will not fail.
class BCS_API route
{
public:
    /// Construct a default route.
    route();

    /// A printable address for logging only.
    std::string display() const;

    /// The message route is delimited using an empty frame.
    bool delimited() const;

    /// Set whether the address is delimited.
    void set_delimited(bool value);

    /// The simple route supports only one address.
    bc::protocol::zmq::message::address address() const;

    /// Set the address.
    void set_address(const bc::protocol::zmq::message::address& value);

protected:
    bool delimited_;
    bc::protocol::zmq::message::address address_;
};

} // namespace server
} // namespace libbitcoin

#endif
