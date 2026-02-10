/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_SERVER_CHANNELS_CHANNEL_HPP
#define LIBBITCOIN_SERVER_CHANNELS_CHANNEL_HPP

#include <memory>
#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

/// Intermediate base class for future server injection.
class BCS_API channel
  : public node::channel
{
public:
    typedef std::shared_ptr<channel> ptr;
    using base = node::channel;
        
    using base::channel;
};

} // namespace server
} // namespace libbitcoin

#endif
