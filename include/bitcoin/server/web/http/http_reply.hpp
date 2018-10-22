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
#ifndef LIBBITCOIN_SERVER_WEB_HTTP_HTTP_REPLY_HPP
#define LIBBITCOIN_SERVER_WEB_HTTP_HTTP_REPLY_HPP

#include <array>
#include <cstddef>
#include <ctime>
#include <string>
#include <sstream>
#include <unordered_map>
#include <bitcoin/server/define.hpp>
#include <bitcoin/server/web/http/http.hpp>
#include <bitcoin/server/web/http/protocol_status.hpp>

namespace libbitcoin {
namespace server {
namespace http {
    
// TODO: move implementation to cpp.
class BCS_API http_reply
{
public:
    static std::string to_string(protocol_status status)
    {
        typedef std::unordered_map<protocol_status, std::string> status_map;
        static const status_map status_strings
        {
            { protocol_status::switching, "HTTP/1.1 101 Switching Protocols\r\n" },
            { protocol_status::ok, "HTTP/1.0 200 OK\r\n" },
            { protocol_status::created, "HTTP/1.0 201 Created\r\n" },
            { protocol_status::accepted, "HTTP/1.0 202 Accepted\r\n" },
            { protocol_status::no_content, "HTTP/1.0 204 No Content\r\n" },
            { protocol_status::multiple_choices, "HTTP/1.0 300 Multiple Choices\r\n" },
            { protocol_status::moved_permanently, "HTTP/1.0 301 Moved Permanently\r\n" },
            { protocol_status::moved_temporarily, "HTTP/1.0 302 Moved Temporarily\r\n" },
            { protocol_status::not_modified, "HTTP/1.0 304 Not Modified\r\n" },
            { protocol_status::bad_request, "HTTP/1.0 400 Bad Request\r\n" },
            { protocol_status::unauthorized, "HTTP/1.0 401 Unauthorized\r\n" },
            { protocol_status::forbidden, "HTTP/1.0 403 Forbidden\r\n" },
            { protocol_status::not_found, "HTTP/1.0 404 Not Found\r\n" },
            { protocol_status::internal_server_error, "HTTP/1.0 500 Internal Server Error\r\n" },
            { protocol_status::not_implemented, "HTTP/1.0 501 Not Implemented\r\n" },
            { protocol_status::bad_gateway, "HTTP/1.0 502 Bad Gateway\r\n" },
            { protocol_status::service_unavailable, "HTTP/1.0 503 Service Unavailable\r\n" }
        };

        const auto it = status_strings.find(status);
        return it == status_strings.end() ? std::string{} : it->second;
    }

    static std::string generate(protocol_status status, std::string mime_type,
        size_t content_length, bool keep_alive)
    {
        static const size_t max_date_time_length = 32;
        std::array<char, max_date_time_length> time_buffer;
        const auto current_time = std::time(nullptr);

        // BUGBUG: std::gmtime may not be thread safe.
        std::strftime(time_buffer.data(), time_buffer.size(),
            "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&current_time));

        std::stringstream response;
        response
            << to_string(status)
            << "Date: " << time_buffer.data() << "\r\n"
            << "Accept-Ranges: none\r\n"
            << "Connection: " << (keep_alive ? "keep-alive" : "close")
            << "\r\n";

        if (!mime_type.empty())
            response << "Content-Type: " << mime_type << "\r\n";

        if (content_length > 0)
            response << "Content-Length: " << content_length << "\r\n";

        response << "\r\n";
        return response.str();
    }

    static std::string generate_upgrade(const std::string& key_response,
        const std::string& protocol)
    {
        std::stringstream response;
        response
            << to_string(protocol_status::switching)
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade" << "\r\n";

        if (!protocol.empty())
            response << protocol << "\r\n";

        response << "Sec-WebSocket-Accept: " << key_response << "\r\n\r\n";
        return response.str();
    }

    protocol_status status;
    string_map headers;
    std::string content;
};

} // namespace http
} // namespace server
} // namespace libbitcoin

#endif
