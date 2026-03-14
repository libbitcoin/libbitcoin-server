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
#include "../../test.hpp"
#include "../blocks.hpp"
#include "electrum.hpp"
#include <boost/format.hpp>

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_setup_fixture)

// blockchain.transaction.broadcast

using namespace system;
static const code invalid_argument{ server::error::invalid_argument };
static const code unconfirmable_transaction{ server::error::unconfirmable_transaction };

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__empty__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":74,"method":"blockchain.transaction.broadcast","params":[""]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__invalid_encoding__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":75,"method":"blockchain.transaction.broadcast","params":["deadbeef"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__invalid_tx__invalid_argument)
{
    BOOST_CHECK(handshake());

    const auto response = get(R"({"id":76,"method":"blockchain.transaction.broadcast","params":["0100000001"]})" "\n");
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), invalid_argument.value());
}

BOOST_AUTO_TEST_CASE(electrum__blockchain_transaction_broadcast__genesis_coinbase__unconfirmable_transaction)
{
    BOOST_CHECK(handshake());

    const auto tx0_text = encode_base16(genesis.transactions_ptr()->front()->to_data(true));
    constexpr auto request = R"({"id":73,"method":"blockchain.transaction.broadcast","params":["%1%"]})" "\n";
    const auto response = get((boost::format(request) % tx0_text).str());
    BOOST_CHECK_EQUAL(response.at("error").as_object().at("code").as_int64(), unconfirmable_transaction.value());
}

BOOST_AUTO_TEST_SUITE_END()
