"""
Transport coverage for libbitcoin-server json-rpc services.

Each json-rpc service is served over one channel on three transports:

    tcp    raw json on the connection, which the server detects and
           downgrades from http to a newline-delimited stream
    http   json-rpc in an http POST body (cannot push notifications)
    ws     an http upgrade, framed and full duplex (pushes notifications)

The per-service suites cover the transport each service is usually reached
on: bitcoind and btcd over http/ws, electrum over tcp. These verify that the
other transports reach the same protocol.

Run with:
    pytest test_transports.py
    pytest test_transports.py --electrum-port=50001 --bitcoind-rpc-port=8332
"""

from __future__ import annotations

import json
import socket

import pytest
import requests
import websocket

_TIMEOUT = 15.0


# ─── Helpers ─────────────────────────────────────────────────────────────────

def _tcp_rpc(host: str, port: int, method: str, params=None, id_=1):
    """
    Send a raw json-rpc request over tcp, which downgrades the connection.

    Returns the parsed response, or None if the server closed without one.
    """
    request = json.dumps({
        "jsonrpc": "2.0",
        "id": id_,
        "method": method,
        "params": params or []
    }) + "\n"

    with socket.create_connection((host, port), timeout=_TIMEOUT) as sock:
        sock.settimeout(_TIMEOUT)
        sock.sendall(request.encode())

        buffer = b""
        while b"\n" not in buffer:
            data = sock.recv(65536)
            if not data:
                return None
            buffer += data

    return json.loads(buffer.split(b"\n", 1)[0])


def _requires_downgrade(response, service: str):
    """Skip when the endpoint refuses an unauthenticated downgrade."""
    if response is None:
        pytest.skip(
            f"{service} closed the downgraded connection: a credential is "
            "configured and http basic authorization cannot be carried on a "
            "raw tcp connection"
        )


def _is_unauthorized(response) -> bool:
    error = response.get("error")
    return isinstance(error, dict) and "unauthorized" in str(
        error.get("message", "")).lower()


# ─── bitcoind over tcp (downgrade) ───────────────────────────────────────────

def test_bitcoind_tcp_downgrade(bitcoind_rpc_config):
    """A raw json request downgrades the connection and is answered."""
    response = _tcp_rpc(
        bitcoind_rpc_config["host"],
        bitcoind_rpc_config["port"],
        "getblockcount"
    )

    _requires_downgrade(response, "bitcoind")
    assert response["id"] == 1
    if _is_unauthorized(response):
        pytest.skip("bitcoind requires authorization on this endpoint")

    assert isinstance(response["result"], int)


def test_bitcoind_tcp_downgrade_is_latched(bitcoind_rpc_config):
    """The downgrade is latched, so a second request on it stays tcp."""
    host = bitcoind_rpc_config["host"]
    port = bitcoind_rpc_config["port"]

    request = json.dumps({
        "jsonrpc": "2.0", "id": 1, "method": "getblockcount", "params": []
    }) + "\n"

    with socket.create_connection((host, port), timeout=_TIMEOUT) as sock:
        sock.settimeout(_TIMEOUT)
        responses = []
        buffer = b""

        for identifier in (1, 2):
            sock.sendall(request.replace('"id": 1', f'"id": {identifier}').encode())
            while b"\n" not in buffer:
                data = sock.recv(65536)
                if not data:
                    pytest.skip(
                        "bitcoind closed the downgraded connection "
                        "(authorization required)"
                    )
                buffer += data

            line, buffer = buffer.split(b"\n", 1)
            responses.append(json.loads(line))

    if any(_is_unauthorized(r) for r in responses):
        pytest.skip("bitcoind requires authorization on this endpoint")

    assert [r["id"] for r in responses] == [1, 2]
    assert responses[0]["result"] == responses[1]["result"]


# ─── btcd over tcp (downgrade) ───────────────────────────────────────────────

def test_btcd_tcp_downgrade(btcd_config):
    """btcd shares the universal channel, so it downgrades identically."""
    response = _tcp_rpc(
        btcd_config["host"],
        btcd_config["port"],
        "getblockcount"
    )

    _requires_downgrade(response, "btcd")
    assert response["id"] == 1

    # btcd carries its own authenticate method, so an unauthorized reply is
    # itself proof that the downgraded connection reached the protocol.
    assert ("result" in response) or _is_unauthorized(response)


# ─── electrum over http POST ─────────────────────────────────────────────────

def _electrum_post(config: dict, method: str, params=None, id_=1):
    """Send one electrum request as an http POST (no persistent state)."""
    url = f"http://{config['host']}:{config['port']}/"
    body = {"id": id_, "method": method, "params": params or []}
    response = requests.post(url, json=body, timeout=_TIMEOUT)
    response.raise_for_status()
    return response.json()


def test_electrum_http_post_handshake(electrum_config):
    """server.version negotiates over http POST."""
    response = _electrum_post(
        electrum_config,
        "server.version",
        ["test", electrum_config["protocol"]]
    )

    assert response["id"] == 1
    result = response["result"]
    assert isinstance(result, list) and len(result) == 2


def test_electrum_http_post_request(electrum_config):
    """A method is served over http POST on the same connection semantics."""
    session = requests.Session()
    url = f"http://{electrum_config['host']}:{electrum_config['port']}/"

    handshake = session.post(url, timeout=_TIMEOUT, json={
        "id": 1,
        "method": "server.version",
        "params": ["test", electrum_config["protocol"]]
    })
    handshake.raise_for_status()

    response = session.post(url, timeout=_TIMEOUT, json={
        "id": 2, "method": "server.banner", "params": []
    })
    response.raise_for_status()

    body = response.json()
    assert body["id"] == 2
    assert isinstance(body["result"], str)


# ─── electrum over websocket ─────────────────────────────────────────────────

def test_electrum_websocket_request(electrum_config):
    """server.version and a method are served over a websocket upgrade."""
    url = f"ws://{electrum_config['host']}:{electrum_config['port']}/"
    connection = websocket.create_connection(url, timeout=_TIMEOUT)
    try:
        connection.send(json.dumps({
            "id": 1,
            "method": "server.version",
            "params": ["test", electrum_config["protocol"]]
        }))
        handshake = json.loads(connection.recv())
        assert handshake["id"] == 1
        assert isinstance(handshake["result"], list)

        connection.send(json.dumps({
            "id": 2, "method": "server.banner", "params": []
        }))
        response = json.loads(connection.recv())
        assert response["id"] == 2
        assert isinstance(response["result"], str)
    finally:
        connection.close()
