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
#include "electrum.hpp"

////using namespace system;
////static const code not_found{ server::error::not_found };
////static const code wrong_version{ server::error::wrong_version };
////static const code not_implemented{ server::error::not_implemented };
////static const code invalid_argument{ server::error::invalid_argument };

BOOST_FIXTURE_TEST_SUITE(electrum_tests, electrum_ten_block_setup_fixture)

// blockchain.address.subscribe
// blockchain.scripthash.subscribe
// blockchain.scripthash.unsubscribe
// blockchain.scriptpubkey.subscribe
// blockchain.scriptpubkey.unsubscribe

BOOST_AUTO_TEST_SUITE_END()
