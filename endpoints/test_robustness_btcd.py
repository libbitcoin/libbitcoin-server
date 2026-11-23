"""
Robustness of the btcd service: json-rpc 1.0 over a persistent websocket
(with the authenticate handshake) and over http POST (basic auth).

Covers the authorization path under abuse (wrong, malformed and repeated
credentials, methods before authenticate), malformed json and envelopes,
edge parameters of the btcd extension methods (filters, rescans,
notifications), and clients that subscribe to notifications and stop reading.

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_btcd.py --btcd-username=u --btcd-password=p
    pytest test_robustness_btcd.py --bs-pid=$(pgrep -f 'bs.*--config')
"""

from __future__ import annotations

import base64
import json
import time

import pytest

from robustness import (REACT, WAIT, CLOSED, ENVELOPES, MALFORMED, REPLY,
                        SILENT, OP_TEXT, RawWs, assert_reacts, basic,
                        exchange, http_done, http_request, http_split,
                        run_parallel, stream, survived, ws_text)
from utils import ReferenceData


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


@pytest.fixture
def svc(robust_env):
    return robust_env.service("btcd")


def rpc(ident, method, params=None) -> str:
    return json.dumps({"jsonrpc": "1.0", "id": ident, "method": method,
                       "params": [] if params is None else params})


class Client:
    """A btcd websocket session, authenticated unless told otherwise."""

    def __init__(self, svc, authenticate: bool = True, rcvbuf=None,
                 headers=()):
        self.svc = svc
        try:
            self.ws = RawWs(svc.host, svc.port, rcvbuf=rcvbuf,
                            headers=headers)
        except ConnectionError as exc:
            pytest.skip(f"btcd: no websocket ({exc})")
        if authenticate and svc.user:
            reply = self.call("auth", "authenticate",
                              [svc.user, svc.password])
            assert isinstance(reply, dict) and reply.get("error") is None, \
                reply

    def send(self, text) -> bool:
        return self.ws.text(text)

    def next(self, wait: float = REACT):
        """Next json message, CLOSED, or None on timeout."""
        got = self.ws.message(wait)
        if got is None or got == CLOSED or got[0] != OP_TEXT:
            return None if got is None else CLOSED
        try:
            return json.loads(got[1])
        except ValueError:
            return got[1]

    def call(self, ident, method, params=None, wait: float = REACT):
        self.send(rpc(ident, method, params))
        while True:
            reply = self.next(wait)
            if isinstance(reply, dict) and "id" not in reply:
                continue                    # a notification
            return reply

    def outcome(self, wait: float = REACT) -> str:
        reply = self.next(wait)
        if reply is None:
            return SILENT
        return CLOSED if reply == CLOSED else REPLY

    def works(self) -> bool:
        reply = self.call("w", "getblockcount")
        return isinstance(reply, dict) and reply.get("error") is None

    def close(self):
        self.ws.close()

    def reset(self):
        self.ws.reset()


@pytest.fixture
def client(svc):
    c = Client(svc)
    yield c
    c.close()


def no_result(reply) -> bool:
    """True if a reply carries no successful result (error or closed)."""
    return reply == CLOSED or (isinstance(reply, dict) and
                               reply.get("error") is not None)


def needs_credentials(svc):
    if not svc.user:
        pytest.skip("no btcd credential configured (--btcd-username)")


# ─── Authorization ───────────────────────────────────────────────────────────

def test_method_before_authenticate_is_refused(svc):
    needs_credentials(svc)
    c = Client(svc, authenticate=False)
    try:
        reply = c.call(1, "getblockcount")
        assert reply is not None, "no reaction"
        assert no_result(reply), f"served without authenticate: {reply}"
    finally:
        c.close()


@pytest.mark.parametrize("params", [
    [], ["robust"], [1, 2], [None, None], [{"a": 1}, []],
    ["robust", "wrong"], ["", ""], ["u" * 1000000, "p"],
    ["robust", "robust", "extra"], ["robust\x00", "robust"],
    ["robust", "robust\n"],
], ids=["empty", "one", "numbers", "nulls", "object", "wrong_password",
        "empty_strings", "huge_user", "extra_param", "nul_user",
        "newline_password"])
def test_bad_authenticate(svc, params):
    needs_credentials(svc)
    c = Client(svc, authenticate=False)
    try:
        reply = c.call(1, "authenticate", params)
        assert reply is not None, "no reaction"
        assert no_result(reply), f"accepted {params!r:.60}: {reply}"
    finally:
        c.close()


def test_wrong_authenticate_repeated_on_one_connection(svc):
    needs_credentials(svc)
    c = Client(svc, authenticate=False)
    try:
        for i in range(200):
            reply = c.call(i, "authenticate", [svc.user, f"wrong{i}"])
            if reply in (None, CLOSED):
                break
            assert no_result(reply), reply
        assert reply is not None, "no reaction"
    finally:
        c.close()


def test_wrong_authenticate_in_parallel(svc):
    needs_credentials(svc)

    def work(index):
        for i in range(10):
            c = Client(svc, authenticate=False)
            try:
                reply = c.call(i, "authenticate", [svc.user, "x" * index])
                assert reply is not None and no_result(reply), reply
            finally:
                c.reset()

    errors = run_parallel(16, work)
    assert not [e for e in errors if isinstance(e, AssertionError)], errors[:3]


def test_authenticate_twice(svc):
    needs_credentials(svc)
    c = Client(svc)
    try:
        reply = c.call(2, "authenticate", [svc.user, svc.password])
        assert reply is not None, "no reaction"
        if reply != CLOSED:
            assert c.works() or no_result(reply)
    finally:
        c.close()


def test_authenticate_then_wrong_authenticate(svc):
    """A failed re-authenticate must not leave a half-authorized session."""
    needs_credentials(svc)
    c = Client(svc)
    try:
        reply = c.call(2, "authenticate", [svc.user, "wrong"])
        assert reply is not None, "no reaction"
        if reply == CLOSED:
            return
        after = c.call(3, "getblockcount")
        assert after is not None, "no reaction after failed re-authenticate"
    finally:
        c.close()


@pytest.mark.parametrize("header", [
    None,
    ("Authorization", "Basic"),
    ("Authorization", "Basic !!!"),
    ("Authorization", "Basic " + base64.b64encode(b"nocolon").decode()),
    ("Authorization", "Basic " + base64.b64encode(b"robust:wrong").decode()),
    ("Authorization", "Basic " + base64.b64encode(b":").decode()),
    ("Authorization", "Basic " + base64.b64encode(
        b"robust:" + b"x" * 100000).decode()),
    ("Authorization", "basic " + base64.b64encode(b"robust:robust").decode()),
    ("Authorization", "Digest username=robust"),
], ids=["missing", "no_token", "not_base64", "no_colon", "wrong_password",
        "empty_pair", "huge_password", "lowercase_scheme", "digest"])
def test_http_bad_authorization(svc, header):
    needs_credentials(svc)
    body = rpc(1, "getblockcount").encode()
    headers = [("Content-Type", "application/json")]
    if header:
        headers.append(header)
    out = exchange(svc.host, svc.port,
                   [http_request("POST", "/", svc.authority, body, headers)],
                   done=http_done)
    assert_reacts(out, f"authorization {header!r:.40}")
    parsed = http_split(out.data)
    if parsed and parsed[0] == 200:
        value = json.loads(parsed[2])
        if "lowercase_scheme" not in str(header):
            assert value.get("error") is not None, \
                f"served with authorization {header!r:.40}: {value}"


def test_ws_upgrade_with_basic_auth(svc):
    """Basic auth on the upgrade request authorizes the websocket too."""
    needs_credentials(svc)
    c = Client(svc, authenticate=False,
               headers=[basic(svc.user, svc.password)])
    try:
        reply = c.call(1, "getblockcount")
        assert isinstance(reply, dict), reply
    finally:
        c.close()


# ─── Malformed json and envelopes ────────────────────────────────────────────

WS_MALFORMED = [k for k in sorted(MALFORMED) if k not in (
    "invalid_utf8", "overlong_utf8", "unterminated_deep_nesting")]


@pytest.mark.parametrize("case", WS_MALFORMED)
def test_malformed_json_over_websocket(client, case):
    client.send(MALFORMED[case].rstrip(b"\n"))
    assert client.outcome() != SILENT, f"{case}: no reaction"


@pytest.mark.parametrize("case", ["deep_array_nesting", "truncated_object",
                                  "huge_exponent", "lone_surrogate",
                                  "two_objects_one_line", "bare_null"])
def test_malformed_json_over_http(svc, case):
    body = MALFORMED[case].rstrip(b"\n")
    request = http_request("POST", "/", svc.authority, body,
                           [("Content-Type", "application/json")] +
                           svc.auth_headers())
    assert_reacts(exchange(svc.host, svc.port, [request], done=http_done),
                  case)


@pytest.mark.parametrize("case", sorted(ENVELOPES))
def test_unusual_envelope(client, case):
    client.send(json.dumps(ENVELOPES[case]))
    assert client.outcome() != SILENT, f"{case}: no reaction"


def test_request_without_id(client):
    """json-rpc 1.0 requires an id (null for a notification): a missing one
    may be answered, ignored or closed, but must not wedge the session."""
    client.send(json.dumps({"jsonrpc": "1.0", "method": "getblockcount",
                            "params": []}))
    reply = client.next(WAIT)
    if reply != CLOSED:
        assert client.works()


@pytest.mark.parametrize("batch", [
    [], [1],
    [json.loads(rpc(i, "getblockcount")) for i in range(500)],
], ids=["empty", "number", "five_hundred"])
def test_batch_shapes(client, batch):
    client.send(json.dumps(batch))
    assert client.outcome() != SILENT


# ─── Edge parameters ─────────────────────────────────────────────────────────

ADDRESS = ReferenceData.EXAMPLE_ADDRESS
OUTPOINT = {"hash": ReferenceData.GENESIS_TX_HASH, "index": 0}

PARAMS = [
    ("getblockhash", [-1]),
    ("getblockhash", [2 ** 64]),
    ("getblockhash", ["0"]),
    ("getblock", ["zz" * 32]),
    ("getblock", [ReferenceData.GENESIS_HASH, 99]),
    ("getblock", [ReferenceData.GENESIS_HASH, True]),
    ("getblock", [ReferenceData.GENESIS_HASH, "x"]),
    ("getblockheader", [ReferenceData.GENESIS_HASH, "x"]),
    ("getblockheader", ["00" * 1000000]),
    ("getrawtransaction", [ReferenceData.GENESIS_TX_HASH, True]),
    ("getrawtransaction", [ReferenceData.GENESIS_TX_HASH, 2 ** 40]),
    ("gettxout", [ReferenceData.GENESIS_TX_HASH, -1]),
    ("gettxout", [ReferenceData.GENESIS_TX_HASH, 2 ** 40, True]),
    ("loadtxfilter", []),
    ("loadtxfilter", [True]),
    ("loadtxfilter", ["yes", [ADDRESS], []]),
    ("loadtxfilter", [True, ADDRESS, []]),
    ("loadtxfilter", [True, [ADDRESS] * 100000, []]),
    ("loadtxfilter", [True, [], [OUTPOINT] * 100000]),
    ("loadtxfilter", [True, [], [{"hash": ReferenceData.GENESIS_TX_HASH,
                                  "index": -1}]]),
    ("loadtxfilter", [True, [], [{"hash": ReferenceData.GENESIS_TX_HASH,
                                  "index": 2 ** 40}]]),
    ("loadtxfilter", [True, ["x" * 100000], []]),
    ("rescanblocks", []),
    ("rescanblocks", [ReferenceData.GENESIS_HASH]),
    ("rescanblocks", [[ReferenceData.GENESIS_HASH] * 20000]),
    ("rescanblocks", [["00" * 32] * 20000]),
    ("rescan", [ReferenceData.GENESIS_HASH, [], [], "zz"]),
    ("rescan", [ReferenceData.GENESIS_HASH, None, None, None]),
    ("notifyreceived", [[ADDRESS] * 100000]),
    ("notifyreceived", [ADDRESS]),
    ("notifyspent", [[OUTPOINT] * 100000]),
    ("notifyspent", [[{"hash": 5, "index": "x"}]]),
    ("createrawtransaction", [[{"txid": "00" * 32, "vout": 0}] * 50000,
                              {ADDRESS: 1}]),
    ("createrawtransaction", [[], {ADDRESS: -1}]),
    ("createrawtransaction", [[], {ADDRESS: 1e300}]),
    ("decoderawtransaction", ["ff" * 2000000]),
    ("decoderawtransaction", ["0" * 3]),
    ("decodescript", ["4d" + "ff" * 10]),
    ("decodescript", ["00" * 1000000]),
    ("validateaddress", ["1" * 100000]),
    ("help", ["x" * 100000]),
    ("session", [1, 2, 3]),
    ("getcurrentnet", [None]),
]


@pytest.mark.parametrize("method,params", PARAMS,
                         ids=[f"{m}-{i}" for i, (m, _) in enumerate(PARAMS)])
def test_edge_parameters(client, method, params):
    client.send(rpc(7, method, params))
    assert client.outcome(3 * REACT) != SILENT, f"{method} {params!r:.80}"


def test_filter_replace_churn(client):
    """loadtxfilter reload/append cycles must not accumulate without bound."""
    payload = []
    for i in range(300):
        payload.append(rpc(i, "loadtxfilter", [i % 2 == 0, [ADDRESS] * 100,
                                               [OUTPOINT] * 100]))
    for message in payload:
        client.send(message)
    replies = 0
    while replies < len(payload):
        reply = client.next(2 * REACT)
        if not isinstance(reply, dict):
            break
        replies += 1
    assert replies == len(payload) or reply == CLOSED, \
        f"{replies} replies, then {reply!r}"


def test_notify_toggle_churn(client):
    for i in range(500):
        client.send(rpc(i, "notifyblocks" if i % 2 == 0 else
                        "stopnotifyblocks"))
    replies = 0
    while replies < 500:
        reply = client.next(2 * REACT)
        if not isinstance(reply, dict):
            break
        if "id" in reply:
            replies += 1
    assert replies == 500 or reply == CLOSED, f"{replies}, then {reply!r}"


# ─── Volume and slow readers ─────────────────────────────────────────────────

def test_pipelined_messages_answered_in_order(client):
    count = 1000
    for i in range(count):
        client.send(rpc(i, "getblockcount"))
    ids = []
    while len(ids) < count:
        reply = client.next(2 * REACT)
        assert isinstance(reply, dict), f"after {len(ids)}: {reply!r}"
        if "id" in reply:
            ids.append(reply["id"])
    assert ids == list(range(count))


def test_requests_without_reading_responses(svc):
    c = Client(svc, rcvbuf=4096)
    try:
        burst = b"".join(ws_text(rpc(i, "getblock",
                                     [ReferenceData.GENESIS_HASH, 2]))
                         for i in range(200))
        stream(c.ws.sock, b"", burst, 32 * 1024 * 1024, stall=1.0)
        other = Client(svc)
        try:
            start = time.monotonic()
            assert other.works()
            assert time.monotonic() - start < REACT
        finally:
            other.close()
    finally:
        c.reset()


def test_notifications_never_read(svc):
    """notifyblocks on a syncing node, then the client stops reading: the
    server must bound the backlog and keep serving everyone else."""
    c = Client(svc, rcvbuf=4096)
    try:
        reply = c.call(1, "notifyblocks")
        assert isinstance(reply, dict) and reply.get("error") is None, reply
        c.call(2, "loadtxfilter", [True, [ADDRESS], [OUTPOINT]])
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            time.sleep(2)
            other = Client(svc)
            try:
                assert other.works()
            finally:
                other.close()
    finally:
        c.reset()


def test_disconnect_during_rescan(svc):
    for _ in range(10):
        c = Client(svc)
        c.send(rpc(1, "loadtxfilter", [True, [ADDRESS], []]))
        c.send(rpc(2, "rescanblocks", [[ReferenceData.GENESIS_HASH] * 2000]))
        c.reset()


def test_good_clients_served_among_bad_ones(svc):
    import threading

    def bad(index):
        for _ in range(10):
            c = Client(svc, authenticate=index % 2 == 0)
            c.send(MALFORMED[WS_MALFORMED[index % len(WS_MALFORMED)]]
                   .rstrip(b"\n"))
            c.send(rpc(1, "rescanblocks", [["00" * 32] * 5000]))
            c.reset()

    def good(index):
        c = Client(svc)
        try:
            for i in range(50):
                reply = c.call(i, "getbestblockhash" if i % 2 else
                               "getblockcount")
                assert isinstance(reply, dict) and \
                    reply.get("error") is None, reply
        finally:
            c.close()

    errors = []
    bad_thread = threading.Thread(target=lambda: errors.extend(
        run_parallel(8, bad)))
    bad_thread.start()
    errors += run_parallel(8, good)
    bad_thread.join(120)
    failures = [e for e in errors if isinstance(e, AssertionError)]
    assert not failures, failures[:3]
