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
using peer_socket = boost::asio::ip::tcp::socket;

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
static void peer_write(peer_socket& peer, const data_chunk& data)
{
    const boost::asio::const_buffer out{ data.data(), data.size() };
    boost::asio::write(peer, out);
}

// Synchronously read one frame (flags and body) from the peer socket.
static void peer_read_frame(peer_socket& peer, uint8_t& flags,
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
static data_stack peer_read_message(peer_socket& peer)
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

// Synchronously read to require that the server has closed the connection.
static void peer_read_closed(peer_socket& peer)
{
    uint8_t byte{};
    network::boost_code ec{};
    const boost::asio::mutable_buffer in{ &byte, sizeof(byte) };
    boost::asio::read(peer, in, ec);
    BOOST_REQUIRE(ec);
}

// Synchronously perform the peer (SUB) side of the ZMTP handshake.
static void peer_handshake(peer_socket& peer)
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
static void peer_subscribe(peer_socket& peer, std::string_view topic)
{
    peer_write(peer, command("SUBSCRIBE", { topic.begin(), topic.end() }));
}

// Cancel a topic subscription (3.1 command dialect).
static void peer_cancel(peer_socket& peer, std::string_view topic)
{
    peer_write(peer, command("CANCEL", { topic.begin(), topic.end() }));
}

// Send a PING and require the echoed PONG, proving all prior frames were
// consumed by the server (the stream is ordered).
static void peer_ping_pong(peer_socket& peer)
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
static void peer_curve_handshake(peer_socket& peer, zmtp_cipher& client)
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
static void peer_curve_subscribe(peer_socket& peer, zmtp_cipher& client,
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
static data_stack peer_curve_read_message(peer_socket& peer,
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
static void peer_curve_ping_pong(peer_socket& peer, zmtp_cipher& client)
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

// Synchronously attempt the CURVE handshake, returning the frame following
// INITIATE (READY if authorized, otherwise ERROR).
static data_chunk peer_curve_initiate(peer_socket& peer, zmtp_cipher& client)
{
    peer_write(peer, zmtp_stream::make_greeting(false, true));
    data_chunk theirs(zmtp_stream::greeting_size, 0x00);
    const boost::asio::mutable_buffer in{ theirs.data(), theirs.size() };
    boost::asio::read(peer, in);

    uint8_t minor{};
    bool curve{};
    bool as_server{};
    BOOST_REQUIRE(zmtp_stream::parse_greeting(theirs, minor, curve, as_server));

    data_chunk hello{};
    BOOST_REQUIRE(client.hello(hello));
    peer_write(peer, zmtp_stream::frame_encode(hello, true, false));

    uint8_t flags{};
    data_chunk welcome{};
    peer_read_frame(peer, flags, welcome);

    data_chunk initiate{};
    const auto metadata = zmtp_stream::make_property("Socket-Type", "SUB");
    BOOST_REQUIRE(client.initiate(initiate, welcome, metadata));
    peer_write(peer, zmtp_stream::frame_encode(initiate, true, false));

    data_chunk reply{};
    peer_read_frame(peer, flags, reply);
    BOOST_REQUIRE(!is_zero(flags & zmtp_stream::flag_command));
    return reply;
}

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
    peer_subscribe(socket_, "hashblock");
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    const auto expected = to_chunk(reverse_copy(header->hash()));

    notify(node::chase::organized, node::header_t{ link });
    const auto first = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(first.size(), 3u);
    BOOST_REQUIRE_EQUAL(first.at(0), to_chunk(std::string{ "hashblock" }));
    BOOST_REQUIRE_EQUAL(first.at(1), expected);
    BOOST_REQUIRE_EQUAL(first.at(2), base16_chunk("00000000"));

    // The per-topic sequence increments after each publication.
    notify(node::chase::organized, node::header_t{ link });
    const auto second = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(second.size(), 3u);
    BOOST_REQUIRE_EQUAL(second.at(2), base16_chunk("01000000"));
}

BOOST_AUTO_TEST_CASE(zmq__rawtx__organized__block_transaction_only_subscribed_topic)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, "rawtx");
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto block = query_.get_block(link, true);
    BOOST_REQUIRE(block);
    const auto expected = block->transactions_ptr()->front()->to_data(true);

    // hashblock/rawblock/sequence precede rawtx but are not subscribed.
    notify(node::chase::organized, node::header_t{ link });
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk("rawtx"));
    BOOST_REQUIRE_EQUAL(message.at(1), expected);
}

BOOST_AUTO_TEST_CASE(zmq__sequence__organized__reversed_hash_and_connected_label)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, "sequence");
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    auto expected = to_chunk(reverse_copy(header->hash()));
    expected.push_back('C');

    notify(node::chase::organized, node::header_t{ link });
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk("sequence"));
    BOOST_REQUIRE_EQUAL(message.at(1), expected);
}

BOOST_AUTO_TEST_CASE(zmq__subscribe__empty_prefix__all_topics_published)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, "");
    peer_ping_pong(socket_);

    // The empty prefix matches every topic, published in this order.
    notify(node::chase::organized, node::header_t{ query_.to_confirmed(1) });
    BOOST_REQUIRE_EQUAL(peer_read_message(socket_).at(0), to_chunk("hashtx"));
    BOOST_REQUIRE_EQUAL(peer_read_message(socket_).at(0), to_chunk("rawtx"));
    BOOST_REQUIRE_EQUAL(peer_read_message(socket_).at(0), to_chunk("hashblock"));
    BOOST_REQUIRE_EQUAL(peer_read_message(socket_).at(0), to_chunk("rawblock"));
    BOOST_REQUIRE_EQUAL(peer_read_message(socket_).at(0), to_chunk("sequence"));
}

BOOST_AUTO_TEST_CASE(zmq__rawtx__reorganized__block_transaction_published)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, "rawtx");
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto block = query_.get_block(link, true);
    BOOST_REQUIRE(block);
    const auto expected = block->transactions_ptr()->front()->to_data(true);

    notify(node::chase::reorganized, node::header_t{ link });
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk("rawtx"));
    BOOST_REQUIRE_EQUAL(message.at(1), expected);
}

BOOST_AUTO_TEST_CASE(zmq__sequence__reorganized__reversed_hash_and_disconnected_label)
{
    peer_handshake(socket_);
    peer_subscribe(socket_, "sequence");
    peer_ping_pong(socket_);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    auto expected = to_chunk(reverse_copy(header->hash()));
    expected.push_back('D');

    notify(node::chase::reorganized, node::header_t{ link });
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk("sequence"));
    BOOST_REQUIRE_EQUAL(message.at(1), expected);
}

BOOST_AUTO_TEST_CASE(zmq__maximum_subscriptions__third_subscription__channel_closed)
{
    peer_handshake(socket_);

    // The fixture limit is two, and there is no response to carry a reject.
    peer_subscribe(socket_, "hashtx");
    peer_subscribe(socket_, "rawtx");
    peer_subscribe(socket_, "hashblock");
    peer_read_closed(socket_);
}

BOOST_AUTO_TEST_CASE(zmq__maximum_subscriptions__cancelled__subscription_released)
{
    peer_handshake(socket_);

    // The fixture limit is two, and the cancels release both subscriptions.
    peer_subscribe(socket_, "hashtx");
    peer_subscribe(socket_, "rawtx");
    peer_cancel(socket_, "hashtx");
    peer_cancel(socket_, "rawtx");
    peer_subscribe(socket_, "hashblock");
    peer_ping_pong(socket_);

    // The cancelled topics precede hashblock but are no longer published.
    notify(node::chase::organized, node::header_t{ query_.to_confirmed(1) });
    const auto message = peer_read_message(socket_);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk("hashblock"));
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
    peer_curve_subscribe(socket_, client, "hashblock");
    peer_curve_ping_pong(socket_, client);

    const auto link = query_.to_confirmed(1);
    const auto header = query_.get_header(link);
    BOOST_REQUIRE(header);
    notify(node::chase::organized, node::header_t{ link });

    const auto message = peer_curve_read_message(socket_, client);
    BOOST_REQUIRE_EQUAL(message.size(), 3u);
    BOOST_REQUIRE_EQUAL(message.at(0), to_chunk("hashblock"));
    BOOST_REQUIRE_EQUAL(message.at(1), to_chunk(reverse_copy(header->hash())));
    BOOST_REQUIRE_EQUAL(message.at(2), base16_chunk("00000000"));
}


BOOST_AUTO_TEST_CASE(zmq_curve__handshake__unauthorized_client__error_400)
{
    zmtp_cipher::key secret{};
    zmtp_cipher::key public_key{};
    x25519::generate(secret, public_key);
    zmtp_cipher stranger{ secret, public_key, base16_array(ZMQ_CURVE_SERVER) };

    const auto reply = peer_curve_initiate(socket_, stranger);
    std::string name{};
    std::span<const uint8_t> reason{};
    const std::span<const uint8_t> frame{ reply };
    BOOST_REQUIRE(zmtp_stream::command_name(name, reason, frame));
    BOOST_REQUIRE_EQUAL(name, "ERROR");
    BOOST_REQUIRE_EQUAL(std::string(std::next(reason.begin()), reason.end()), "400");
}

BOOST_AUTO_TEST_SUITE_END()
