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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_HTTP_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_HTTP_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Centrally including headers here.
#ifdef _MSC_VER
    #include <process.h>
    #include <strsafe.h>
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/select.h>
#endif

// Centrally including headers here.
#ifdef WITH_MBEDTLS
    #include <mbedtls/base64.h>
    #include <mbedtls/error.h>
    #include <mbedtls/ecp.h>
    #include <mbedtls/platform.h>
    #include <mbedtls/sha1.h>
    #include <mbedtls/ssl.h>
    #include <mbedtls/x509_crt.h>
#endif

namespace libbitcoin {
namespace server {
namespace http {

#ifdef _MSC_VER
    typedef SOCKET sock_t;
    typedef uint32_t in_addr_t;
    #define CLOSE_SOCKET closesocket
#else
    typedef uint32_t sock_t;
    #define CLOSE_SOCKET ::close
#endif

#ifdef WITH_MBEDTLS
    static const int32_t default_ciphers[] =
    {
        MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,
        MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
        MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
        MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        0
    };
#endif

static const size_t default_buffer_length = 1 * 1024;
static const size_t transfer_buffer_length = 256 * 1024;

typedef std::array<uint8_t, default_buffer_length> read_buffer;
typedef std::unordered_map<std::string, std::string> string_map;

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
