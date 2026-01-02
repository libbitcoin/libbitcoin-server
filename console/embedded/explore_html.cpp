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
#include "../embedded/embedded.hpp"

namespace libbitcoin {
namespace server {

// Simple test html for embedded page, links in css and page icon.
DEFINE_EMBEDDED_PAGE(explore_pages, char, html,
    R"DELIM(<!doctype html>
<html lang="en" class="h-full">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>libbitcoin Explorer</title>
    <script type="module" crossorigin src="/script.js"></script>
    <link rel="stylesheet" crossorigin href="/style.css">
  </head>
  <body class="h-full bg-background text-white">
    <div id="root" class="h-full"></div>
  </body>
</html>
)DELIM"

)

} // namespace server
} // namespace libbitcoin
