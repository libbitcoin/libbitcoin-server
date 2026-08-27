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
#ifndef LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_DEPLOYMENTS_HPP
#define LIBBITCOIN_SERVER_SERIALIZERS_BITCOIND_DEPLOYMENTS_HPP

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// The getdeploymentinfo result for the block at link/height.
BCS_API network::rpc::object_t deployment_info(const node::query& query,
    const system::settings& settings, const database::header_link& link,
    size_t height) NOEXCEPT;

} // namespace server
} // namespace libbitcoin

#endif
