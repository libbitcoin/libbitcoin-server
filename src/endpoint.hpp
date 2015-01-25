/*
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
#ifndef LIBBITCOIN_SERVER_ENDPOINT_HPP
#define LIBBITCOIN_SERVER_ENDPOINT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace server {

class endpoint_type
{
public:
    endpoint_type();
    endpoint_type(const std::string& value);
    endpoint_type(const endpoint_type& other);

    const std::string& get_scheme() const;
    const std::string& get_host() const;
    uint16_t get_port() const;
    operator const std::string() const;

    friend std::istream& operator>>(std::istream& input,
        endpoint_type& argument);
    friend std::ostream& operator<<(std::ostream& output,
        const endpoint_type& argument);

private:
    std::string scheme_;
    std::string host_;
    uint16_t port_;
};

} // namespace server
} // namespace libbitcoin

#endif
