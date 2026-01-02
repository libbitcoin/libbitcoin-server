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
DEFINE_EMBEDDED_PAGE(web_pages, char, html,
R"(<html>
<head>
    <title>Libbitcoin Server</title>
    <meta charset="utf-8">
    <meta name="description" content="libbitcoin server admin site">
    <link rel="stylesheet" href="style.css"/>
    <link rel="icon" href="icon.png" type="image/png"/>
    <link rel="preload" href="boston.woff2" type="font/woff2" as="font">
    <script src="script.js" defer></script>
</head>
<body>
    <p>Hello world!</p>
</body>
</html>)")

} // namespace server
} // namespace libbitcoin
