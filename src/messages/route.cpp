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
#include <bitcoin/server/messages/route.hpp>

#include <string>
#include <bitcoin/protocol.hpp>

namespace libbitcoin {
namespace server {

using namespace bc:: protocol;

static const zmq::message::address default_address
{
    { 0x00, 0x00, 0x00, 0x00, 0x00 }
};

route::route()
  : delimited_(false),
    address_(default_address)
{
}

bool route::delimited() const
{
    return delimited_;
}

zmq::message::address route::address() const
{
    return address_;
}

void route::set_delimited(bool value)
{
    delimited_ = value;
}

void route::set_address(const zmq::message::address& value)
{
    address_ = value;
}

std::string route::display() const
{
    return "[" + encode_base16(address_) + "]" + (delimited_ ? "[]" : "");
}

} // namespace server
} // namespace libbitcoin
