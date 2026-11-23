"""
Tests for libbitcoin-server Stratum v1 mining interface.

Tests the Stratum v1 JSON-RPC 1.0 interface over TCP: subscription, worker
authorization, version rolling (bip310), job notification and share
submission. Replies are JSON-RPC 1.0 (both result and error, no jsonrpc
member) and errors are positional, [code, message, data].

A job is notified only to an authorized worker. The worker is an address
paying the coinbase, unless the server configures [stratum_v1]
payout_address, in which case any worker name is authorized.

Run with:
    pytest test_stratum_v1.py
    pytest test_stratum_v1.py --stratum-v1-host=localhost --stratum-v1-port=8880
    pytest test_stratum_v1.py --stratum-v1-worker=bc1q...
"""

from __future__ import annotations

import json
import socket
import time

import pytest
import requests

_UNKNOWN = 20
_JOB_NOT_FOUND = 21
_DUPLICATE_SHARE = 22
_LOW_DIFFICULTY_SHARE = 23
_UNAUTHORIZED_WORKER = 24
_NOT_SUBSCRIBED = 25
_METHOD_NOT_FOUND = -32601
_ROLLING_MASK = "1fffe000"


class StratumClient:
    """One TCP connection, newline delimited, notifications retained."""

    def __init__(self, host: str, port: int, timeout: float):
        self.timeout = timeout
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buffer = b""
        self.notifications: list[dict] = []
        self.next_id = 1

    def close(self):
        self.sock.close()

    def _line(self, deadline: float) -> dict:
        while b"\n" not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("no message from server")
            self.sock.settimeout(remaining)
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("server closed the connection")
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        return json.loads(line)

    def call(self, method: str, params: list) -> dict:
        """Send a request and return its response, retaining notifications."""
        request_id = self.next_id
        self.next_id += 1
        request = {"id": request_id, "method": method, "params": params}
        self.sock.sendall(json.dumps(request).encode() + b"\n")
        deadline = time.monotonic() + self.timeout
        while True:
            message = self._line(deadline)
            if "method" in message:
                self.notifications.append(message)
            elif message.get("id") == request_id:
                return message

    def notification(self, method: str) -> dict:
        """Return the next retained or received notification of method."""
        deadline = time.monotonic() + self.timeout
        while True:
            for index, message in enumerate(self.notifications):
                if message["method"] == method:
                    return self.notifications.pop(index)
            self.notifications.append(self._line(deadline))


@pytest.fixture
def client(stratum_v1_config: dict):
    try:
        connection = StratumClient(stratum_v1_config["host"],
                                   stratum_v1_config["port"],
                                   stratum_v1_config["timeout"])
    except OSError as exc:
        pytest.skip(f"Cannot connect to Stratum v1 server: {exc}")
    yield connection
    connection.close()


@pytest.fixture
def job(client: StratumClient, stratum_v1_config: dict) -> list:
    """Subscribe and authorize, returning the first job notified."""
    client.call("mining.subscribe", ["libbitcoin-server-test/1.0"])
    response = client.call("mining.authorize", [stratum_v1_config["worker"], "x"])
    if response["result"] is not True:
        pytest.skip("Worker not authorized, set --stratum-v1-worker to an "
                    "address or configure [stratum_v1] payout_address")
    return client.notification("mining.notify")["params"]


def error_code(response: dict) -> int:
    error = response["error"]
    assert isinstance(error, list) and len(error) == 3
    return error[0]


def is_word(text: str) -> bool:
    return isinstance(text, str) and len(text) == 8 and \
        all(char in "0123456789abcdef" for char in text)


# ─── mining.subscribe ────────────────────────────────────────────────────────

def test_subscribe_json_rpc_1_0(client: StratumClient):
    response = client.call("mining.subscribe", [])
    assert "jsonrpc" not in response
    assert response["error"] is None
    subscriptions, extranonce1, extranonce2_size = response["result"]
    assert subscriptions[0][0] == "mining.set_difficulty"
    assert subscriptions[1][0] == "mining.notify"
    assert len(bytes.fromhex(extranonce1)) > 0
    assert extranonce2_size > 0


def test_subscribe_http_post(stratum_v1_config: dict):
    url = f"http://{stratum_v1_config['host']}:{stratum_v1_config['port']}/"
    request = {"id": 1, "method": "mining.subscribe", "params": []}
    response = requests.post(url, json=request,
                             timeout=stratum_v1_config["timeout"]).json()
    assert response["error"] is None
    assert len(response["result"]) == 3


# ─── mining.configure ────────────────────────────────────────────────────────

def test_configure_version_rolling_mask(client: StratumClient):
    response = client.call("mining.configure", [
        ["version-rolling"], {"version-rolling.mask": "ffffffff"}])
    assert response["result"]["version-rolling"] is True
    assert response["result"]["version-rolling.mask"] == _ROLLING_MASK


def test_configure_unknown_extension_declined(client: StratumClient):
    response = client.call("mining.configure", [["minimum-difficulty"], {}])
    assert response["result"] == {"minimum-difficulty": False}


# ─── errors ──────────────────────────────────────────────────────────────────

def test_unknown_method_positional_error(client: StratumClient):
    response = client.call("mining.bogus", [])
    assert response["result"] is None
    assert error_code(response) == _METHOD_NOT_FOUND


def test_submit_not_subscribed(client: StratumClient):
    response = client.call("mining.submit",
                           ["w", "00000000", "00000000", "00000000", "00000000"])
    assert error_code(response) in (_NOT_SUBSCRIBED, _UNAUTHORIZED_WORKER)


def test_authorize_invalid_address(client: StratumClient,
                                   stratum_v1_config: dict):
    response = client.call("mining.authorize", ["not an address", "x"])
    if response["result"] is True:
        pytest.skip("Server configures [stratum_v1] payout_address")
    assert error_code(response) == _UNAUTHORIZED_WORKER


# ─── mining.notify ───────────────────────────────────────────────────────────

def test_notify_job_fields(job: list):
    (job_id, prevhash, coinb1, coinb2, branch, version, nbits, ntime,
     clean) = job
    assert is_word(job_id)
    assert len(bytes.fromhex(prevhash)) == 32
    assert bytes.fromhex(coinb1)[:4] == bytes.fromhex("02000000")
    assert len(bytes.fromhex(coinb2)) > 0
    assert all(len(bytes.fromhex(item)) == 32 for item in branch)
    assert is_word(version) and is_word(nbits) and is_word(ntime)
    assert clean is True


def test_set_difficulty_precedes_job(client: StratumClient,
                                     stratum_v1_config: dict):
    client.call("mining.subscribe", [])
    response = client.call("mining.authorize", [stratum_v1_config["worker"], "x"])
    if response["result"] is not True:
        pytest.skip("Worker not authorized")
    difficulty = client.notification("mining.set_difficulty")
    assert difficulty["id"] is None
    assert difficulty["params"][0] > 0


# ─── mining.submit ───────────────────────────────────────────────────────────

def test_submit_unknown_job(client: StratumClient, job: list,
                            stratum_v1_config: dict):
    response = client.call("mining.submit", [stratum_v1_config["worker"],
                           "ffffffff", "00000000", job[7], "00000000"])
    assert error_code(response) == _JOB_NOT_FOUND


def test_submit_malformed_extranonce2(client: StratumClient, job: list,
                                      stratum_v1_config: dict):
    response = client.call("mining.submit", [stratum_v1_config["worker"],
                           job[0], "00", job[7], "00000000"])
    assert error_code(response) == _UNKNOWN


def test_submit_share_answered_once(client: StratumClient, job: list,
                                    stratum_v1_config: dict):
    params = [stratum_v1_config["worker"], job[0], "00000000", job[7],
              "00000000"]
    first = client.call("mining.submit", params)
    second = client.call("mining.submit", params)
    if first["result"] is True:
        assert error_code(second) == _DUPLICATE_SHARE
    else:
        assert error_code(first) == _LOW_DIFFICULTY_SHARE
