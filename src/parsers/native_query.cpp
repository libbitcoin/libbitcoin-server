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
#include <bitcoin/server/parsers/native_query.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::http;

BC_PUSH_WARNING(NO_ARRAY_INDEXING)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

inline bool is_true(const std::string& value) NOEXCEPT
{
    return value == token::true_;
}

inline bool is_false(const std::string& value) NOEXCEPT
{
    return value == token::false_;
}

inline void set_media(rpc::object_t& params, media_type media) NOEXCEPT
{
    params["media"] = to_value(media);
}

bool native_query(rpc::request_t& out, const request& request) NOEXCEPT
{
    wallet::uri uri{};
    if (!uri.decode(request.target()))
        return false;

    constexpr auto html = media_type::text_html;
    constexpr auto text = media_type::text_plain;
    constexpr auto json = media_type::application_json;
    constexpr auto data = media_type::application_octet_stream;

    // Caller must have provided a request.params object.
    if (!out.params.has_value() ||
        !std::holds_alternative<rpc::object_t>(out.params.value()))
        return false;

    auto query = uri.decode_query();
    auto& params = std::get<rpc::object_t>(out.params.value());

    // Witness is optional<true> (where applicable), so only set if false.
    if (const auto witness = query.find(token::witness); witness != query.end())
    {
        if (is_false(witness->second))
            params[token::witness] = false;
        else if (!is_true(witness->second))
            return false;
    }

    // Turbo is optional<true> (where applicable), so only set if false.
    if (const auto turbo = query.find(token::turbo); turbo != query.end())
    {
        if (is_false(turbo->second))
            params[token::turbo] = false;
        else if (!is_true(turbo->second))
            return false;
    }

    const auto format = query[token::format];
    const auto accepts = to_media_types((request)[field::accept]);

    // Prioritize query string format over http headers.
    if (format == token::formats::json)
        set_media(params, json);
    else if (format == token::formats::text)
        set_media(params, text);
    else if (format == token::formats::data)
        set_media(params, data);
    else if (format == token::formats::html)
        set_media(params, html);

    // Priotize: json, html, text, data (ignores accept priorities).
    else if (contains(accepts, json))
        set_media(params, json);
    else if (contains(accepts, html))
        set_media(params, html);
    else if (contains(accepts, text))
        set_media(params, text);
    else if (contains(accepts, data))
        set_media(params, data);
    else
        return false;

    // Media type has been set.
    return true;
}

media_type get_media(const rpc::request_t& model) NOEXCEPT
{
    if (model.params.has_value())
    {
        const auto& params = std::get<rpc::object_t>(model.params.value());
        const auto& media = params.find("media");
        if (media != params.end() && std::holds_alternative<uint8_t>(
            media->second.value()))
        {
            switch (const auto value = static_cast<media_type>(
                std::get<uint8_t>(media->second.value())))
            {
                case media_type::text_html:
                case media_type::text_plain:
                case media_type::application_json:
                case media_type::application_octet_stream:
                    return value;
                default:
                    return media_type::unknown;
            }
        }
    }

    return media_type::unknown;
}

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
