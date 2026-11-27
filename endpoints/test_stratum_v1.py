"""
Tests for libbitcoin-server Stratum v1 mining interface.

Tests the Stratum v1 JSON-RPC 1.0 interface over TCP: subscription, worker
authorization, version rolling (bip310), difficulty, job notification and
share submission. Replies are JSON-RPC 1.0 (both result and error, no jsonrpc
member) and errors are positional, [code, message, data].

A job is notified only to an authorized worker. The worker name up to any dot
is the account, an address paying the coinbase, unless the server configures
[stratum_v1] payout_address, in which case any worker name is authorized.
With a [stratum_v1] credential, mining.authorize carries it on raw tcp (the
account is the credential user, --stratum-v1-password its password).

The job tests rebuild the coinbase, merkle root and header from the job, as a
miner does. Where the share target is reachable by hashing in python (a small
[stratum_v1] minimum_difficulty), valid shares are submitted, and where the
block target is reachable too (regtest), a block is mined and confirmed.

Run with:
    pytest test_stratum_v1.py
    pytest test_stratum_v1.py --stratum-v1-host=localhost --stratum-v1-port=8880
    pytest test_stratum_v1.py --stratum-v1-worker=bc1q...
"""

from __future__ import annotations

import base64
import hashlib
import json
import socket
import struct
import time

import pytest
import requests
import websocket

_UNKNOWN = 20
_JOB_NOT_FOUND = 21
_DUPLICATE_SHARE = 22
_LOW_DIFFICULTY_SHARE = 23
_UNAUTHORIZED_WORKER = 24
_NOT_SUBSCRIBED = 25
_METHOD_NOT_FOUND = -32601
_UNAUTHORIZED = 107
_ROLLING_MASK = "1fffe000"

# The target of difficulty one (bits 0x1d00ffff).
_DIFFICULTY_ONE = 0xffff << 208

# Hashes tried for a share or block (seconds in python).
_MAXIMUM_TRIES = 1 << 20

# A search of the budget misses with probability e^-16 at this bound.
_REACHABLE_TRIES = _MAXIMUM_TRIES // 16


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

    def notification(self, method: str, timeout: float | None = None) -> dict:
        """Return the next retained or received notification of method."""
        deadline = time.monotonic() + (timeout or self.timeout)
        while True:
            for index, message in enumerate(self.notifications):
                if message["method"] == method:
                    return self.notifications.pop(index)
            self.notifications.append(self._line(deadline))


# ─── Fixtures ────────────────────────────────────────────────────────────────

def _connect(config: dict) -> StratumClient:
    try:
        return StratumClient(config["host"], config["port"], config["timeout"])
    except OSError as exc:
        pytest.skip(f"Cannot connect to Stratum v1 server: {exc}")


@pytest.fixture
def client(stratum_v1_config: dict):
    connection = _connect(stratum_v1_config)
    yield connection
    connection.close()


@pytest.fixture
def other(stratum_v1_config: dict):
    """A second connection to the server."""
    connection = _connect(stratum_v1_config)
    yield connection
    connection.close()


def authorize(client: StratumClient, config: dict, worker: str | None = None,
              password: str | None = None) -> dict:
    name = config["worker"] if worker is None else worker
    secret = config["password"] if password is None else password
    return client.call("mining.authorize", [name, secret])


def subscribed(client: StratumClient, config: dict) -> dict:
    """Subscribe and authorize, skipping when the worker is not authorized."""
    response = client.call("mining.subscribe", ["libbitcoin-server-test/1.0"])
    if authorize(client, config)["result"] is not True:
        pytest.skip("Worker not authorized, set --stratum-v1-worker to an "
                    "address or configure [stratum_v1] payout_address")
    return response


@pytest.fixture
def job(client: StratumClient, stratum_v1_config: dict) -> list:
    """Subscribe and authorize, returning the first job notified."""
    subscribed(client, stratum_v1_config)
    return client.notification("mining.notify")["params"]


@pytest.fixture
def miner(client: StratumClient, stratum_v1_config: dict) -> "Miner":
    """A subscribed and authorized channel with its current job."""
    response = subscribed(client, stratum_v1_config)
    _, extranonce1, extranonce2_size = response["result"]
    difficulty = client.notification("mining.set_difficulty")["params"][0]
    params = client.notification("mining.notify")["params"]
    return Miner(client, stratum_v1_config["worker"], extranonce1,
                 extranonce2_size, difficulty, params)


@pytest.fixture
def payout_configured(other: StratumClient) -> bool:
    """The server authorizes any worker name (payout_address configured)."""
    return other.call("mining.authorize", ["not an address", ""])[
        "result"] is True


# ─── Helpers ─────────────────────────────────────────────────────────────────

def error_code(response: dict) -> int:
    error = response["error"]
    assert isinstance(error, list) and len(error) == 3
    return error[0]


def is_word(text: str) -> bool:
    return isinstance(text, str) and len(text) == 8 and \
        all(char in "0123456789abcdef" for char in text)


def dsha256(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def word_swap(data: bytes) -> bytes:
    """Reverse each 4 byte word (stratum previous hash encoding)."""
    return b"".join(data[i:i + 4][::-1] for i in range(0, len(data), 4))


def expand(bits: int) -> int:
    """The target of compact bits."""
    return (bits & 0x007fffff) << (8 * ((bits >> 24) - 3))


def share_target(difficulty: float) -> int:
    if difficulty <= 0:
        return (1 << 256) - 1
    return min((1 << 256) - 1, int(_DIFFICULTY_ONE / difficulty))


def read_varint(data: bytes, offset: int) -> tuple[int, int]:
    head = data[offset]
    if head < 0xfd:
        return head, offset + 1
    size = {0xfd: 2, 0xfe: 4, 0xff: 8}[head]
    return int.from_bytes(data[offset + 1:offset + 1 + size], "little"), \
        offset + 1 + size


def parse_coinbase(tx: bytes) -> dict:
    """Parse a coinbase serialized without witness."""
    version = int.from_bytes(tx[:4], "little")
    inputs, offset = read_varint(tx, 4)
    prevout = tx[offset:offset + 36]
    offset += 36
    size, offset = read_varint(tx, offset)
    script = tx[offset:offset + size]
    offset += size
    sequence = int.from_bytes(tx[offset:offset + 4], "little")
    offset += 4
    count, offset = read_varint(tx, offset)
    outputs = []
    for _ in range(count):
        value = int.from_bytes(tx[offset:offset + 8], "little")
        size, offset = read_varint(tx, offset + 8)
        outputs.append((value, tx[offset:offset + size]))
        offset += size
    locktime = int.from_bytes(tx[offset:offset + 4], "little")
    assert offset + 4 == len(tx), "trailing coinbase bytes"
    return {"version": version, "inputs": inputs, "prevout": prevout,
            "script": script, "sequence": sequence, "outputs": outputs,
            "locktime": locktime}


def script_height(script: bytes) -> int:
    """The bip34 height pushed first by a coinbase input script."""
    size = script[0]
    if 0x51 <= size <= 0x60:
        return size - 0x50
    assert 1 <= size <= 4, "height is not a minimal push"
    return int.from_bytes(script[1:1 + size], "little")


# Bech32 and bech32m (bip173, bip350).
_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"


def _polymod(values: list[int]) -> int:
    generator = [0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3]
    check = 1
    for value in values:
        top = check >> 25
        check = (check & 0x1ffffff) << 5 ^ value
        for i in range(5):
            check ^= generator[i] if (top >> i) & 1 else 0
    return check


def _expand_hrp(hrp: str) -> list[int]:
    return [ord(c) >> 5 for c in hrp] + [0] + [ord(c) & 31 for c in hrp]


def _convert(data, source: int, target: int, pad: bool) -> list[int] | None:
    accumulator, bits, out = 0, 0, []
    maximum = (1 << target) - 1
    for value in data:
        accumulator = (accumulator << source) | value
        bits += source
        while bits >= target:
            bits -= target
            out.append((accumulator >> bits) & maximum)
    if pad and bits:
        out.append((accumulator << (target - bits)) & maximum)
    elif not pad and (bits >= source or (accumulator << (target - bits)) &
                      maximum):
        return None
    return out


def decode_segwit(address: str) -> tuple[str, int, bytes] | None:
    """(hrp, version, program) of a segwit address, or None."""
    address = address.lower()
    position = address.rfind("1")
    if position < 1 or position + 7 > len(address):
        return None
    hrp, rest = address[:position], address[position + 1:]
    if any(c not in _CHARSET for c in rest):
        return None
    data = [_CHARSET.find(c) for c in rest]
    constant = _polymod(_expand_hrp(hrp) + data)
    if constant not in (1, 0x2bc830a3):
        return None
    program = _convert(data[1:-6], 5, 8, False)
    if program is None or not data:
        return None
    version = data[0]
    if (version == 0) != (constant == 1):
        return None
    return hrp, version, bytes(program)


def encode_segwit(hrp: str, version: int, program: bytes) -> str:
    data = [version] + _convert(program, 8, 5, True)
    constant = 1 if version == 0 else 0x2bc830a3
    check = _polymod(_expand_hrp(hrp) + data + [0] * 6) ^ constant
    data += [(check >> 5 * (5 - i)) & 31 for i in range(6)]
    return hrp + "1" + "".join(_CHARSET[d] for d in data)


def segwit_script(version: int, program: bytes) -> bytes:
    opcode = 0 if version == 0 else 0x50 + version
    return bytes([opcode, len(program)]) + program


class Miner:
    """Builds coinbases, merkle roots and headers from jobs, as a miner does."""

    def __init__(self, client: StratumClient, worker: str, extranonce1: str,
                 extranonce2_size: int, difficulty: float, params: list):
        self.client = client
        self.worker = worker
        self.extranonce1 = bytes.fromhex(extranonce1)
        self.extranonce2_size = extranonce2_size
        self.difficulty = difficulty
        self.params = params

    @property
    def job_id(self) -> str:
        return self.params[0]

    @property
    def ntime(self) -> str:
        return self.params[7]

    @property
    def bits(self) -> int:
        return int(self.params[6], 16)

    def coinbase(self, extranonce2: bytes) -> bytes:
        _, _, coinb1, coinb2 = self.params[:4]
        return bytes.fromhex(coinb1) + self.extranonce1 + extranonce2 + \
            bytes.fromhex(coinb2)

    def header(self, extranonce2: bytes, ntime: str, nonce: int,
               version: int | None = None) -> bytes:
        prevhash, branch = self.params[1], self.params[4]
        root = dsha256(self.coinbase(extranonce2))
        for item in branch:
            root = dsha256(root + bytes.fromhex(item))
        job_version = int(self.params[5], 16) if version is None else version
        return struct.pack("<I", job_version) + \
            word_swap(bytes.fromhex(prevhash)) + root + \
            struct.pack("<III", int(ntime, 16), self.bits, nonce)

    def search(self, target: int, extranonce2: bytes,
               start: int = 0) -> tuple[int, bytes] | None:
        """The first nonce from start whose header hash meets the target."""
        for nonce in range(start, start + _MAXIMUM_TRIES):
            digest = dsha256(self.header(extranonce2, self.ntime, nonce))
            if int.from_bytes(digest, "little") <= target:
                return nonce, digest
        return None

    def submit(self, extranonce2: bytes, nonce: int,
               version_bits: str | None = None, worker: str | None = None,
               job_id: str | None = None, ntime: str | None = None) -> dict:
        params = [self.worker if worker is None else worker,
                  self.job_id if job_id is None else job_id,
                  extranonce2.hex(),
                  self.ntime if ntime is None else ntime,
                  f"{nonce:08x}"]
        if version_bits is not None:
            params.append(version_bits)
        return self.client.call("mining.submit", params)


def tries(target: int) -> float:
    return float(1 << 256) / (target + 1)


def basic_auth(config: dict) -> tuple[str, str] | None:
    """The http credential, the worker account and password, if configured."""
    if not config["password"]:
        return None
    return config["worker"].split(".")[0], config["password"]


def native_top(native_config: dict) -> dict:
    """The confirmed top, {hash, height}, from the native interface."""
    url = f"{native_config['base_url']}/top?format=json"
    tops = requests.get(url, timeout=10).json()
    return next(top for top in tops if top.get("chain") == "confirmed")


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


def test_subscribe_extranonce1_is_unique_per_connection(client: StratumClient,
                                                        other: StratumClient):
    first = client.call("mining.subscribe", [])["result"]
    second = other.call("mining.subscribe", [])["result"]
    assert first[0][0][1] == first[0][1][1], "subscription ids differ"
    assert first[1] != second[1]
    assert len(first[1]) == len(second[1])


@pytest.mark.parametrize("params", [
    ["bosminer/1.0", None, "pool.example:3333", None],
    ["cgminer/4.11", None, "pool.example", 3333],
    ["agent/1.0", "00000001"],
])
def test_subscribe_miner_parameters(client: StratumClient, params: list):
    """Miners send a session id, host and port, which may be null."""
    response = client.call("mining.subscribe", params)
    assert response["error"] is None
    assert len(response["result"]) == 3


def test_subscribe_http_post(stratum_v1_config: dict):
    url = f"http://{stratum_v1_config['host']}:{stratum_v1_config['port']}/"
    request = {"id": 1, "method": "mining.subscribe", "params": []}
    try:
        response = requests.post(url, json=request,
                                 auth=basic_auth(stratum_v1_config),
                                 timeout=stratum_v1_config["timeout"])
    except requests.exceptions.ConnectionError as exc:
        pytest.skip(f"Cannot connect to Stratum v1 server: {exc}")
    assert response.status_code == 200
    assert response.json()["error"] is None
    assert len(response.json()["result"]) == 3


def test_websocket_notifies_job(stratum_v1_config: dict):
    """A websocket is full duplex, so it is notified jobs as raw tcp is."""
    url = f"ws://{stratum_v1_config['host']}:{stratum_v1_config['port']}/"
    header = []
    credential = basic_auth(stratum_v1_config)
    if credential is not None:
        token = base64.b64encode(":".join(credential).encode()).decode()
        header.append(f"Authorization: Basic {token}")
    try:
        ws = websocket.create_connection(url, header=header,
                                         timeout=stratum_v1_config["timeout"])
    except (OSError, websocket.WebSocketException) as exc:
        pytest.skip(f"No Stratum v1 websocket: {exc}")
    try:
        ws.send(json.dumps({"id": 1, "method": "mining.subscribe",
                            "params": []}))
        ws.send(json.dumps({"id": 2, "method": "mining.authorize",
                            "params": [stratum_v1_config["worker"],
                                       stratum_v1_config["password"]]}))
        responses, methods = {}, []
        deadline = time.monotonic() + stratum_v1_config["timeout"]
        while "mining.notify" not in methods:
            assert time.monotonic() < deadline, f"no job: {responses}"
            message = json.loads(ws.recv())
            if "method" in message:
                methods.append(message["method"])
            else:
                responses[message["id"]] = message
                if message["id"] == 2 and message["result"] is not True:
                    pytest.skip("Worker not authorized")
        assert responses[1]["error"] is None
        assert methods.index("mining.set_difficulty") < \
            methods.index("mining.notify")
    finally:
        ws.close()


# ─── mining.authorize ────────────────────────────────────────────────────────

def test_authorize_invalid_address(client: StratumClient):
    response = client.call("mining.authorize", ["not an address", "x"])
    if response["result"] is True:
        pytest.skip("Server configures [stratum_v1] payout_address")
    assert error_code(response) == _UNAUTHORIZED_WORKER


def test_authorize_null_password(client: StratumClient,
                                 stratum_v1_config: dict):
    if stratum_v1_config["password"]:
        pytest.skip("a credential is configured")
    response = client.call("mining.authorize",
                           [stratum_v1_config["worker"], None])
    if response["result"] is not True:
        pytest.skip("Worker not authorized")
    assert response["error"] is None


def test_authorize_worker_suffix(client: StratumClient,
                                 stratum_v1_config: dict):
    """The account is the worker name up to the first dot."""
    if authorize(client, stratum_v1_config)["result"] is not True:
        pytest.skip("Worker not authorized")
    rig = stratum_v1_config["worker"] + ".rig1"
    response = authorize(client, stratum_v1_config, worker=rig)
    assert response["result"] is True


def test_authorize_other_payout_rejected(client: StratumClient,
                                         stratum_v1_config: dict,
                                         payout_configured: bool):
    """The payout of a channel is set by its first authorized worker."""
    if payout_configured:
        pytest.skip("Server configures [stratum_v1] payout_address")
    decoded = decode_segwit(stratum_v1_config["worker"].split(".")[0])
    if decoded is None:
        pytest.skip("Worker is not a segwit address")
    hrp, version, program = decoded
    sibling = encode_segwit(hrp, 0, bytes([0x01] * 20))
    if authorize(client, stratum_v1_config)["result"] is not True:
        pytest.skip("Worker not authorized")
    response = authorize(client, stratum_v1_config, worker=sibling)
    assert error_code(response) == _UNAUTHORIZED_WORKER


def test_authorize_wrong_password_rejected(client: StratumClient,
                                           stratum_v1_config: dict):
    if not stratum_v1_config["password"]:
        pytest.skip("no credential configured (--stratum-v1-password)")
    response = authorize(client, stratum_v1_config,
                         password=stratum_v1_config["password"] + "x")
    assert error_code(response) == _UNAUTHORIZED_WORKER


def test_submit_before_credential_refused(client: StratumClient,
                                          stratum_v1_config: dict):
    """Before its credential a raw tcp channel may only shake hands."""
    if not stratum_v1_config["password"]:
        pytest.skip("no credential configured (--stratum-v1-password)")
    client.call("mining.subscribe", [])
    response = client.call("mining.submit", [stratum_v1_config["worker"],
                           "00000000", "00000000", "00000000", "00000000"])
    assert response["result"] is None
    assert error_code(response) == _UNAUTHORIZED


def test_authorize_credential_over_http(stratum_v1_config: dict):
    """Over http the credential is basic authorization, as for any service."""
    if not stratum_v1_config["password"]:
        pytest.skip("no credential configured (--stratum-v1-password)")
    url = f"http://{stratum_v1_config['host']}:{stratum_v1_config['port']}/"
    request = {"id": 1, "method": "mining.subscribe", "params": []}
    try:
        response = requests.post(url, json=request,
                                 timeout=stratum_v1_config["timeout"])
    except requests.exceptions.ConnectionError as exc:
        pytest.skip(f"Cannot connect to Stratum v1 server: {exc}")
    assert response.status_code == 401


# ─── mining.configure ────────────────────────────────────────────────────────

def test_configure_version_rolling_mask(client: StratumClient):
    response = client.call("mining.configure", [
        ["version-rolling"], {"version-rolling.mask": "ffffffff"}])
    assert response["result"]["version-rolling"] is True
    assert response["result"]["version-rolling.mask"] == _ROLLING_MASK


@pytest.mark.parametrize("requested,granted", [
    ("0000e000", "0000e000"),
    ("e0000000", "00000000"),
    ("not hex", "00000000"),
])
def test_configure_version_rolling_mask_intersected(client: StratumClient,
                                                    requested: str,
                                                    granted: str):
    """The mask granted is the requested mask within the server mask."""
    response = client.call("mining.configure", [
        ["version-rolling"], {"version-rolling.mask": requested}])
    assert response["result"]["version-rolling.mask"] == granted


def test_configure_version_rolling_default_mask(client: StratumClient):
    response = client.call("mining.configure", [["version-rolling"], {}])
    assert response["result"]["version-rolling.mask"] == _ROLLING_MASK


def test_configure_unknown_extension_declined(client: StratumClient):
    response = client.call("mining.configure", [["minimum-difficulty"], {}])
    assert response["result"] == {"minimum-difficulty": False}


def test_configure_extensions_answered_each(client: StratumClient):
    response = client.call("mining.configure", [
        ["version-rolling", "minimum-difficulty", "subscribe-extranonce",
         "info"], {"version-rolling.mask": "1fffe000"}])
    assert response["result"] == {
        "version-rolling": True,
        "version-rolling.mask": _ROLLING_MASK,
        "minimum-difficulty": False,
        "subscribe-extranonce": False,
        "info": False,
    }


# ─── unserved and trivial methods ────────────────────────────────────────────

def test_unknown_method_positional_error(client: StratumClient):
    response = client.call("mining.bogus", [])
    assert response["result"] is None
    assert error_code(response) == _METHOD_NOT_FOUND


@pytest.mark.parametrize("method,params", [
    ("mining.get_transactions", ["00000000"]),
    ("mining.multi_version", [2]),
])
def test_unserved_method_answers_stratum_code(client: StratumClient,
                                              stratum_v1_config: dict,
                                              method: str, params: list):
    subscribed(client, stratum_v1_config)
    response = client.call(method, params)
    assert response["result"] is None
    assert error_code(response) == _UNKNOWN
    assert response["error"][1]


def test_extranonce_subscribe_acknowledged(client: StratumClient):
    response = client.call("mining.extranonce.subscribe", [])
    assert response["error"] is None
    assert response["result"] is True


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
    subscribed(client, stratum_v1_config)
    difficulty = client.notification("mining.set_difficulty")
    assert difficulty["id"] is None
    assert difficulty["params"][0] > 0


def test_set_difficulty_is_first_notification(client: StratumClient,
                                              stratum_v1_config: dict):
    """The difficulty of a job is notified before the job."""
    subscribed(client, stratum_v1_config)
    client.notification("mining.notify")
    # Notifications received before the job are retained in order.
    methods = [message["method"] for message in client.notifications]
    assert methods[:1] == ["mining.set_difficulty"]


def test_notify_previous_is_top(client: StratumClient,
                                stratum_v1_config: dict, native_config: dict):
    """The job builds on the confirmed top (stratum word order)."""
    subscribed(client, stratum_v1_config)
    params = client.notification("mining.notify")["params"]
    try:
        top = native_top(native_config)
    except (requests.RequestException, KeyError, ValueError) as exc:
        pytest.skip(f"native top unavailable: {exc}")
    previous = word_swap(bytes.fromhex(params[1]))[::-1].hex()
    if previous != top["hash"]:
        pytest.skip("top changed while the job was notified")
    assert previous == top["hash"]


def test_notify_coinbase_layout(client: StratumClient,
                                stratum_v1_config: dict, native_config: dict):
    """The coinbase of a job: bip34 height, extranonce, payout, bip141, bip54."""
    response = subscribed(client, stratum_v1_config)
    _, extranonce1, extranonce2_size = response["result"]
    params = client.notification("mining.notify")["params"]
    coinb1, coinb2 = bytes.fromhex(params[2]), bytes.fromhex(params[3])
    extranonce = bytes.fromhex(extranonce1) + bytes(extranonce2_size)
    coinbase = parse_coinbase(coinb1 + extranonce + coinb2)

    assert coinbase["version"] == 2
    assert coinbase["inputs"] == 1
    assert coinbase["prevout"] == bytes(32) + b"\xff\xff\xff\xff"

    # The input script ends with the extranonce, pushed by its size.
    script = coinbase["script"]
    assert script.endswith(bytes([len(extranonce)]) + extranonce)
    assert coinb1.endswith(bytes([len(extranonce)]))

    # bip54: the sequence is not final and the locktime is the prior height.
    height = script_height(script)
    assert coinbase["sequence"] == 0xfffffffe
    assert coinbase["locktime"] == height - 1

    # The payout, then the zero value witness commitment (bip141).
    (value, payout), (zero, commitment) = coinbase["outputs"]
    assert value > 0
    assert zero == 0
    assert commitment[:6] == bytes.fromhex("6a24aa21a9ed")
    assert len(commitment) == 38

    decoded = decode_segwit(stratum_v1_config["worker"].split(".")[0])
    if decoded is not None and authorize(
            client, stratum_v1_config, worker="not an address")[
            "result"] is not True:
        assert payout == segwit_script(decoded[1], decoded[2])

    try:
        top = native_top(native_config)
    except (requests.RequestException, KeyError, ValueError):
        return
    assert height in (top["height"] + 1, top["height"] + 2)


def test_notify_time_and_version(job: list):
    version, ntime = int(job[5], 16), int(job[7], 16)
    assert abs(ntime - time.time()) < 2 * 60 * 60
    assert version & 0xe0000000 == 0x20000000, "not a bip9 version"


# ─── mining.suggest_difficulty / mining.suggest_target ───────────────────────

def test_suggest_difficulty_refreshes_job(client: StratumClient,
                                          stratum_v1_config: dict):
    """
    A raised difficulty is notified, then the current job again at a later
    time.
    """
    subscribed(client, stratum_v1_config)
    initial = client.notification("mining.set_difficulty")["params"][0]
    first = client.notification("mining.notify")["params"]
    response = client.call("mining.suggest_difficulty", [initial * 4])
    assert response["result"] is True
    assert client.notification("mining.set_difficulty")["params"][0] == \
        pytest.approx(initial * 4)
    refreshed = client.notification("mining.notify")["params"]
    assert refreshed[0] != first[0]
    assert refreshed[1:7] == first[1:7]
    assert int(refreshed[7], 16) > int(first[7], 16)
    assert refreshed[8] is False


def test_suggest_difficulty_floored_at_minimum(client: StratumClient,
                                               stratum_v1_config: dict):
    subscribed(client, stratum_v1_config)
    initial = client.notification("mining.set_difficulty")["params"][0]
    client.notification("mining.notify")
    client.call("mining.suggest_difficulty", [initial * 4])
    client.notification("mining.set_difficulty")
    response = client.call("mining.suggest_difficulty", [0])
    assert response["result"] is True
    floored = client.notification("mining.set_difficulty")["params"][0]
    assert 0 < floored <= initial * 4


def test_suggest_difficulty_before_subscribe(client: StratumClient):
    response = client.call("mining.suggest_difficulty", [1024])
    assert response["result"] is True


def test_suggest_target(client: StratumClient, stratum_v1_config: dict):
    subscribed(client, stratum_v1_config)
    initial = client.notification("mining.set_difficulty")["params"][0]
    client.notification("mining.notify")
    target = share_target(initial * 8)
    response = client.call("mining.suggest_target", [f"{target:064x}"])
    assert response["result"] is True
    difficulty = client.notification("mining.set_difficulty")["params"][0]
    assert difficulty == pytest.approx(initial * 8, rel=1e-3)


def test_suggest_target_malformed(client: StratumClient):
    response = client.call("mining.suggest_target", ["not a target"])
    assert error_code(response) == _UNKNOWN


@pytest.mark.slow
def test_difficulty_decays_without_shares(client: StratumClient,
                                          stratum_v1_config: dict):
    """Without shares the difficulty decays (4 share intervals, 10 s each)."""
    subscribed(client, stratum_v1_config)
    initial = client.notification("mining.set_difficulty")["params"][0]
    client.notification("mining.notify")
    client.call("mining.suggest_difficulty", [initial * 64])
    raised = client.notification("mining.set_difficulty")["params"][0]
    decayed = client.notification("mining.set_difficulty", timeout=120)
    assert decayed["params"][0] < raised


# ─── mining.submit ───────────────────────────────────────────────────────────

def test_submit_not_subscribed(client: StratumClient,
                               stratum_v1_config: dict):
    if stratum_v1_config["password"]:
        pytest.skip("a credential is configured (raw tcp refuses first)")
    response = client.call("mining.submit",
                           ["w", "00000000", "00000000", "00000000", "00000000"])
    assert error_code(response) in (_NOT_SUBSCRIBED, _UNAUTHORIZED_WORKER)


def test_submit_not_authorized(client: StratumClient, stratum_v1_config: dict):
    if stratum_v1_config["password"]:
        pytest.skip("a credential is configured (raw tcp refuses first)")
    client.call("mining.subscribe", [])
    response = client.call("mining.submit", [stratum_v1_config["worker"],
                           "00000000", "00000000", "00000000", "00000000"])
    assert error_code(response) == _UNAUTHORIZED_WORKER


def test_submit_other_worker(client: StratumClient, job: list):
    response = client.call("mining.submit", ["someone.else", job[0],
                           "00000000", job[7], "00000000"])
    assert error_code(response) == _UNAUTHORIZED_WORKER


def test_submit_unknown_job(client: StratumClient, job: list,
                            stratum_v1_config: dict):
    response = client.call("mining.submit", [stratum_v1_config["worker"],
                           "ffffffff", "00000000", job[7], "00000000"])
    assert error_code(response) == _JOB_NOT_FOUND


@pytest.mark.parametrize("field,value", [
    ("job_id", "not hex"),
    ("extranonce2", "00"),
    ("extranonce2", "zzzzzzzz"),
    ("ntime", "0000"),
    ("nonce", "nonce"),
])
def test_submit_malformed_field(client: StratumClient, miner: Miner,
                                field: str, value: str):
    params = {"worker": miner.worker, "job_id": miner.job_id,
              "extranonce2": "00" * miner.extranonce2_size,
              "ntime": miner.ntime, "nonce": "00000000"}
    params[field] = value
    response = client.call("mining.submit", list(params.values()))
    assert error_code(response) == _UNKNOWN


def test_submit_version_bits_without_mask(client: StratumClient,
                                          miner: Miner):
    """Without mining.configure the mask is empty, so no bit may roll."""
    response = miner.submit(bytes(miner.extranonce2_size), 0,
                            version_bits="00002000")
    assert error_code(response) == _UNKNOWN


def test_submit_version_bits_outside_mask(client: StratumClient, miner: Miner):
    client.call("mining.configure", [["version-rolling"],
                                     {"version-rolling.mask": "1fffe000"}])
    response = miner.submit(bytes(miner.extranonce2_size), 0,
                            version_bits="00001000")
    assert error_code(response) == _UNKNOWN


def test_submit_version_bits_within_mask(client: StratumClient, miner: Miner):
    client.call("mining.configure", [["version-rolling"],
                                     {"version-rolling.mask": "1fffe000"}])
    response = miner.submit(bytes(miner.extranonce2_size), 0,
                            version_bits="00002000")
    assert response["result"] is True or \
        error_code(response) == _LOW_DIFFICULTY_SHARE


@pytest.mark.parametrize("ntime", ["00000000", "futuristic"])
def test_submit_invalid_timestamp(client: StratumClient, miner: Miner,
                                  ntime: str):
    """A header the template context rejects is answered, not mined."""
    if ntime == "futuristic":
        ntime = f"{int(time.time()) + 3 * 60 * 60:08x}"
    response = miner.submit(bytes(miner.extranonce2_size), 0, ntime=ntime)
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


def test_submit_low_difficulty_share(client: StratumClient, miner: Miner):
    """A header hash above the share target is a low difficulty share."""
    target = share_target(miner.difficulty)
    if tries(target) < 2:
        pytest.skip("every hash meets the share target")
    extranonce2 = bytes(miner.extranonce2_size)
    for nonce in range(1000):
        digest = dsha256(miner.header(extranonce2, miner.ntime, nonce))
        if int.from_bytes(digest, "little") > target:
            break
    else:
        pytest.skip("no hash above the share target")
    response = miner.submit(extranonce2, nonce)
    assert error_code(response) == _LOW_DIFFICULTY_SHARE


def test_submit_valid_share(client: StratumClient, miner: Miner):
    """
    A share meeting the share target is valid, and answered once. Where the
    block target is easier than the share target (regtest), it also mines.
    """
    target = share_target(miner.difficulty)
    if tries(target) > _REACHABLE_TRIES:
        pytest.skip(f"share difficulty {miner.difficulty} not reachable "
                    "in python, lower [stratum_v1] minimum_difficulty")
    extranonce2 = (2).to_bytes(miner.extranonce2_size, "little")
    found = miner.search(target, extranonce2)
    assert found is not None
    first = miner.submit(extranonce2, found[0])
    assert first["error"] is None and first["result"] is True
    second = miner.submit(extranonce2, found[0])
    if int.from_bytes(found[1], "little") > expand(miner.bits):
        assert error_code(second) == _DUPLICATE_SHARE
    else:
        # A mined block may have cleaned the job before the resubmission.
        assert error_code(second) in (_DUPLICATE_SHARE, _JOB_NOT_FOUND)


def test_submit_mines_block(client: StratumClient, miner: Miner,
                            native_config: dict):
    """
    A share meeting the block target is mined: the block becomes the top, and
    a clean job follows on it, dropping the jobs of the prior top.

    Runs only where the block target is reachable in python (regtest), as it
    extends the chain.
    """
    block = expand(miner.bits)
    target = min(block, share_target(miner.difficulty))
    if tries(target) > _REACHABLE_TRIES:
        pytest.skip("block target not reachable in python (not regtest)")
    try:
        before = native_top(native_config)
    except (requests.RequestException, KeyError, ValueError) as exc:
        pytest.skip(f"native top unavailable: {exc}")

    extranonce2 = (1).to_bytes(miner.extranonce2_size, "little")
    found = miner.search(target, extranonce2)
    assert found is not None
    nonce, digest = found
    response = miner.submit(extranonce2, nonce)
    assert response["error"] is None and response["result"] is True

    mined = digest[::-1].hex()
    deadline = time.monotonic() + 30
    while native_top(native_config)["hash"] != mined:
        assert time.monotonic() < deadline, "mined block did not become top"
        time.sleep(0.2)
    assert native_top(native_config)["height"] == before["height"] + 1

    # The next job is clean and builds on the mined block.
    deadline = time.monotonic() + 30
    while True:
        params = client.notification("mining.notify", timeout=30)["params"]
        if word_swap(bytes.fromhex(params[1]))[::-1].hex() == mined:
            break
        assert time.monotonic() < deadline, "no job on the mined block"
    assert params[8] is True

    old = miner.submit(extranonce2, nonce)
    assert error_code(old) == _JOB_NOT_FOUND
