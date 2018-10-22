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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_JSON_STRING_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_JSON_STRING_HPP

#include <cstdint>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/server_node.hpp>

namespace libbitcoin {
namespace server {
namespace web {

// Object to JSON converters.
//-----------------------------------------------------------------------------

std::string to_json(const boost::property_tree::ptree& tree);
std::string to_json(uint64_t height, uint32_t id);
std::string to_json(const code& code, uint32_t id);
std::string to_json(const chain::header& header, uint32_t id);
std::string to_json(const chain::block& block, uint32_t id);
std::string to_json(const chain::block& block, uint32_t height, uint32_t id);
std::string to_json(const chain::transaction& transaction, uint32_t id);

} // namespace web
} // namespace server
} // namespace libbitcoin

#endif
