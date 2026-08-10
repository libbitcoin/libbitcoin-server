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
#include "../test.hpp"

BOOST_AUTO_TEST_SUITE(descriptor_tests)

// Vectors from bitcoind (rpc example and descriptor tests).

BOOST_AUTO_TEST_CASE(descriptor__checksum__wpkh__expected)
{
    BOOST_REQUIRE_EQUAL(server::descriptor_checksum("wpkh([d34db33f/84h/0h/0h]xpub6DJ2dNUysrn5Vt36jH2KLBT2i1auw1tTSSomg8PhqNiUtx8QX2SvC9nrHu81fT41fvDUnhMjEzQgXnQjKEu3oaqMSzhSrHMxyyoEAmUHQbY/0/*)"), "cjjspncu");
}

BOOST_AUTO_TEST_CASE(descriptor__checksum__sh_multi_2__expected)
{
    BOOST_REQUIRE_EQUAL(server::descriptor_checksum("sh(multi(2,[00000000/111'/222]xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL,xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHBaohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y/0))"), "tjg09x5t");
}

BOOST_AUTO_TEST_CASE(descriptor__checksum__sh_multi_3__expected)
{
    BOOST_REQUIRE_EQUAL(server::descriptor_checksum("sh(multi(3,[00000000/111'/222]xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL,xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHBaohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y/0))"), "d4x0uxyv");
}

// A character outside the descriptor character set has no checksum.
BOOST_AUTO_TEST_CASE(descriptor__checksum__invalid_character__empty)
{
    BOOST_REQUIRE(server::descriptor_checksum("raw(\x01)").empty());
}

BOOST_AUTO_TEST_SUITE_END()
