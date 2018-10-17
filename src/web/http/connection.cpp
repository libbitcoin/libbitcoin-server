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
    last_active_(std::chrono::steady_clock::now()),
    high_water_mark_(default_high_water_mark),
    maximum_incoming_frame_length_(default_incoming_frame_length),
    read_buffer_{},
    write_buffer_{},
    ssl_context_{},
    websocket_(false),
    websocket_endpoint_{},
    json_rpc_(false),
    file_transfer_{},
    websocket_transfer_{}
{
    write_buffer_.reserve(high_water_mark_);
}

connection::connection(socket_connection connection, struct sockaddr_in&
    address)
  : user_data_(nullptr),
    state_(connection_state::unknown),
    socket_(connection),
    address_(address),
    last_active_(std::chrono::steady_clock::now()),
    high_water_mark_(default_high_water_mark),
    maximum_incoming_frame_length_(default_incoming_frame_length),
    read_buffer_{},
    write_buffer_{},
    ssl_context_{},
    websocket_(false),
    websocket_endpoint_{},
    json_rpc_(false),
    file_transfer_{},
    websocket_transfer_{}
{
    write_buffer_.reserve(high_water_mark_);
}

connection::~connection()
{
    if (!closed())
        close();
}

// May invalidate any buffered write data.
void connection::set_high_water_mark(int32_t high_water_mark)
{
    if (high_water_mark > 0)
    {
        high_water_mark_ = high_water_mark;
        write_buffer_.reserve(high_water_mark);
        write_buffer_.shrink_to_fit();
    }
}

int32_t connection::high_water_mark()
{
    return high_water_mark_;
}

void connection::set_maximum_incoming_frame_length(int32_t length)
{
    if (length > 0)
        maximum_incoming_frame_length_ = length;
}

int32_t connection::maximum_incoming_frame_length()
{
    return maximum_incoming_frame_length_;
}

void connection::set_socket_non_blocking()
{
#ifdef WIN32
    unsigned long non_blocking = 1;
    ioctlsocket(socket_ , FIONBIO, &non_blocking);
#else
    fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFD) | O_NONBLOCK);
#endif
}

struct sockaddr_in connection::address()
{
    return address_;
}

bool connection::reuse_address()
{
    static constexpr int opt = 1;
    return setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt)) != -1;
}

connection_state connection::state()
{
    return state_;
}

void connection::set_state(connection_state state)
{
    state_ = state;
}

bool connection::closed()
{
    return state_ == connection_state::closed;
}

int32_t connection::read()
{
#ifdef WIN32
    auto data = reinterpret_cast<char*>(read_buffer_.data());
#else
    auto data = reinterpret_cast<unsigned char*>(read_buffer_.data());
#endif

#ifdef WITH_MBEDTLS
    bytes_read_ = (ssl_context_.enabled ? mbedtls_ssl_read(
        &ssl_context_.context, data, maximum_read_length) :
        recv(socket_, data, maximum_read_length, 0));
#else
    bytes_read_ = recv(socket_, data, maximum_read_length, 0);
#endif
    return bytes_read_;
}

int32_t connection::read_length()
{
    return bytes_read_;
}

buffer& connection::read_buffer()
{
    return read_buffer_;
}

data_buffer& connection::write_buffer()
{
    return write_buffer_;
}

// This is effectively a blocking write call that does not buffer
// internally.
int32_t connection::do_write(const unsigned char* data, int32_t length,
    bool write_frame)
{
    write_method plaintext_write = [this](const unsigned char* data,
        int32_t length)
    {
        return static_cast<int32_t>(send(socket_,
#ifdef WIN32
            reinterpret_cast<char*>(data),
#else
            data,
#endif
            length, 0));
    };

#ifdef WITH_MBEDTLS
    write_method ssl_write = [this](const unsigned char* data, int32_t length)
    {
        return static_cast<int32_t>(
            mbedtls_ssl_write(&ssl_context_.context, data, length));
    };

    auto writer = static_cast<write_method>(ssl_context_.enabled ? ssl_write :
        plaintext_write);
#else
    auto writer = static_cast<write_method>(plaintext_write);
#endif

    if (write_frame)
    {
        const auto frame = generate_websocket_frame(length,
            websocket_op::text);
        const auto frame_data = reinterpret_cast<const unsigned char*>(&frame);

        int32_t frame_write = writer(frame_data, frame.write_length);
        if (frame_write < 0)
            return frame_write;
    }

    int32_t written = 0;
    auto remaining = length;
    auto position = data;

    do
    {
        written = writer(position, remaining);
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

            continue;
        }

        position += written;
        remaining -= written;

    } while (remaining != 0);

    return static_cast<int32_t>(position - data);
}

int32_t connection::write(const std::string& buffer)
{
    return write(reinterpret_cast<const unsigned char*>(buffer.c_str()),
        buffer.size());
}

// This is a buffered write call so long as we're under the high
// water mark.
int32_t connection::write(const unsigned char* data, int32_t length)
{
    if (length < 0)
        return length;

    const int32_t buffered_length = write_buffer_.size() + length +
        (websocket_ ? sizeof(websocket_frame) : 0);

    // If we're currently at the hwm, issue blocking writes until
    // we've cleared the buffered data and then write this current
    // request.  This is an expensive operation, but should be
    // mostly avoidable with proper hwm tuning of your
    // application.
    if (buffered_length >= high_water_mark_)
    {
        // Drain the buffered data.
        while (!write_buffer_.empty())
        {
            const auto segment_length = std::min(transfer_buffer_length,
                static_cast<int32_t>(write_buffer_.size()));
            const auto written = do_write(reinterpret_cast<unsigned char*>(
                write_buffer_.data()), segment_length, false);

            if (written < 0)
                return written;

            if (!write_buffer_.empty())
                write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin()
                    + written);
        }

        // Perform this write in a blocking manner.
        return do_write(data, length, websocket_);
    }

    if (websocket_)
    {
        const auto frame = generate_websocket_frame(length,
            websocket_op::text);
        const auto frame_data = reinterpret_cast<const unsigned char*>(&frame);
        write_buffer_.insert(write_buffer_.end(), frame_data, frame_data +
            frame.write_length);
    }

    // Buffer this data for future writes (called from poll).
    write_buffer_.insert(write_buffer_.end(), data, data + length);
    return length;
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

    close_socket(socket_);
    state_ = connection_state::closed;
    LOG_VERBOSE(LOG_SERVER_HTTP)
        << "Closed socket " << this;
}

socket_connection& connection::socket()
{
    return socket_;
}

http::ssl& connection::ssl_context()
{
    return ssl_context_;
}

bool connection::ssl_enabled()
{
    return ssl_context_.enabled;
}

bool connection::websocket()
{
    return websocket_;
}

void connection::set_websocket(bool websocket)
{
    websocket_ = websocket;
}

const std::string& connection::websocket_endpoint()
{
    return websocket_endpoint_;
}

void connection::set_websocket_endpoint(const std::string endpoint)
{
    websocket_endpoint_ = endpoint;
}

bool connection::json_rpc()
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

websocket_frame connection::generate_websocket_frame(int32_t length,
    websocket_op code)
{
    BITCOIN_ASSERT(length > 0);
    websocket_frame frame{};
    frame.flags = 0x80 | static_cast<uint8_t>(code);

    auto start = &frame.length[0];
    if (length < 126)
    {
        frame.payload_length = static_cast<uint8_t>(length);
        frame.write_length = 2;
    }
    else if (length < std::numeric_limits<uint16_t>::max())
    {
        auto data_length = static_cast<uint16_t>(length);
        *reinterpret_cast<uint16_t*>(start) =
            boost::endian::endian_reverse(data_length);
        frame.payload_length = 126;
        frame.write_length = 4; // 2 + sizeof(uint16_t)
    }
    else
    {
        auto data_length = static_cast<uint64_t>(length);
        *reinterpret_cast<uint64_t*>(start) =
            boost::endian::endian_reverse(data_length);
        frame.payload_length = 127;
        frame.write_length = 10; // 2 + sizeof(uint64_t)
    }

    return frame;
}

} // namespace http
} // namespace server
} // namespace libbitcoin
