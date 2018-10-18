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
#include <algorithm>
#include <bitcoin/server/define.hpp>

#include "connection.hpp"
#include "http.hpp"
#include "utilities.hpp"

namespace libbitcoin {
namespace server {
namespace http {

connection::connection()
  : user_data_(nullptr),
    state_(connection_state::unknown),
    socket_(0),
    address_{},
    last_active_(asio::steady_clock::now()),
    high_water_mark_(default_high_water_mark),
    maximum_incoming_frame_length_(default_incoming_frame_length),
    ssl_context_{},
    websocket_endpoint_{},
    json_rpc_(false),
    websocket_(false),
    file_transfer_{},
    websocket_transfer_{},
    bytes_read_(0)
{
    write_buffer_.reserve(high_water_mark_);
}

connection::connection(sock_t connection, struct sockaddr_in& address)
  : user_data_(nullptr),
    state_(connection_state::unknown),
    socket_(connection),
    address_(address),
    last_active_(asio::steady_clock::now()),
    high_water_mark_(default_high_water_mark),
    maximum_incoming_frame_length_(default_incoming_frame_length),
    ssl_context_{},
    websocket_endpoint_{},
    websocket_(false),
    json_rpc_(false),
    file_transfer_{},
    websocket_transfer_{},
    bytes_read_(0)
{
    write_buffer_.reserve(high_water_mark_);
}

connection::~connection()
{
    if (!closed())
        close();
}

connection_state connection::state() const
{
    return state_;
}

void connection::set_state(connection_state state)
{
    state_ = state;
}

size_t connection::high_water_mark() const
{
    return high_water_mark_;
}

// May invalidate any buffered write data.
void connection::set_high_water_mark(size_t high_water_mark)
{
    if (high_water_mark > 0)
    {
        high_water_mark_ = high_water_mark;
        write_buffer_.reserve(high_water_mark);
        write_buffer_.shrink_to_fit();
    }
}

size_t connection::maximum_incoming_frame_length() const
{
    return maximum_incoming_frame_length_;
}

void connection::set_maximum_incoming_frame_length(size_t length)
{
    if (length > 0)
        maximum_incoming_frame_length_ = length;
}

void connection::set_socket_non_blocking()
{
#ifdef WIN32
    ULONG non_blocking = 1;
    ioctlsocket(socket_, FIONBIO, &non_blocking);
#else
    fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFD) | O_NONBLOCK);
#endif
}

sockaddr_in connection::address() const
{
    return address_;
}

bool connection::reuse_address() const
{
    static constexpr uint32_t opt = 1;

    // reinterpret_cast required for Win32, otherwise nop.
    return setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) != -1;
}

bool connection::closed() const
{
    return state_ == connection_state::closed;
}

int32_t connection::read()
{
#ifdef WITH_MBEDTLS
    bytes_read_ = ssl_context_.enabled ?
        mbedtls_ssl_read(&ssl_context_.context, data, maximum_read_length) :
        recv(socket_, data, maximum_read_length, 0)
#else
    // reinterpret_cast required for Win32, otherwise nop.
    bytes_read_ = recv(socket_, reinterpret_cast<char*>(read_buffer_.data()),
        maximum_read_length, 0);
#endif
    return bytes_read_;
}

int32_t connection::read_length()
{
    return bytes_read_;
}

http::read_buffer& connection::read_buffer()
{
    return read_buffer_;
}

data_chunk& connection::write_buffer()
{
    return write_buffer_;
}

// This is effectively a blocking write call that does not buffer internally.
int32_t connection::do_write(const uint8_t* data, size_t length, bool frame)
{
    const auto plaintext_write = [this](const uint8_t* data, size_t length)
    {
        // BUGBUG: must set errno for return error handling.
        if (length > max_int32)
            return -1;

#ifdef WIN32
        return send(socket_, reinterpret_cast<const char*>(data),
            static_cast<int32_t>(length), 0);
#else
        return send(socket_, data, length, 0);
#endif
    };

#ifdef WITH_MBEDTLS
    const auto ssl_write = [this](const uint8_t* data, size_t length)
    {
        // BUGBUG: handle MBEDTLS_ERR_SSL_WANT_WRITE
        return mbedtls_ssl_write(&ssl_context_.context, data, length));
    };

    auto writer = ssl_context_.enabled ? ssl_write : plaintext_write;
#else
    auto writer = plaintext_write;
#endif

    if (frame)
    {
        auto frame_data = websocket_frame::to_data(length, websocket_op::text);
        const auto written = writer(frame_data.data(), frame_data.size());

        if (written < 0)
            return written;
    }

    auto remaining = length;
    auto position = data;

    do
    {
        const auto written = writer(position, remaining);

        if (written < 0)
        {
            const auto error = last_error();
            if (!would_block(error))
            {
                LOG_WARNING(LOG_SERVER_HTTP)
                    << "do_write failed. requested " << remaining
                    << " and wrote " << written << ": " << error_string();
                return written;
            }

            // BUGBUG: non-terminating loop , or does would_block prevent?
            continue;
        }

        position += written;
        remaining -= written;

    } while (remaining != 0);

    return static_cast<int32_t>(position - data);
}

int32_t connection::write(const data_chunk& buffer)
{
    return write(buffer.data(), buffer.size());
}

int32_t connection::write(const std::string& buffer)
{
    const auto data = reinterpret_cast<const uint8_t*>(buffer.data());
    return write(data, buffer.size());
}

// This is a buffered write call if under the high water mark.
int32_t connection::write(const uint8_t* data, size_t length)
{
    if (length > max_int32)
        return -1;

    static constexpr auto maximal_websocket_frame = 11u;
    const auto buffered_length = write_buffer_.size() + length +
        (websocket_ ? maximal_websocket_frame : 0);

    // If we're currently at the hwm, issue blocking writes until
    // we've cleared the buffered data and then write this current
    // request.  This is an expensive operation, but should be
    // mostly avoidable with proper hwm tuning of your application.
    if (buffered_length >= high_water_mark_)
    {
        // Drain the buffered data.
        while (!write_buffer_.empty())
        {
            const auto segment_length = std::min(transfer_buffer_length,
                write_buffer_.size());
            const auto written = do_write(write_buffer_.data(), segment_length,
                false);

            if (written < 0)
                return written;

            if (!write_buffer_.empty())
                write_buffer_.erase(write_buffer_.begin(),
                    write_buffer_.begin() + written);
        }

        // Perform this write in a blocking manner.
        return do_write(data, length, websocket_);
    }

    if (websocket_)
    {
        auto frame_data = websocket_frame::to_data(length, websocket_op::text);
        write_buffer_.insert(write_buffer_.end(), frame_data.begin(),
            frame_data.end());
    }

    // Buffer this data for future writes (called from poll).
    write_buffer_.insert(write_buffer_.end(), data, data + length);
    return static_cast<int32_t>(length);
}

void connection::close()
{
    if (state_ == connection_state::closed)
        return;

#ifdef WITH_MBEDTLS
    if (ssl_context_.enabled)
    {
        if (state_ != connection_state::listening)
            mbedtls_ssl_free(&ssl_context_.context);

        mbedtls_pk_free(&ssl_context_.key);
        mbedtls_x509_crt_free(&ssl_context_.certificate);
        mbedtls_x509_crt_free(&ssl_context_.ca_certificate);
        mbedtls_ssl_config_free(&ssl_context_.configuration);
        ssl_context_.enabled = false;
    }
#endif

    CLOSE_SOCKET(socket_);
    state_ = connection_state::closed;
    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "Closed socket " << this;
}

sock_t& connection::socket()
{
    return socket_;
}

http::ssl& connection::ssl_context()
{
    return ssl_context_;
}

bool connection::ssl_enabled() const
{
    return ssl_context_.enabled;
}

bool connection::websocket() const
{
    return websocket_;
}

void connection::set_websocket(bool websocket)
{
    websocket_ = websocket;
}

const std::string& connection::websocket_endpoint() const
{
    return websocket_endpoint_;
}

void connection::set_websocket_endpoint(const std::string endpoint)
{
    websocket_endpoint_ = endpoint;
}

bool connection::json_rpc() const
{
    return json_rpc_;
}

void connection::set_json_rpc(bool json_rpc)
{
    json_rpc_ = json_rpc;
}

void* connection::user_data()
{
    return user_data_;
}

void connection::set_user_data(void* user_data)
{
    user_data_ = user_data;
}

http::file_transfer& connection::file_transfer()
{
    return file_transfer_;
}

http::websocket_transfer& connection::websocket_transfer()
{
    return websocket_transfer_;
}

bool connection::operator==(const connection& other)
{
    return user_data_ == other.user_data_ && socket_ == other.socket_;
}

} // namespace http
} // namespace server
} // namespace libbitcoin
