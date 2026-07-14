/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/server/parsers/admin_query.hpp>

#include <bitcoin/server/define.hpp>

namespace libbitcoin {
namespace server {

using namespace system;
using namespace network;
using namespace network::http;

BC_PUSH_WARNING(NO_ARRAY_INDEXING)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Masks are constrained to exact json number (ieee double) representation.
constexpr auto maximum_filter = sub1(power2<uint64_t>(53u));

template <typename Number>
static bool to_number(Number& out, const std::string& token) NOEXCEPT
{
    return !token.empty() && is_ascii_numeric(token) && (is_one(token.size()) ||
        token.front() != '0') && deserialize(out, token);
}

inline void set_media(rpc::object_t& params, media_type media) NOEXCEPT
{
    params["media"] = to_value(media);
}

bool admin_query(rpc::request_t& out, const request& request) NOEXCEPT
{
    const auto accepts = to_media_types((request)[field::accept]);
    return admin_query(out, request.target(), accepts);
}

bool admin_query(rpc::request_t& out, const std::string& target,
    const media_types& accepts) NOEXCEPT
{
    wallet::uri uri{};
    if (!uri.decode(target))
        return false;

    constexpr auto html = media_type::text_html;
    constexpr auto json = media_type::application_json;

    // Caller must have provided a request.params object.
    if (!out.params.has_value() ||
        !std::holds_alternative<rpc::object_t>(out.params.value()))
        return false;

    using namespace server::admin;
    auto query = uri.decode_query();
    auto& params = std::get<rpc::object_t>(out.params.value());

    // Filter is required by admin methods but not html (page) requests.
    // Omission for dispatched (non-html) requests is rejected by dispatch.
    if (const auto filter = query.find(token::filter); filter != query.end())
    {
        uint64_t value{};
        if (!to_number(value, filter->second) || value > maximum_filter)
            return false;

        params[token::filter] = value;
    }

    const auto format = query[token::format];

    // Prioritize query string format over http headers.
    if (format == token::formats::json)
        set_media(params, json);
    else if (format == token::formats::html)
        set_media(params, html);
    else if (!format.empty())
        return false;

    // Priotize: json, html (ignores accept priorities).
    else if (contains(accepts, json))
        set_media(params, json);
    else if (contains(accepts, html))
        set_media(params, html);
    //else no media type is set, which results in not acceptable.

    // Parse successful, media type not acceptable if not set.
    return true;
}

media_type strip_media(rpc::request_t& model) NOEXCEPT
{
    if (model.params.has_value())
    {
        auto& params = std::get<rpc::object_t>(model.params.value());
        const auto& media = params.find("media");
        if (media != params.end() && std::holds_alternative<uint8_t>(
            media->second.value()))
        {
            switch (const auto value = static_cast<media_type>(
                std::get<uint8_t>(media->second.value())))
            {
                case media_type::text_html:
                case media_type::application_json:
                {
                    params.erase("media");
                    return value;
                }
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
