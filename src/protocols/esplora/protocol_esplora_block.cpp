/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#include <bitcoin/server/protocols/protocol_esplora.hpp>

#include <bitcoin/server/define.hpp>
#include <bitcoin/server/serializers/bitcoind_json.hpp>

namespace libbitcoin {
namespace server {

#define CLASS protocol_esplora

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// The number of blocks returned by a blocks request.
constexpr size_t block_page = 10;

// Serializers.
// ----------------------------------------------------------------------------

bool protocol_esplora::to_block(boost::json::object& out,
    const database::header_link& link) NOEXCEPT
{
    const auto& query = archive();
    const auto header = query.get_header(link);

    size_t height{}, nominal{}, maximal{};
    if (!header || !query.is_associated(link) ||
        !query.get_height(height, link) ||
        !query.get_block_sizes(nominal, maximal, link))
        return false;

    out =
    {
        { "id", encode_hash(query.get_header_key(link)) },
        { "height", height },
        { "version", header->version() },
        { "timestamp", header->timestamp() },
        { "bits", header->bits() },
        { "nonce", header->nonce() },
        { "difficulty", header->difficulty() },
        { "merkle_root", encode_hash(header->merkle_root()) },
        { "tx_count", query.get_tx_count(link) },
        { "size", maximal },
        { "weight", chain::weighted_size(nominal, maximal) },
        { "mediantime", median_time(query, system_settings(), link) }
    };

    // Genesis has no previous block.
    if (header->previous_block_hash() != null_hash)
        out["previousblockhash"] =
            encode_hash(header->previous_block_hash());

    return true;
}

// Block methods.
// ----------------------------------------------------------------------------

bool protocol_esplora::handle_get_block(const code& ec, interface::block,
    uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    const auto& query = archive();
    const auto link = query.to_header(*hash);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    if (media == data)
    {
        size_t size{};
        if (!query.get_block_size(size, link, true))
        {
            send_not_found();
            return true;
        }

        data_chunk out(size);
        stream::out::fast sink{ out };
        write::bytes::fast writer{ sink };
        if (!query.get_wire_block(writer, link, true))
        {
            send_internal_server_error(database::error::integrity);
            return true;
        }

        send_chunk(std::move(out));
        return true;
    }

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    boost::json::object out{};
    if (!to_block(out, link))
    {
        send_not_found();
        return true;
    }

    send_json(std::move(out), 512);
    return true;
}

bool protocol_esplora::handle_get_block_header(const code& ec,
    interface::block_header, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    const auto header = archive().get_header(archive().to_header(*hash));
    if (!header)
    {
        send_not_found();
        return true;
    }

    constexpr auto size = chain::header::serialized_size();
    std::string out(two * size, '\0');
    stream::out::fast sink{ out };
    write::base16::fast writer{ sink };
    header->to_data(writer);
    send_text(std::move(out));
    return true;
}

bool protocol_esplora::handle_get_block_status(const code& ec,
    interface::block_status, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_header(*hash);

    size_t height{};
    if (link.is_terminal() || !query.get_height(height, link))
    {
        send_not_found();
        return true;
    }

    const auto confirmed = query.is_confirmed_block(link);
    boost::json::object out{ { "in_best_chain", confirmed } };

    if (confirmed && height < query.get_top_confirmed())
        out["next_best"] = encode_hash(
            query.get_header_key(query.to_confirmed(add1(height))));

    send_json(std::move(out), 128);
    return true;
}

bool protocol_esplora::handle_get_block_txids(const code& ec,
    interface::block_txids, uint8_t media, const hash_cptr& hash) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto keys = query.get_tx_keys(query.to_header(*hash));
    if (keys.empty())
    {
        send_not_found();
        return true;
    }

    boost::json::array out(keys.size());
    std::ranges::transform(keys, out.begin(),
        [](const auto& key) { return encode_hash(key); });

    send_json(std::move(out), two * keys.size() * hash_size);
    return true;
}

bool protocol_esplora::handle_get_block_txid(const code& ec,
    interface::block_txid, uint8_t media, const hash_cptr& hash,
    uint32_t index) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto keys = query.get_tx_keys(query.to_header(*hash));
    if (index >= keys.size())
    {
        send_not_found();
        return true;
    }

    send_text(encode_hash(keys.at(index)));
    return true;
}

bool protocol_esplora::handle_get_block_height(const code& ec,
    interface::block_height, uint8_t media, uint32_t height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto link = query.to_confirmed(height);
    if (link.is_terminal())
    {
        send_not_found();
        return true;
    }

    send_text(encode_hash(query.get_header_key(link)));
    return true;
}

bool protocol_esplora::handle_get_blocks(const code& ec, interface::blocks,
    uint8_t media, std::optional<uint32_t> height) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != json)
    {
        send_not_acceptable();
        return true;
    }

    const auto& query = archive();
    const auto top = query.get_top_confirmed();
    auto start = height.value_or(possible_narrow_cast<uint32_t>(top));
    if (start > top)
    {
        send_not_found();
        return true;
    }

    boost::json::array out{};
    for (size_t index{}; index < block_page; ++index)
    {
        boost::json::object object{};
        if (!to_block(object, query.to_confirmed(start)))
            break;

        out.emplace_back(std::move(object));
        if (is_zero(start))
            break;

        --start;
    }

    send_json(std::move(out), block_page * 512);
    return true;
}

bool protocol_esplora::handle_get_tip_height(const code& ec,
    interface::tip_height, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    send_text(serialize(archive().get_top_confirmed()));
    return true;
}

bool protocol_esplora::handle_get_tip_hash(const code& ec,
    interface::tip_hash, uint8_t media) NOEXCEPT
{
    if (stopped(ec))
        return false;

    if (media != text)
    {
        send_not_acceptable();
        return true;
    }

    send_text(encode_hash(archive().get_top_confirmed_hash()));
    return true;
}

BC_POP_WARNING()

} // namespace server
} // namespace libbitcoin
