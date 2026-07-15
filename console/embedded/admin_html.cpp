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
#include "../embedded/embedded.hpp"

namespace libbitcoin {
namespace server {

// Simple test html for embedded page, links in css and page icon.
DEFINE_EMBEDDED_PAGE(admin_pages, char, html,
R"(<html>
<head>
    <title>Libbitcoin Server</title>
    <meta charset="utf-8">
    <meta name="description" content="libbitcoin server admin site">
    <link rel="stylesheet" href="style.css"/>
    <script src="script.js" defer></script>
</head>
<body>
    <h1>Admin Console</h1>
    <div id="tabs">
        <button id="show-log">Log</button>
        <button id="show-events">Events</button>
    </div>
    <div id="log-tab">
        <div id="levels"></div>
        <pre id="log"></pre>
    </div>
    <div id="event-tab" hidden>
        <div id="kinds"></div>
        <pre id="events"></pre>
    </div>
    <div id="status">disconnected</div>
</body>
</html>)")

} // namespace server
} // namespace libbitcoin
