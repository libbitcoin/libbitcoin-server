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

#include <string>
#include <boost/filesystem.hpp>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/web/http/http.hpp>
#include <bitcoin/server/web/http/http_request.hpp>
#include <bitcoin/server/web/http/websocket_op.hpp>

namespace libbitcoin {
namespace server {
namespace http {

#ifdef _MSC_VER
    #define last_error() GetLastError()
#else
    #define last_error() errno
#endif

#ifdef _MSC_VER
    #define would_block(value) (value == WSAEWOULDBLOCK)
#else
    #define would_block(value) (value == EAGAIN || value == EWOULDBLOCK)
#endif

#ifdef WITH_MBEDTLS
    std::string mbedtls_error_string(int32_t error);
    ////short_hash sha1(const std::string& input);
    #define mbedtls_would_block(value) \
        (value == MBEDTLS_ERR_SSL_WANT_READ || \
         value == MBEDTLS_ERR_SSL_WANT_WRITE)
#endif

BCS_API std::string error_string();
BCS_API std::string op_to_string(websocket_op code);
BCS_API std::string websocket_key_response(const std::string& websocket_key);
BCS_API bool is_json_request(const std::string& header_value);
BCS_API bool parse_http(http_request& out, const std::string& request);
BCS_API std::string mime_type(const boost::filesystem::path& path);

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
