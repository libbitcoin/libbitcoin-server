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

constexpr size_t small_header_size = 215;
using header_t = system::data_array<small_header_size>;
using store_t = database::store<database::map>;
using query_t = database::query<database::store<database::map>>;

extern const server::settings::embedded_pages admin;
extern const server::settings::embedded_pages native;

extern const system::hash_digest block1_hash;
extern const system::hash_digest block2_hash;
extern const system::hash_digest block3_hash;
extern const system::hash_digest block4_hash;
extern const system::hash_digest block5_hash;
extern const system::hash_digest block6_hash;
extern const system::hash_digest block7_hash;
extern const system::hash_digest block8_hash;
extern const system::hash_digest block9_hash;

extern const header_t block1_data;
extern const header_t block2_data;
extern const header_t block3_data;
extern const header_t block4_data;
extern const header_t block5_data;
extern const header_t block6_data;
extern const header_t block7_data;
extern const header_t block8_data;
extern const header_t block9_data;

extern const system::chain::block genesis;
extern const system::chain::block block1;
extern const system::chain::block block2;
extern const system::chain::block block3;
extern const system::chain::block block4;
extern const system::chain::block block5;
extern const system::chain::block block6;
extern const system::chain::block block7;
extern const system::chain::block block8;
extern const system::chain::block block9;
