// Writes the console/embedded translation units from the built sites.
// Usage: node web/embed.mjs [--web <web directory>] [--out <console/embedded>]
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const options = parse(process.argv.slice(2));
const web = resolve(options.web ?? here);
const out = resolve(options.out ?? join(here, '..', 'console', 'embedded'));

const chunk_size = 8000;
const row_size = 12;
const delimiter = 'DELIM';
const open = `R"${delimiter}(`;
const close = `)${delimiter}"`;

const header =
`/**
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
`;

const assets =
[
    { output: 'admin_html.cpp', container: 'admin_pages', member: 'html', source: 'admin-dashboard/dist/index.html', comment: 'Simple test html for embedded page, links in css and page icon.' },
    { output: 'admin_css.cpp', container: 'admin_pages', member: 'css', source: 'admin-dashboard/dist/style.css', comment: 'Simple test css for embedded page, links in font.' },
    { output: 'admin_ecma.cpp', container: 'admin_pages', member: 'ecma', source: 'admin-dashboard/dist/script.js', comment: 'Simple test ecma script for embedded page.' },
    { output: 'native_html.cpp', container: 'native_pages', member: 'html', source: 'blockexplorer/dist/index.html' },
    { output: 'native_css.cpp', container: 'native_pages', member: 'css', source: 'blockexplorer/dist/style.css' },
    { output: 'native_ecma.cpp', container: 'native_pages', member: 'ecma', source: 'blockexplorer/dist/script.js' },
    { output: 'icon.cpp', container: 'shared_pages', member: 'icon', source: 'shared/favicon.svg', binary: true, comment: 'Icon shared by the embedded sites.' }
];

// Strings are latin1 throughout so that every byte round-trips unchanged.
function parse(args)
{
    const options = {};
    for (let i = 0; i < args.length; i += 2)
    {
        if (!args[i].startsWith('--') || args[i + 1] === undefined)
            fail(`unexpected argument '${args[i]}'`);

        options[args[i].slice(2)] = args[i + 1];
    }

    return options;
}

function fail(message)
{
    console.error(`embed: ${message}`);
    process.exit(1);
}

function text_body(bytes, name)
{
    const content = bytes.replace(/\r\n/g, '\n');
    if (content.includes(close))
        fail(`${name} contains the raw string delimiter ${close}`);

    const chunks = [];
    for (let i = 0; i < content.length; i += chunk_size)
        chunks.push(open + content.slice(i, i + chunk_size) + close);

    return chunks.join('\n') + '\n\n';
}

function binary_body(bytes)
{
    const hex = Array.from(Buffer.from(bytes, 'latin1'), (byte) =>
        '0x' + byte.toString(16).padStart(2, '0'));

    const rows = [];
    for (let i = 0; i < hex.length; i += row_size)
        rows.push('    ' + hex.slice(i, i + row_size).join(', '));

    return '{\n' + rows.join(',\n') + '\n}';
}

function unit(asset, bytes)
{
    const type = asset.binary ? 'uint8_t' : 'char';
    const body = asset.binary ? binary_body(bytes) : text_body(bytes, asset.source);
    const comment = asset.comment ? `// ${asset.comment}\n` : '';
    return header +
        '#include "../embedded/embedded.hpp"\n' +
        '\n' +
        'namespace libbitcoin {\n' +
        'namespace server {\n' +
        '\n' +
        comment +
        `DEFINE_EMBEDDED_PAGE(${asset.container}, ${type}, ${asset.member},\n` +
        body +
        ')\n' +
        '\n' +
        '} // namespace server\n' +
        '} // namespace libbitcoin\n';
}

// Line endings follow the existing output file so a checkout is not churned.
function write(path, content)
{
    const existing = existsSync(path) ? readFileSync(path, 'latin1') : '';
    const styled = existing.includes('\r\n') ? content.replace(/\n/g, '\r\n') : content;
    if (styled === existing)
    {
        console.log(`unchanged : ${path}`);
        return;
    }

    writeFileSync(path, styled, 'latin1');
    console.log(`embedded  : ${path}`);
}

// All units are generated before any is written, so a failure writes nothing.
const units = assets.map((asset) =>
{
    const source = join(web, asset.source);
    if (!existsSync(source))
        fail(`missing built asset ${source}`);

    return { path: join(out, asset.output), content: unit(asset, readFileSync(source, 'latin1')) };
});

for (const { path, content } of units)
    write(path, content);
