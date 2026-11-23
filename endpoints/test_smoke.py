"""
Smoke tests for any running libbitcoin-server, on any network.

Tests that the native and admin pages and their assets are served (with
revalidation), that a malformed native request is refused, that the native
heights are ordered, that esplora and bitcoind agree with native on the
height, and (with --log) that the node log shows no fault since startup.

Run with:
    pytest test_smoke.py
    pytest test_smoke.py --native-port=8181 --admin-port=8080 --log=run.stdout
"""

import re
import pytest
import requests

TIMEOUT = 10
ASSET = re.compile(r'(?:src|href)="(/[^"]+)"')
FAULT = re.compile(r"store fault|failure storing|exception", re.IGNORECASE)
ASSETS = {"/favicon.svg", "/script.js", "/style.css"}
SITES = ["native", "admin"]
MEDIA = [
    ("/favicon.svg", "image/svg+xml"),
    ("/script.js", "javascript"),
    ("/style.css", "text/css"),
]


def root(config):
    """The site root of an interface configuration."""
    return "http://{}:{}".format(config["host"], config["port"])


@pytest.fixture(params=SITES)
def site(request, native_config, admin_config):
    """The site root of each embedded page."""
    configs = {"native": native_config, "admin": admin_config}
    return root(configs[request.param])


def page(base):
    """The response for the page of a site."""
    return requests.get(base + "/", headers={"Accept": "text/html"},
        timeout=TIMEOUT)


def native_heights(native_config):
    """The native confirmed and candidate heights."""
    response = requests.get(native_config["base_url"] + "/top?format=json",
        timeout=TIMEOUT)
    response.raise_for_status()
    data = response.json()
    return data["confirmed"]["height"], data["candidate"]["height"]


def bracketed(native_config, height):
    """The native confirmed height before, another height, and after."""
    before = native_heights(native_config)[0]
    reported = height()
    after = native_heights(native_config)[0]
    return before, reported, after


def esplora_height(esplora_config):
    """The esplora tip height."""
    response = requests.get(esplora_config["base_url"] + "/blocks/tip/height",
        timeout=TIMEOUT)
    response.raise_for_status()
    return int(response.text)


def bitcoind_height(bitcoind_rpc_config):
    """The bitcoind block count."""
    payload = {"jsonrpc": "1.0", "id": 1, "method": "getblockcount",
        "params": []}
    response = requests.post(bitcoind_rpc_config["url"], json=payload,
        timeout=TIMEOUT)
    response.raise_for_status()
    return response.json()["result"]


def faults(node_log):
    """The lines of the node log that announce a fault."""
    return [line for line in node_log.splitlines() if FAULT.search(line)]


def test_page(site):
    """The page is served as html with its application root."""
    response = page(site)
    assert response.status_code == 200
    assert "text/html" in response.headers["Content-Type"]
    assert 'id="root"' in response.text


def test_page_assets(site):
    """The page references its icon, script and stylesheet."""
    assert set(ASSET.findall(page(site).text)) == ASSETS


@pytest.mark.parametrize("path,media", MEDIA)
def test_page_asset(site, path, media):
    """The asset is served with its media type and an entity tag."""
    response = requests.get(site + path, timeout=TIMEOUT)
    assert response.status_code == 200
    assert media in response.headers["Content-Type"]
    assert response.content
    assert response.headers["ETag"]


@pytest.mark.parametrize("path,media", MEDIA)
def test_page_asset_revalidated(site, path, media):
    """The asset is not resent when its entity tag matches."""
    tag = requests.get(site + path, timeout=TIMEOUT).headers["ETag"]
    response = requests.get(site + path, headers={"If-None-Match": tag}, timeout=TIMEOUT)
    assert response.status_code == 304


def test_native_malformed_refused(native_config):
    """A json request for the bare api root is refused."""
    response = requests.get(native_config["base_url"], headers={"Accept": "application/json"}, timeout=TIMEOUT)
    assert response.status_code == 400


def test_native_heights_ordered(native_config):
    """The confirmed height does not exceed the candidate height."""
    confirmed, candidate = native_heights(native_config)
    assert 0 <= confirmed <= candidate


def test_esplora_height_agrees(native_config, esplora_config):
    """The esplora tip is the native confirmed height."""
    before, reported, after = bracketed(native_config, lambda: esplora_height(esplora_config))
    assert before <= reported <= after


def test_bitcoind_height_agrees(native_config, bitcoind_rpc_config):
    """The bitcoind block count is the native confirmed height."""
    before, reported, after = bracketed(native_config, lambda: bitcoind_height(bitcoind_rpc_config))
    assert before <= reported <= after


def test_log_no_faults(node_log):
    """The node log shows no store fault or exception since startup."""
    assert faults(node_log) == []


def test_log_running(node_log):
    """The node log shows no shutdown since startup."""
    assert "== shutdown ==" not in node_log
