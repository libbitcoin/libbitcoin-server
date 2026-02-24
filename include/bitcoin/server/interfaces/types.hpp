/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_INTERFACES_TYPES_HPP
#define LIBBITCOIN_SERVER_INTERFACES_TYPES_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {
namespace interface {
    
/// Alias network::rpc names within interface::.

template <text_t Text, typename ...Args>
using method = network::rpc::method<Text, Args...>;
template <auto& Methods, size_t Index>
using method_at = network::rpc::method_at<Methods, Index>;
template <typename Methods, network::rpc::grouping Mode =
    network::rpc::grouping::either>
using publish = network::rpc::publish<Methods, Mode>;

template <auto Default>
using optional = network::rpc::optional<Default>;
template <typename Type>
using nullable = network::rpc::nullable<Type>;
using boolean_t = network::rpc::boolean_t;
using string_t = network::rpc::string_t;
using number_t = network::rpc::number_t;
using object_t = network::rpc::object_t;
using array_t = network::rpc::array_t;
using value_t = network::rpc::value_t;
using null_t = network::rpc::null_t;

namespace empty { constexpr auto array = network::rpc::empty::array; };
namespace empty { constexpr auto object = network::rpc::empty::object; };
namespace empty { constexpr auto value = network::rpc::empty::value; };

} // namespace interface
} // namespace server
} // namespace libbitcoin

#endif
