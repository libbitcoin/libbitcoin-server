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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_WEBSOCKET_FRAME_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_WEBSOCKET_FRAME_HPP

#include <cstddef>
#include <cstdint>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/web/http/event.hpp>
#include <bitcoin/server/web/http/websocket_op.hpp>

namespace libbitcoin {
namespace server {
namespace http {

// TODO: move implementation to cpp.
class BCS_API websocket_frame
{
public:
    websocket_frame(const uint8_t* data, size_t size)
      : valid_(false), flags_(0), header_(0), data_(0)
    {
        from_data(data, size);
    }

    static data_chunk to_header(size_t length, websocket_op code)
    {
        if (length < 0x7e)
        {
            return build_chunk(
            {
                to_array(uint8_t(0x80) | static_cast<uint8_t>(code)),
                to_array(static_cast<uint8_t>(length))
            });
        }
        else if (length < max_uint16)
        {
            return build_chunk(
            {
                to_array(uint8_t(0x80) | static_cast<uint8_t>(code)),
                to_array(uint8_t(0x7e)),
                to_big_endian(static_cast<uint16_t>(length))
            });
        }
        else
        {
            return build_chunk(
            {
                to_array(uint8_t(0x80) | static_cast<uint8_t>(code)),
                to_array(uint8_t(0x7f)),
                to_big_endian(static_cast<uint64_t>(length))
            });
        }
    }

    operator bool() const
    {
        return valid_;
    }

    bool final() const
    {
        return (flags_ & 0x80) != 0;
    }

    bool fragment() const
    {
        return !final() || op_code() == websocket_op::continuation;
    }

    event event_type() const
    {
        return (flags_ & 0x08) ? event::websocket_control_frame :
            event::websocket_frame;
    }

    websocket_op op_code() const
    {
        return static_cast<websocket_op>(flags_ & 0x0f);
    }

    uint8_t flags() const
    {
        return flags_;
    }

    size_t header_length() const
    {
        return header_;
    }

    size_t data_length() const
    {
        return data_;
    }

    size_t mask_length() const
    {
        return valid_ ? mask_ : 0;
    }

private:
    void from_data(const uint8_t* data, size_t read_length)
    {
        static constexpr size_t prefix = 2;
        static constexpr size_t prefix16 = prefix + sizeof(uint16_t);
        static constexpr size_t prefix64 = prefix + sizeof(uint64_t);

        // Invalid websocket frame (too small).
        // Invalid websocket frame (unmasked).
        if (read_length < 2 || (data[1] & 0x80) == 0)
            return;

        flags_ = data[0];
        const size_t length = (data[1] & 0x7f);

        if (read_length >= mask_ && length < 0x7e)
        {
            header_ = prefix + mask_;
            data_ = length;
        }
        else if (read_length >= prefix16 + mask_ && length == 0x7e)
        {
            header_ = prefix16 + mask_;
            data_ = from_big_endian<uint16_t>(&data[prefix],
                &data[prefix16]);
        }
        else if (read_length >= prefix64 + mask_)
        {
            header_ = prefix64 + mask_;
            data_ = from_big_endian<uint64_t>(&data[prefix],
                &data[prefix64]);
        }

        valid_ = true;
        return;
    }

private:
    static const size_t mask_ = 4;

    bool valid_;
    uint8_t flags_;
    size_t header_;
    size_t data_;
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
