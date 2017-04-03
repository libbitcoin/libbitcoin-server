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
#include <boost/functional/hash_fwd.hpp>
#include <bitcoin/bitcoin.hpp>
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
    /// A zeromq route identifier is always this size.
    static const size_t identifier_size = 5;

    /// A zeromq route identifier.
    ////typedef byte_array<identifier_size> identifier;

    // TODO: change to byte_array
    /// A zeromq route identifier.
    typedef data_chunk identifier;

    /// Construct a route.
    route();

    /// A printable address for logging only.
    std::string display() const;

    /// Equality operator.
    bool operator==(const route& other) const;

    /// The message requires a secure port.
    bool secure;

    /// The message route is delimited using an empty frame.
    bool delimited;

    /// The simple route supports only one address.
    identifier address;
};

} // namespace server
} // namespace libbitcoin

namespace std
{

template<>
struct hash<bc::server::route>
{
    size_t operator()(const bc::server::route& value) const
    {
        size_t seed = 0;
        boost::hash_combine(seed, value.secure);
        boost::hash_combine(seed, value.address);
        boost::hash_combine(seed, value.delimited);
        return seed;
    }
};

} // namespace std

#endif
