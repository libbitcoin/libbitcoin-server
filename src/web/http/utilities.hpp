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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_UTILITIES_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_UTILITIES_HPP

namespace libbitcoin {
namespace server {
namespace http {

#ifdef WIN32
#define last_error() GetLastError()
#else
#define last_error() errno
#endif

#ifdef WIN32
    #define would_block(value) (value == WSAEWOULDBLOCK)
#else
    #define would_block(value) (value == EAGAIN || value == EWOULDBLOCK)
#endif

#define mbedtls_would_block(value) \
    (value == MBEDTLS_ERR_SSL_WANT_READ || value == MBEDTLS_ERR_SSL_WANT_WRITE)

#ifdef WITH_MBEDTLS
    std::string mbedtls_error_string(int error);
    sha1_hash sha1(const std::string& input);
#endif

std::string error_string();
std::string to_string(websocket_op code);
std::string websocket_key_response(const std::string& websocket_key);
bool is_json_request(const std::string& header_value);
bool parse_http(http_request& out, const std::string& request);
////unsigned long resolve_hostname(const std::string& hostname);
std::string mime_type(const std::string& filename);

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
