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
#include <bitcoin/server/web/json_string.hpp>

// Explicitly use std::placeholders here for usage internally to the
// boost parsing helpers included from json_parser.hpp.
// See: https://svn.boost.org/trac10/ticket/12621
#include <functional>
using namespace std::placeholders;

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <bitcoin/server/server_node.hpp>



namespace libbitcoin {
namespace server {
namespace web {
using namespace boost::property_tree;

// Object to JSON converters.
//-----------------------------------------------------------------------------

std::string to_json(const ptree& tree)
{
    std::stringstream json_stream;
    write_json(json_stream, tree);
    return json_stream.str();
}

std::string to_json(uint64_t height, uint32_t sequence)
{
    return to_json(bc::property_tree(height, sequence));
}

std::string to_json(const std::error_code& code, uint32_t sequence)
{
    return to_json(bc::property_tree(code, sequence));
}

std::string to_json(const bc::chain::header& header, uint32_t sequence)
{
    auto tree = bc::property_tree(bc::config::header(header));
    tree.put("sequence", sequence);
    return to_json(tree);
}

std::string to_json(const bc::chain::block& block, uint32_t height,
    uint32_t sequence)
{
    auto tree = bc::property_tree(bc::config::header(block.header()));
    tree.put("height", height);
    tree.put("sequence", sequence);
    return to_json(tree);
}

std::string to_json(const bc::chain::transaction& transaction,
    uint32_t sequence)
{
    static constexpr auto json_encoding = true;
    auto tree = bc::property_tree(bc::config::transaction(
        transaction), json_encoding);
    tree.put("sequence", sequence);
    return to_json(tree);
}

} // namespace web
} // namespace server
} // namespace libbitcoin
