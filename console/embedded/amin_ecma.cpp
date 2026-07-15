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

// Simple test ecma script for embedded page.
DEFINE_EMBEDDED_PAGE(admin_pages, char, ecma,
R"(document.addEventListener('DOMContentLoaded', function()
{
    // Contract: network::levels ordinals (version 1).
    const levels =
    [
        'application', 'news', 'session', 'protocol', 'proxy',
        'remote', 'fault', 'quitting', 'objects', 'verbose'
    ];

    // Contract: node::events ordinals (version 1).
    const events =
    [
        'header_archived', 'header_organized', 'header_reorganized',
        'block_archived', 'block_buffered', 'block_validated',
        'block_confirmed', 'block_unconfirmable', 'validate_bypassed',
        'confirm_bypassed', 'tx_archived', 'tx_validated', 'tx_invalidated',
        'block_organized', 'block_reorganized', 'template_issued',
        'snapshot_secs', 'prune_msecs', 'reload_msecs', 'block_usecs',
        'ancestry_msecs', 'filter_msecs', 'filterhashes_msecs',
        'filterchecks_msecs', 'ecdsa_secs', 'schnorr_secs', 'silent_secs',
        'unknown'
    ];

    const status = document.getElementById('status');

    // Bounded output pane: frames append lines (cheap), the dom renders at
    // most once per animation frame, retention is capped to scrollback.
    function pane(id, scrollback = 1000)
    {
        const element = document.getElementById(id);
        const lines = [];
        let pending = false;

        function render()
        {
            pending = false;
            element.textContent = lines.join('');
            element.scrollTop = element.scrollHeight;
        }

        return function(line)
        {
            lines.push(line);
            if (lines.length > scrollback)
                lines.splice(0, lines.length - scrollback);

            if (!pending)
            {
                pending = true;
                requestAnimationFrame(render);
            }
        };
    }

    const logPane = pane('log');
    const eventPane = pane('events');

    // One socket, upgraded on the page's own endpoint.
    const socket = new WebSocket(
        (location.protocol === 'https:' ? 'wss://' : 'ws://') +
        location.host + '/');

    // Renders a checkbox set; the url parameter carries the channel mask.
    function channel(names, containerId, param, target)
    {
        const boxes = [];
        const container = document.getElementById(containerId);
        const initial = Number(new URLSearchParams(location.search)
            .get(param) ?? 0);

        // Filter mask from checkboxes (2 ** ordinal, exact below 2 ** 53).
        function mask()
        {
            return boxes.reduce((sum, box, bit) =>
                sum + (box.checked ? 2 ** bit : 0), 0);
        }

        function subscribe()
        {
            // Mask state rides the page url (bookmarkable, survives refresh).
            const url = new URLSearchParams(location.search);
            url.set(param, mask());
            history.replaceState(null, '', location.pathname + '?' + url);

            if (socket.readyState === WebSocket.OPEN)
                socket.send(target + '?filter=' + mask());
        }

        names.forEach((name, bit) =>
        {
            const label = document.createElement('label');
            const box = document.createElement('input');
            box.type = 'checkbox';
            box.checked = (initial / 2 ** bit) % 2 >= 1;
            box.addEventListener('change', subscribe);
            label.append(box, ' ', name, ' ');
            container.append(label);
            boxes.push(box);
        });

        return { boxes, subscribe };
    }

    const logs = channel(levels, 'levels', 'filter', '/v1/log/subscribe');
    const kinds = channel(events, 'kinds', 'events', '/v1/event/subscribe');

    // Tab switching.
    const logTab = document.getElementById('log-tab');
    const eventTab = document.getElementById('event-tab');

    document.getElementById('show-log').addEventListener('click', () =>
    {
        logTab.hidden = false;
        eventTab.hidden = true;
    });

    document.getElementById('show-events').addEventListener('click', () =>
    {
        logTab.hidden = true;
        eventTab.hidden = false;
    });

    socket.addEventListener('open', () =>
    {
        status.textContent = 'connected';
        logs.subscribe();
        kinds.subscribe();
    });

    socket.addEventListener('close', () =>
    {
        status.textContent = 'disconnected';
    });

    socket.addEventListener('message', (frame) =>
    {
        const message = JSON.parse(frame.data);

        // Ack: previous filter (confirmation only, rough page ignores it).
        if ('previous' in message)
            return;

        // Log frame: time is zulu milliseconds (js native), second granular.
        if ('level' in message)
        {
            // Dump frames deselected client-side (in-flight on old filter).
            if (!logs.boxes[message.level]?.checked)
                return;

            const time = new Date(message.time).toLocaleTimeString();
            logPane(time + ' [' + levels[message.level] + '] ' +
                message.message);
            return;
        }

        // Event frame: time is zulu milliseconds (js native), value is
        // event-defined data (height, timespan in named units, etc.).
        if ('event' in message)
        {
            // Dump frames deselected client-side (in-flight on old filter).
            if (!kinds.boxes[message.event]?.checked)
                return;

            const time = new Date(message.time).toLocaleTimeString() + '.' +
                String(message.time % 1000).padStart(3, '0');
            eventPane(time + ' [' + events[message.event] + '] ' +
                message.value + '\n');
        }
    });
});)")

} // namespace server
} // namespace libbitcoin
