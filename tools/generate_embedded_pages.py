#!/usr/bin/env python3
"""
Generate the embedded web pages for libbitcoin-server.

Source: web/admin-dashboard and web/blockexplorer, each installed exactly from
        its package-lock.json and built by its package.json build script,
        and web/shared/favicon.svg.
Output: console/embedded/{admin,native}_{html,css,ecma}.cpp, icon.cpp

Run from the repository root:
  python3 tools/generate_embedded_pages.py

Requires node and npm (and network access for the package install). npm on
the path is used, or the --npm argument, otherwise on Windows the node that
Visual Studio bundles.
"""

import argparse
import os
import shutil
import subprocess
import sys

WEB = "web"
OUTPUT = os.path.join("console", "embedded")
SITES = ["admin-dashboard", "blockexplorer"]
CHUNK = 8000
COLUMNS = 12
OPEN = b'R"DELIM('
CLOSE = b')DELIM"'

LICENSE_HEADER = """\
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
"""

# (output, container, member, source relative to web, binary)
PAGES = [
    ("admin_html.cpp", "admin_pages", "html",
        "admin-dashboard/dist/index.html", False),
    ("admin_css.cpp", "admin_pages", "css",
        "admin-dashboard/dist/style.css", False),
    ("admin_ecma.cpp", "admin_pages", "ecma",
        "admin-dashboard/dist/script.js", False),
    ("native_html.cpp", "native_pages", "html",
        "blockexplorer/dist/index.html", False),
    ("native_css.cpp", "native_pages", "css",
        "blockexplorer/dist/style.css", False),
    ("native_ecma.cpp", "native_pages", "ecma",
        "blockexplorer/dist/script.js", False),
    ("icon.cpp", "shared_pages", "icon",
        "shared/favicon.svg", True)]

# ── build ───────────────────────────────────────────────────────────────────

def visual_studio_npm():
    """The npm bundled with the latest Visual Studio, or None."""
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", ""),
        "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.exists(vswhere):
        return None
    install = subprocess.run([vswhere, "-latest", "-property",
        "installationPath"], capture_output=True, text=True).stdout.strip()
    if not install:
        return None
    npm = os.path.join(install, "MSBuild", "Microsoft", "VisualStudio",
        "NodeJs", "npm.cmd")
    return npm if os.path.exists(npm) else None


def find_npm(npm):
    """The npm executable, from the argument, the path or Visual Studio."""
    found = shutil.which(npm or "npm")
    if found is None and npm is None and os.name == "nt":
        found = visual_studio_npm()
    if found is None:
        sys.exit("npm not found.")
    return found


def build_site(npm, site):
    """Install the site exactly from its lock file and build it."""
    # Package scripts resolve node from the path, so npm's node goes first.
    environment = dict(os.environ)
    environment["PATH"] = os.pathsep.join([os.path.dirname(npm),
        environment.get("PATH", "")])

    directory = os.path.join(WEB, site)
    for arguments in [["ci", "--ignore-scripts", "--no-audit", "--no-fund"],
        ["run", "build"]]:
        print("{}: npm {}".format(site, " ".join(arguments)), flush=True)
        result = subprocess.run([npm] + arguments, cwd=directory,
            env=environment)
        if result.returncode != 0:
            sys.exit("{} build failed.".format(site))

# ── emit ────────────────────────────────────────────────────────────────────

def text_body(data, source):
    """Raw string literals of fixed size, line endings normalized to LF."""
    content = data
    while b"\r\n" in content:
        content = content.replace(b"\r\n", b"\n")
    if CLOSE in content:
        sys.exit("{} contains the raw string delimiter.".format(source))

    chunks = [OPEN + content[i:i + CHUNK] + CLOSE
        for i in range(0, len(content), CHUNK)]
    return b"\n".join(chunks) + b"\n\n"


def binary_body(data):
    """Byte array initializer, bytes unchanged."""
    rows = []
    for row in range(0, len(data), COLUMNS):
        part = data[row:row + COLUMNS]
        rows.append("    " + ", ".join("0x{:02x}".format(byte)
            for byte in part))
    return ("{\n" + ",\n".join(rows) + "\n}").encode("ascii")


def generate_cpp(container, member, source, binary, data):
    """The embedding source for one page."""
    site = source.split("/")[0]
    kind = "uint8_t" if binary else "char"
    body = binary_body(data) if binary else text_body(data, source)
    prefix = "\n".join([LICENSE_HEADER.rstrip("\n"),
        '#include "../embedded/embedded.hpp"',
        "",
        "namespace libbitcoin {",
        "namespace server {",
        "",
        "// Generated by tools/generate_embedded_pages.py from web/{}."
            .format(site),
        "DEFINE_EMBEDDED_PAGE({}, {}, {},".format(container, kind, member),
        ""])
    suffix = "\n".join([")",
        "",
        "} // namespace server",
        "} // namespace libbitcoin",
        ""])
    return prefix.encode("ascii") + body + suffix.encode("ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    parser.add_argument("--npm", help="npm executable")
    options = parser.parse_args()

    if not os.path.isdir(WEB) or not os.path.isdir(OUTPUT):
        sys.exit("Run from the repository root.")

    npm = find_npm(options.npm)
    for site in SITES:
        build_site(npm, site)

    # All pages are generated before any is written.
    pages = []
    for output, container, member, source, binary in PAGES:
        path = os.path.join(WEB, *source.split("/"))
        if not os.path.exists(path):
            sys.exit("Missing {}.".format(path))
        with open(path, "rb") as file:
            data = file.read()
        pages.append((os.path.join(OUTPUT, output),
            generate_cpp(container, member, source, binary, data)))

    for path, content in pages:
        with open(path, "wb") as file:
            file.write(content)
        print("wrote {}".format(path))


if __name__ == "__main__":
    main()
