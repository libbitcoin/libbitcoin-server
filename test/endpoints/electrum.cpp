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
#include "../test.hpp"

#include <future>

using namespace system;

constexpr auto block1_hash = base16_hash("00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048");
constexpr auto block2_hash = base16_hash("000000006a625f06636b8bb6ac7b960a8d03705d1ace08b1a19da3fdcc99ddbd");
constexpr auto block3_hash = base16_hash("0000000082b5015589a3fdf2d4baff403e6f0be035a5d9742c1cae6295464449");
constexpr auto block4_hash = base16_hash("000000004ebadb55ee9096c9a2f8880e09da59c0d68b1c228da88e48844a1485");
constexpr auto block5_hash = base16_hash("000000009b7262315dbf071787ad3656097b892abffd1f95a1a022f896f533fc");
constexpr auto block6_hash = base16_hash("000000003031a0e73735690c5a1ff2a4be82553b2a12b776fbd3a215dc8f778d");
constexpr auto block7_hash = base16_hash("0000000071966c2b1d065fd446b1e485b2c9d9594acd2007ccbd5441cfc89444");
constexpr auto block8_hash = base16_hash("00000000408c48f847aa786c2268fc3e6ec2af68e8468a34a28c61b7f1de0dc6");

// blockchain.info/rawblock/[block-hash]?format=hex
constexpr auto block1_data = base16_array("010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e61bc6649ffff001d01e362990101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d0104ffffffff0100f2052a0100000043410496b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73a82cbf2342c858eeac00000000");
constexpr auto block2_data = base16_array("010000004860eb18bf1b1620e37e9490fc8a427514416fd75159ab86688e9a8300000000d5fdcc541e25de1c7a5addedf24858b8bb665c9f36ef744ee42c316022c90f9bb0bc6649ffff001d08d2bd610101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d010bffffffff0100f2052a010000004341047211a824f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073dee6c89064984f03385237d92167c13e236446b417ab79a0fcae412ae3316b77ac00000000");
constexpr auto block3_data = base16_array("01000000bddd99ccfda39da1b108ce1a5d70038d0a967bacb68b6b63065f626a0000000044f672226090d85db9a9f2fbfe5f0f9609b387af7be5b7fbb7a1767c831c9e995dbe6649ffff001d05e0ed6d0101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d010effffffff0100f2052a0100000043410494b9d3e76c5b1629ecf97fff95d7a4bbdac87cc26099ada28066c6ff1eb9191223cd897194a08d0c2726c5747f1db49e8cf90e75dc3e3550ae9b30086f3cd5aaac00000000");
constexpr auto block4_data = base16_array("010000004944469562ae1c2c74d9a535e00b6f3e40ffbad4f2fda3895501b582000000007a06ea98cd40ba2e3288262b28638cec5337c1456aaf5eedc8e9e5a20f062bdf8cc16649ffff001d2bfee0a90101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d011affffffff0100f2052a01000000434104184f32b212815c6e522e66686324030ff7e5bf08efb21f8b00614fb7690e19131dd31304c54f37baa40db231c918106bb9fd43373e37ae31a0befc6ecaefb867ac00000000");
constexpr auto block5_data = base16_array("0100000085144a84488ea88d221c8bd6c059da090e88f8a2c99690ee55dbba4e00000000e11c48fecdd9e72510ca84f023370c9a38bf91ac5cae88019bee94d24528526344c36649ffff001d1d03e4770101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d0120ffffffff0100f2052a0100000043410456579536d150fbce94ee62b47db2ca43af0a730a0467ba55c79e2a7ec9ce4ad297e35cdbb8e42a4643a60eef7c9abee2f5822f86b1da242d9c2301c431facfd8ac00000000");
constexpr auto block6_data = base16_array("01000000fc33f596f822a0a1951ffdbf2a897b095636ad871707bf5d3162729b00000000379dfb96a5ea8c81700ea4ac6b97ae9a9312b2d4301a29580e924ee6761a2520adc46649ffff001d189c4c970101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d0123ffffffff0100f2052a0100000043410408ce279174b34c077c7b2043e3f3d45a588b85ef4ca466740f848ead7fb498f0a795c982552fdfa41616a7c0333a269d62108588e260fd5a48ac8e4dbf49e2bcac00000000");
constexpr auto block7_data = base16_array("010000008d778fdc15a2d3fb76b7122a3b5582bea4f21f5a0c693537e7a03130000000003f674005103b42f984169c7d008370967e91920a6a5d64fd51282f75bc73a68af1c66649ffff001d39a59c860101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d012bffffffff0100f2052a01000000434104a59e64c774923d003fae7491b2a7f75d6b7aa3f35606a8ff1cf06cd3317d16a41aa16928b1df1f631f31f28c7da35d4edad3603adb2338c4d4dd268f31530555ac00000000");
constexpr auto block8_data = base16_array("010000004494c8cf4154bdcc0720cd4a59d9c9b285e4b146d45f061d2b6c967100000000e3855ed886605b6d4a99d5fa2ef2e9b0b164e63df3c4136bebf2d0dac0f1f7a667c86649ffff001d1c4b56660101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d012cffffffff0100f2052a01000000434104cc8d85f5e7933cb18f13b97d165e1189c1fb3e9c98b0dd5446b2a1989883ff9e740a8a75da99cc59a21016caf7a7afd3e4e9e7952983e18d1ff70529d62e0ba1ac00000000");

static const auto genesis = system::settings{ chain::selection::mainnet }.genesis_block;
static const chain::block block1{ block1_data, true };
static const chain::block block2{ block2_data, true };
static const chain::block block3{ block3_data, true };
static const chain::block block4{ block4_data, true };
static const chain::block block5{ block5_data, true };
static const chain::block block6{ block6_data, true };
static const chain::block block7{ block7_data, true };
static const chain::block block8{ block8_data, true };

static const server::settings::embedded_pages admin{};
static const server::settings::embedded_pages native{};

struct electrum_setup_fixture
{
    using context_t = database::context;
    using store_t = database::store<database::map>;
    using query_t = database::query<database::store<database::map>>;

    DELETE_COPY_MOVE(electrum_setup_fixture);

    electrum_setup_fixture() NOEXCEPT
      : config_{ chain::selection::mainnet, native, admin },
        store_
        {
            [&]() NOEXCEPT -> const database::settings&
            {
                config_.database.path = TEST_DIRECTORY;
                return config_.database;
            }()
        },
        query_{ store_ }, log_{},
        server_{ query_, config_, log_ }
    {
        BOOST_REQUIRE(test::clear(test::directory));

        auto& database_settings = config_.database;
        auto& network_settings = config_.network;
        auto& node_settings = config_.node;
        auto& server_settings = config_.server;
        auto& electrum = server_settings.electrum;

        // >>>>>>>>>>>>>>> REQUIRES LOCALHOST TCP PORT 65000. <<<<<<<<<<<<<<<
        electrum.binds = { { "127.0.0.1:65000" } };
        electrum.maximum_headers = 5;
        electrum.connections = 1;
        database_settings.interval_depth = 2;
        node_settings.delay_inbound = false;
        network_settings.inbound.connections = 0;
        network_settings.outbound.connections = 0;

        // Create and populate the store.
        BOOST_REQUIRE(!store_.create([](auto, auto){}));
        BOOST_REQUIRE(setup_eight_block_store());

        // Run the server.
        std::promise<code> running{};
        server_.run([&](const code& ec) NOEXCEPT
        {
            running.set_value(ec);
        });

        // Block until server is running.
        BOOST_REQUIRE(!running.get_future().get());
        socket_.connect(electrum.binds.back().to_endpoint());
    }

    ~electrum_setup_fixture() NOEXCEPT
    {
        socket_.close();
        server_.close();
        BOOST_REQUIRE(!store_.close([](auto, auto){}));
        BOOST_REQUIRE(test::clear(test::directory));
    }

    const configuration& config() const NOEXCEPT
    {
        return config_;
    }

    auto get(const std::string& request) NOEXCEPT
    {
        socket_.send(boost::asio::buffer(request));
        boost::asio::streambuf stream{};
        read_until(socket_, stream, '\n');

        std::string response{};
        std::istream response_stream{ &stream };
        std::getline(response_stream, response);

        return boost::json::parse(response);
    }

private:
    bool setup_eight_block_store() NOEXCEPT
    {
        return query_.initialize(genesis) &&
            query_.set(block1, context_t{ 0, 1, 0 }, false, false) &&
            query_.set(block2, context_t{ 0, 2, 0 }, false, false) &&
            query_.set(block3, context_t{ 0, 3, 0 }, false, false) &&
            query_.set(block4, context_t{ 0, 4, 0 }, false, false) &&
            query_.set(block5, context_t{ 0, 5, 0 }, false, false) &&
            query_.set(block6, context_t{ 0, 6, 0 }, false, false) &&
            query_.set(block7, context_t{ 0, 7, 0 }, false, false) &&
            query_.set(block8, context_t{ 0, 8, 0 }, false, false) &&
            query_.push_confirmed(query_.to_header(block1_hash), false) &&
            query_.push_confirmed(query_.to_header(block2_hash), false) &&
            query_.push_confirmed(query_.to_header(block3_hash), false) &&
            query_.push_confirmed(query_.to_header(block4_hash), false) &&
            query_.push_confirmed(query_.to_header(block5_hash), false) &&
            query_.push_confirmed(query_.to_header(block6_hash), false) &&
            query_.push_confirmed(query_.to_header(block7_hash), false) &&
            query_.push_confirmed(query_.to_header(block8_hash), false);
    }

    configuration config_;
    store_t store_;
    query_t query_;
    network::logger log_;
    server::server_node server_;
    boost::asio::io_context io{};
    boost::asio::ip::tcp::socket socket_{ io };
};

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

BOOST_AUTO_TEST_CASE(electrum__server_version__default__1_4)
{
    const auto response = get(R"({"id":42,"method":"server.version","params":["foobar"]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 42);
    BOOST_REQUIRE(response.at("result").is_array());

    const auto& result = response.at("result").as_array();
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    BOOST_REQUIRE(result.at(0).is_string());
    BOOST_REQUIRE(result.at(1).is_string());
    BOOST_REQUIRE_EQUAL(result.at(0).as_string(), config().network.user_agent);
    BOOST_REQUIRE_EQUAL(result.at(1).as_string(), "1.4");
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_block_header__genesis__expected)
{
    get(R"({"id":"name","method":"server.version","params":["foobar","1.4"]})" "\n");

    const auto response = get(R"({"id":43,"method":"blockchain.block.header","params":[0]})" "\n");
    BOOST_REQUIRE_EQUAL(response.at("id").as_int64(), 43);
    BOOST_REQUIRE(response.at("result").is_object());

    const auto& result = response.at("result").as_object();
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_REQUIRE(result.at("header").is_string());
    BOOST_REQUIRE_EQUAL(result.at("header").as_string(), encode_base16(genesis.header().to_data()));
}

BOOST_AUTO_TEST_SUITE_END()
