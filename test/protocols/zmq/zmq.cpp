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
#include "../../test.hpp"
#include "zmq_setup_fixture.hpp"

using namespace system;
using zmtp_stream = network::zmtp::stream;
using zmq_protocol = protocol_bitcoind_zmq;
using topics = interface::bitcoind_zmq;
using tcp_socket = boost::asio::ip::tcp::socket;

// Test infrastructure (synchronous ZMTP subscriber peer).
// ----------------------------------------------------------------------------

// Build a command frame with the given name and trailing content.
static data_chunk command(const std::string& name, const data_chunk& content)
{
    data_chunk body{};
    const auto size = possible_narrow_cast<uint8_t>(name.size());
    body.push_back(size);
    body.insert(body.end(), name.begin(), name.end());
    body.insert(body.end(), content.begin(), content.end());
    return zmtp_stream::frame_encode(body, true, false);
}

// Build a READY command advertising the given socket type.
static data_chunk ready(const std::string& type)
{
    const std::string key{ "Socket-Type" };
    const auto key_size = possible_narrow_cast<uint8_t>(key.size());
    const auto type_size = possible_narrow_cast<uint32_t>(type.size());
    const auto value_size = to_big_endian(type_size);

    data_chunk meta{};
    meta.push_back(key_size);
    meta.insert(meta.end(), key.begin(), key.end());
    meta.insert(meta.end(), value_size.begin(), value_size.end());
    meta.insert(meta.end(), type.begin(), type.end());
    return command("READY", meta);
}

// Synchronously write the whole buffer to the peer socket.
static void peer_write(tcp_socket& peer, const data_chunk& data)
{
    const boost::asio::const_buffer out{ data.data(), data.size() };
    boost::asio::write(peer, out);
}

// Synchronously read one frame (flags and body) from the peer socket.
static void peer_read_frame(tcp_socket& peer, uint8_t& flags,
    data_chunk& body)
{
    uint8_t head{};
    const boost::asio::mutable_buffer in{ &head, sizeof(head) };
    boost::asio::read(peer, in);
    flags = head;

    size_t length{};
    if (is_zero(flags & zmtp_stream::flag_long))
    {
        uint8_t size{};
        const boost::asio::mutable_buffer one{ &size, sizeof(size) };
        boost::asio::read(peer, one);
        length = size;
    }
    else
    {
        data_array<sizeof(uint64_t)> size{};
        const boost::asio::mutable_buffer eight{ size.data(), size.size() };
        boost::asio::read(peer, eight);
        length = possible_narrow_cast<size_t>(from_big_endian<uint64_t>(size));
    }

    body.assign(length, 0x00);
    if (body.empty())
        return;

    const boost::asio::mutable_buffer rest{ body.data(), body.size() };
    boost::asio::read(peer, rest);
}

// Synchronously read one whole (multipart) message from the peer socket.
static data_stack peer_read_message(tcp_socket& peer)
{
    data_stack parts{};
    auto more = true;
    while (more)
    {
        uint8_t flags{};
        data_chunk body{};
        peer_read_frame(peer, flags, body);
        parts.push_back(body);
        more = !is_zero(flags & zmtp_stream::flag_more);
    }

    return parts;
}

// Synchronously perform the peer (SUB) side of the ZMTP handshake.
static void peer_handshake(tcp_socket& peer)
{
    peer_write(peer, zmtp_stream::make_greeting(false, false));

    data_chunk theirs(zmtp_stream::greeting_size, 0x00);
    const boost::asio::mutable_buffer in{ theirs.data(), theirs.size() };
    boost::asio::read(peer, in);
    uint8_t minor{};
    bool curve{};
    bool as_server{};
    BOOST_REQUIRE(zmtp_stream::parse_greeting(theirs, minor, curve, as_server));

    peer_write(peer, ready("SUB"));
    uint8_t flags{};
    data_chunk body{};
    peer_read_frame(peer, flags, body);
    BOOST_REQUIRE(!is_zero(flags & zmtp_stream::flag_command));
}

// Subscribe to a topic (3.1 command dialect).
static void peer_subscribe(tcp_socket& peer, std::string_view topic)
{
    peer_write(peer, command("SUBSCRIBE", { topic.begin(), topic.end() }));
}

// Send a PING and require the echoed PONG, proving all prior frames were
// consumed by the server (the stream is ordered).
static void peer_ping_pong(tcp_socket& peer)
{
    const auto context_bytes = base16_chunk("0011223344556677");
    data_chunk ping{ 0x00, 0x00 };
    ping.insert(ping.end(), context_bytes.begin(), context_bytes.end());
    peer_write(peer, command("PING", ping));

    uint8_t flags{};
    data_chunk body{};
    peer_read_frame(peer, flags, body);
    BOOST_REQUIRE(!is_zero(flags & zmtp_stream::flag_command));

    std::string name{};
    std::span<const uint8_t> echo{};
    const std::span<const uint8_t> frame{ body };
    BOOST_REQUIRE(zmtp_stream::command_name(name, echo, frame));
    BOOST_REQUIRE_EQUAL(name, "PONG");
}

// CURVE peer (synchronous, driven by a client cipher).
// ----------------------------------------------------------------------------

using zmtp_cipher = network::zmtp::cipher;

// Synchronously perform the peer (SUB) side of the CURVE handshake.
static void peer_curve_handshake(tcp_socket& peer, zmtp_cipher& client)
{
    peer_write(peer, zmtp_stream::make_greeting(false, true));
    data_chunk theirs(zmtp_stream::greeting_size, 0x00);
    const boost::asio::mutable_buffer in{ theirs.data(), theirs.size() };
    boost::asio::read(peer, in);

    uint8_t minor{};
    bool curve{};
    bool as_server{};
    BOOST_REQUIRE(zmtp_stream::parse_greeting(theirs, minor, curve, as_server));
    BOOST_REQUIRE(curve);
    BOOST_REQUIRE(as_server);

    data_chunk hello{};
    BOOST_REQUIRE(client.hello(hello));
    peer_write(peer, zmtp_stream::frame_encode(hello, true, false));

    uint8_t flags{};
    data_chunk welcome{};
    peer_read_frame(peer, flags, welcome);
    BOOST_REQUIRE(!is_zero(flags & zmtp_stream::flag_command));

    data_chunk initiate{};
    const auto metadata = zmtp_stream::make_property("Socket-Type", "SUB");
    BOOST_REQUIRE(client.initiate(initiate, welcome, metadata));
    peer_write(peer, zmtp_stream::frame_encode(initiate, true, false));

    data_chunk ready{};
    peer_read_frame(peer, flags, ready);
    BOOST_REQUIRE(!is_zero(flags & zmtp_stream::flag_command));

    data_chunk peer_metadata{};
    BOOST_REQUIRE(client.complete(peer_metadata, ready));

    std::string type{};
    BOOST_REQUIRE(zmtp_stream::ready_socket_type(type, peer_metadata));
    BOOST_REQUIRE_EQUAL(type, "PUB");
}

// Box and send a SUBSCRIBE command (3.1 dialect) for the topic.
static void peer_curve_subscribe(tcp_socket& peer, zmtp_cipher& client,
    std::string_view topic)
{
    const std::string name{ "SUBSCRIBE" };
    data_chunk body{};
    body.push_back(possible_narrow_cast<uint8_t>(name.size()));
    body.insert(body.end(), name.begin(), name.end());
    body.insert(body.end(), topic.begin(), topic.end());

    data_chunk message{};
    BOOST_REQUIRE(client.encode(message, zmtp_cipher::payload_command, body));
    peer_write(peer, zmtp_stream::frame_encode(message, false, false));
}

// Synchronously read and unbox one whole multipart message.
static data_stack peer_curve_read_message(tcp_socket& peer,
    zmtp_cipher& client)
{
    data_stack parts{};
    auto more = true;
    while (more)
    {
        uint8_t flags{};
        data_chunk message{};
        peer_read_frame(peer, flags, message);

        uint8_t payload{};
        data_chunk body{};
        BOOST_REQUIRE(client.decode(payload, body, message));
        parts.push_back(body);
        more = !is_zero(payload & zmtp_cipher::payload_more);
    }

    return parts;
}

// Construct a CURVE client for the fixture server (bob) as alice (rfc7748).
static zmtp_cipher curve_client()
{
    const auto alice_secret = base16_array(
        "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    const auto alice_public = base16_array(
        "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
    const auto bob_public = base16_array(
        "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
    return { alice_secret, alice_public, bob_public };
}

// Send a boxed PING and require the boxed PONG, proving all prior frames
// were consumed by the server (the stream is ordered).
static void peer_curve_ping_pong(tcp_socket& peer, zmtp_cipher& client)
{
    const std::string name{ "PING" };
    data_chunk body{};
    body.push_back(possible_narrow_cast<uint8_t>(name.size()));
    body.insert(body.end(), name.begin(), name.end());
    const auto ping = base16_chunk("00000011223344556677");
    body.insert(body.end(), ping.begin(), ping.end());

    data_chunk message{};
    BOOST_REQUIRE(client.encode(message, zmtp_cipher::payload_command, body));
    peer_write(peer, zmtp_stream::frame_encode(message, false, false));

    uint8_t flags{};
    data_chunk pong{};
    peer_read_frame(peer, flags, pong);

    uint8_t payload{};
    data_chunk reply{};
    BOOST_REQUIRE(client.decode(payload, reply, pong));
    BOOST_REQUIRE(!is_zero(payload & zmtp_cipher::payload_command));

    std::string echo_name{};
    std::span<const uint8_t> echo{};
    const std::span<const uint8_t> frame{ reply };
    BOOST_REQUIRE(zmtp_stream::command_name(echo_name, echo, frame));
    BOOST_REQUIRE_EQUAL(echo_name, "PONG");
}

// Codec (static).
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(zmq_codec_tests)

BOOST_AUTO_TEST_CASE(zmq__subscription__subscribe_command__add_topic)
{
    const auto topic = to_chunk(std::string{ "hashblock" });
    const auto framed = command("SUBSCRIBE", topic);
    zmtp_stream::frame frame
    {
        framed.at(0),
        { std::next(framed.begin(), 2), framed.end() }
    };

    bool add{};
    data_chunk found{};
    BOOST_REQUIRE(zmq_protocol::subscription(add, found, frame));
    BOOST_REQUIRE(add);
    BOOST_REQUIRE_EQUAL(found, topic);
}

BOOST_AUTO_TEST_CASE(zmq__subscription__cancel_command__remove_topic)
{
    const auto topic = to_chunk(std::string{ "rawtx" });
    const auto framed = command("CANCEL", topic);
    zmtp_stream::frame frame
    {
        framed.at(0),
        { std::next(framed.begin(), 2), framed.end() }
    };

    bool add{ true };
    data_chunk found{};
    BOOST_REQUIRE(zmq_protocol::subscription(add, found, frame));
    BOOST_REQUIRE(!add);
    BOOST_REQUIRE_EQUAL(found, topic);
}

BOOST_AUTO_TEST_CASE(zmq__subscription__v30_message__add_topic)
{
    const auto topic = to_chunk(std::string{ "sequence" });
    data_chunk body{ 0x01 };
    body.insert(body.end(), topic.begin(), topic.end());
    const zmtp_stream::frame frame{ 0x00, body };

    bool add{};
    data_chunk found{};
    BOOST_REQUIRE(zmq_protocol::subscription(add, found, frame));
    BOOST_REQUIRE(add);
    BOOST_REQUIRE_EQUAL(found, topic);
}

BOOST_AUTO_TEST_CASE(zmq__subscription__data_message__false)
{
    const zmtp_stream::frame frame{ 0x00, data_chunk{ 0x42, 0x43 } };

    bool add{};
    data_chunk found{};
    BOOST_REQUIRE(!zmq_protocol::subscription(add, found, frame));
}

BOOST_AUTO_TEST_CASE(zmq__subscribed__prefix_and_empty__expected)
{
    const data_stack prefix{ to_chunk(std::string{ "hash" }) };
    BOOST_REQUIRE(zmq_protocol::subscribed(prefix, topics::hash_block));
    BOOST_REQUIRE(zmq_protocol::subscribed(prefix, topics::hash_tx));
    BOOST_REQUIRE(!zmq_protocol::subscribed(prefix, topics::raw_block));

    const data_stack all{ data_chunk{} };
    BOOST_REQUIRE(zmq_protocol::subscribed(all, topics::sequence));

    const data_stack none{};
    BOOST_REQUIRE(!zmq_protocol::subscribed(none, topics::sequence));
}

BOOST_AUTO_TEST_CASE(zmq__notification__topic_body_sequence__three_frames)
{
    const auto body = base16_chunk("deadbeef");
    const auto packet = zmq_protocol::notification(topics::hash_tx, body, 0x01020304);

    // hashtx: flags(MORE) len(6) 6; body: flags(MORE) len(4) 4; seq: flags(0) len(4) 4 (LE).
    BOOST_REQUIRE_EQUAL(packet.size(), 8u + 6u + 6u);
    BOOST_REQUIRE_EQUAL(packet.at(0), zmtp_stream::flag_more);
    BOOST_REQUIRE_EQUAL(packet.at(1), 6u);
    BOOST_REQUIRE_EQUAL(packet.at(8), zmtp_stream::flag_more);
    BOOST_REQUIRE_EQUAL(packet.at(9), 4u);
    BOOST_REQUIRE_EQUAL(packet.at(14), 0x00u);
    BOOST_REQUIRE_EQUAL(packet.at(15), 4u);
    BOOST_REQUIRE_EQUAL(packet.at(16), 0x04u);
    BOOST_REQUIRE_EQUAL(packet.at(17), 0x03u);
    BOOST_REQUIRE_EQUAL(packet.at(18), 0x02u);
    BOOST_REQUIRE_EQUAL(packet.at(19), 0x01u);
}

BOOST_AUTO_TEST_CASE(zmq__sequence_body__reversed_hash_then_label__expected)
{
    const auto hash = base16_hash("000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f");
    const auto body = zmq_protocol::sequence_body(hash, topics::block_connected);

    BOOST_REQUIRE_EQUAL(body.size(), hash_size + 1u);
    BOOST_REQUIRE_EQUAL(body.front(), 0x00u);
    BOOST_REQUIRE_EQUAL(body.at(31), 0x6fu);
    BOOST_REQUIRE_EQUAL(body.back(), 'C');
}

BOOST_AUTO_TEST_SUITE_END()

// Service (server with a subscribed peer).
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(zmq_tests, zmq_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(zmq__handshake__v31_peer__ready)
{
    peer_handshake(socket_);
    peer_ping_pong(socket_);
}

BOOST_AUTO_TEST_CASE(zmq__hashblock__organized__reversed_hash_and_sequence)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, topics::hash_block);
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    const auto expected = to_chunk(reverse_copy(header->hash()));

    notify(node::chase::organized, link);
    const auto first = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(first.size(), 3u);
    BOOST_REQUIRE_EQUAL(first.at(0), to_chunk(std::string{ "hashblock" }));
    BOOST_REQUIRE_EQUAL(first.at(1), expected);
    BOOST_REQUIRE_EQUAL(first.at(2), base16_chunk("00000000"));

    // The per-topic sequence increments after each publication.
    notify(node::chase::organized, link);
    const auto second = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(second.size(), 3u);
    BOOST_REQUIRE_EQUAL(second.at(2), base16_chunk("01000000"));
}

BOOST_AUTO_TEST_CASE(zmq__rawtx__organized__block_transaction_only_subscribed_topic)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, topics::raw_tx);
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto block = query_.get_block(link, true);
    BOOST_REQUIRE(block);
    const auto expected = block->transactions_ptr()->front()->to_data(true);

    // hashblock/rawblock/sequence precede rawtx but are not subscribed.
    notify(node::chase::organized, link);
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk(std::string{ "rawtx" }));
    BOOST_REQUIRE_EQUAL(message.at(1), expected);
}

BOOST_AUTO_TEST_CASE(zmq__sequence__organized__reversed_hash_and_connected_label)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, topics::sequence);
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    auto expected = to_chunk(reverse_copy(header->hash()));
    expected.push_back('C');

    notify(node::chase::organized, link);
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk(std::string{ "sequence" }));
    BOOST_REQUIRE_EQUAL(message.at(1), expected);
}

BOOST_AUTO_TEST_CASE(zmq__maximum_subscriptions__third_subscription_not_recorded)
{
    peer_handshake(socket_);

    // The fixture limit is two; hashblock (third) is not recorded.
    peer_subscribe(socket_, topics::hash_tx);
    peer_subscribe(socket_, topics::raw_tx);
    peer_subscribe(socket_, topics::hash_block);
    peer_ping_pong(socket_);

    // hashblock would precede hashtx if it had been recorded.
    notify(node::chase::organized, query_.to_confirmed(1));
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk(std::string{ "hashtx" }));
}

BOOST_AUTO_TEST_SUITE_END()

// CURVE mechanism.
// ----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE(zmq_curve_tests, zmq_curve_ten_block_setup_fixture)

BOOST_AUTO_TEST_CASE(zmq_curve__handshake__alice_peer__ready)
{
    auto client = curve_client();
    peer_curve_handshake(socket_, client);
}

BOOST_AUTO_TEST_CASE(zmq_curve__handshake__null_peer__error_command)
{
    peer_write(socket_, zmtp_stream::make_greeting(false, false));
    data_chunk theirs(zmtp_stream::greeting_size, 0x00);
    const boost::asio::mutable_buffer in{ theirs.data(), theirs.size() };
    boost::asio::read(socket_, in);

    uint8_t minor{};
    bool curve{};
    bool as_server{};
    BOOST_REQUIRE(zmtp_stream::parse_greeting(theirs, minor, curve, as_server));
    BOOST_REQUIRE(curve);

    uint8_t flags{};
    data_chunk body{};
    peer_read_frame(socket_, flags, body);
    BOOST_REQUIRE(!is_zero(flags & zmtp_stream::flag_command));

    std::string name{};
    std::span<const uint8_t> reason{};
    const std::span<const uint8_t> frame{ body };
    BOOST_REQUIRE(zmtp_stream::command_name(name, reason, frame));
    BOOST_REQUIRE_EQUAL(name, "ERROR");
}

BOOST_AUTO_TEST_CASE(zmq_curve__hashblock__organized__boxed_notification)
{
    auto client = curve_client();
    peer_curve_handshake(socket_, client);
    peer_curve_subscribe(socket_, client, topics::hash_block);
    peer_curve_ping_pong(socket_, client);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    notify(node::chase::organized, node::header_t{ link });

    const auto message = peer_curve_read_message(socket_, client);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), data_chunk(topics::hash_block.begin(), topics::hash_block.end()));
    BOOST_REQUIRE_EQUAL(message.at(1), to_chunk(reverse_copy(header->hash())));
    BOOST_REQUIRE_EQUAL(message.at(2), base16_chunk("00000000"));
}

BOOST_AUTO_TEST_SUITE_END()
