"""
Robustness of the bitcoind service: json-rpc over http POST (and the tcp
downgrade), and the REST interface under /rest/.

Covers malformed json and envelopes, batches, edge parameters of the
bitcoind methods (including the boolean verbosity that broke Fulcrum),
authorization abuse when a credential is configured, and hostile REST
targets.

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_bitcoind.py --robust-services=bitcoind
"""

from __future__ import annotations

import base64
import json

import pytest

from robustness import (REACT, ENVELOPES, MALFORMED, SILENT, assert_reacts,
                        exchange, http_done, http_request, http_split,
                        run_parallel, survived)
from utils import ReferenceData


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


@pytest.fixture
def svc(robust_env):
    return robust_env.service("bitcoind")


def post(svc, body: bytes, headers=None, wait: float = REACT):
    """POST a raw body, returning (parsed response or None, outcome)."""
    if headers is None:
        headers = [("Content-Type", "application/json")] + svc.auth_headers()
    out = exchange(svc.host, svc.port,
                   [http_request("POST", "/", svc.authority, body, headers)],
                   done=http_done, wait=wait)
    return http_split(out.data), out


def rpc(ident, method, params=None) -> bytes:
    return json.dumps({"jsonrpc": "1.0", "id": ident, "method": method,
                       "params": [] if params is None else params}).encode()


def assert_answered(parsed, out, what):
    """Answered with an http response (any status), never left hanging."""
    assert_reacts(out, what)
    if parsed:
        assert parsed[0] != 0, what


# ─── Malformed json and envelopes ────────────────────────────────────────────

@pytest.mark.parametrize("case", sorted(MALFORMED))
def test_malformed_json(svc, case):
    parsed, out = post(svc, MALFORMED[case].rstrip(b"\n"))
    assert_answered(parsed, out, case)


@pytest.mark.parametrize("case", sorted(ENVELOPES))
def test_unusual_envelope(svc, case):
    parsed, out = post(svc, json.dumps(ENVELOPES[case]).encode())
    assert_answered(parsed, out, case)


@pytest.mark.parametrize("batch", [
    [], [1], [{}],
    [json.loads(rpc(i, "getblockcount")) for i in range(1000)],
    [json.loads(rpc(1, "getblockcount")), "x", None,
     json.loads(rpc(2, "no_such_method"))],
], ids=["empty", "number", "empty_object", "thousand", "mixed"])
def test_batch_shapes(svc, batch):
    parsed, out = post(svc, json.dumps(batch).encode(), wait=3 * REACT)
    assert_answered(parsed, out, "batch")


@pytest.mark.parametrize("case", ["truncated_object", "deep_array_nesting",
                                  "two_objects_one_line", "bare_null",
                                  "invalid_utf8"])
def test_malformed_json_over_tcp_downgrade(svc, case):
    if svc.user:
        pytest.skip("a credential is configured; tcp cannot carry basic auth")
    out = exchange(svc.host, svc.port, [MALFORMED[case]],
                   done=lambda d: b"\n" in d)
    if out.kind == SILENT and case == "truncated_object":
        pytest.skip("incomplete json value on a stream (legitimate wait)")
    assert_reacts(out, case)


# ─── Edge parameters ─────────────────────────────────────────────────────────

GENESIS = ReferenceData.GENESIS_HASH
GENESIS_TX = ReferenceData.GENESIS_TX_HASH

PARAMS = [
    ("getblockhash", [-1]),
    ("getblockhash", [2 ** 64]),
    ("getblockhash", ["0"]),
    ("getblock", ["zz" * 32]),
    ("getblock", [GENESIS, 99]),
    ("getblock", [GENESIS, True]),
    ("getblock", [GENESIS, False]),
    ("getblock", [GENESIS, "x"]),
    ("getblock", [GENESIS, None]),
    ("getblockheader", [GENESIS, "x"]),
    ("getblockheader", ["00" * 1000000]),
    ("getblockstats", [-1]),
    ("getblockstats", [0, ["x" * 100000] * 10]),
    ("getblockfilter", [GENESIS, "x" * 10000]),
    ("getchaintxstats", [2 ** 40]),
    ("getrawtransaction", [GENESIS_TX, True]),
    ("getrawtransaction", [GENESIS_TX, 2 ** 40]),
    ("getrawtransaction", [GENESIS_TX, False, GENESIS]),
    ("getrawtransaction", ["zz"]),
    ("gettxout", [GENESIS_TX, -1]),
    ("gettxout", [GENESIS_TX, 2 ** 40, True]),
    ("decoderawtransaction", ["ff" * 2000000]),
    ("decodescript", ["4d" + "ff" * 10]),
    ("sendrawtransaction", ["00"]),
    ("sendrawtransaction", ["ff" * 2000000]),
    ("testmempoolaccept", [["00"] * 10000]),
    ("estimatesmartfee", [-1]),
    ("estimatesmartfee", [2 ** 40, "x"]),
    ("getnetworkhashps", [-2, -2]),
    ("help", ["x" * 100000]),
    ("validateaddress", ["1" * 100000]),
    ("createrawtransaction", [[{"txid": "00" * 32, "vout": 0}] * 50000,
                              {ReferenceData.EXAMPLE_ADDRESS: 1}]),
]


@pytest.mark.parametrize("method,params", PARAMS,
                         ids=[f"{m}-{i}" for i, (m, _) in enumerate(PARAMS)])
def test_edge_parameters(svc, method, params):
    parsed, out = post(svc, rpc(7, method, params), wait=3 * REACT)
    assert_answered(parsed, out, f"{method} {params!r:.80}")
    if parsed:
        assert parsed[0] < 500 or parsed[0] == 500 and \
            b'"error"' in parsed[2], f"{method}: {parsed[0]} {parsed[2][:200]}"


def test_parallel_requests(svc):
    def work(index):
        for i in range(50):
            parsed, out = post(svc, rpc(i, "getblockcount" if i % 2 else
                                        "getbestblockhash"))
            assert parsed and parsed[0] == 200, str(out)

    errors = run_parallel(16, work)
    assert not [e for e in errors if isinstance(e, AssertionError)], \
        errors[:3]


# ─── Authorization (when a credential is configured) ─────────────────────────

@pytest.mark.parametrize("header", [
    None,
    ("Authorization", "Basic"),
    ("Authorization", "Basic !!!"),
    ("Authorization", "Basic " + base64.b64encode(b"nocolon").decode()),
    ("Authorization", "Basic " + base64.b64encode(b"robust:wrong").decode()),
    ("Authorization", "Basic " + base64.b64encode(b":").decode()),
    ("Authorization", "Bearer x"),
], ids=["missing", "no_token", "not_base64", "no_colon", "wrong_password",
        "empty_pair", "bearer"])
def test_bad_authorization(svc, header):
    if not svc.user:
        pytest.skip("no bitcoind credential configured (--bitcoind-username)")
    headers = [("Content-Type", "application/json")]
    if header:
        headers.append(header)
    parsed, out = post(svc, rpc(1, "getblockcount"), headers)
    assert_reacts(out, f"authorization {header!r:.40}")
    if parsed and parsed[0] == 200:
        assert json.loads(parsed[2]).get("error") is not None, \
            f"served with authorization {header!r:.40}"


# ─── REST ────────────────────────────────────────────────────────────────────

REST = [
    "/rest/chaininfo.json",
    "/rest/chaininfo.xml",
    "/rest/chaininfo",
    "/rest/block/" + GENESIS + ".bin",
    "/rest/block/" + GENESIS,
    "/rest/block/" + "z" * 64 + ".json",
    "/rest/block/" + "f" * 63 + ".json",
    "/rest/block/notxdetails/" + GENESIS + ".hex",
    "/rest/headers/4294967296/" + GENESIS + ".json",
    "/rest/headers/-1/" + GENESIS + ".json",
    "/rest/headers/0/" + GENESIS + ".json",
    "/rest/headers/" + "9" * 1000 + "/" + GENESIS + ".json",
    "/rest/blockhashbyheight/-1.json",
    "/rest/blockhashbyheight/18446744073709551616.json",
    "/rest/blockpart/" + GENESIS + "/4294967295/1.bin",
    "/rest/blockpart/" + GENESIS + "/0/4294967295.bin",
    "/rest/blockpart/" + GENESIS + "/-1/-1.bin",
    "/rest/blockfilter/x/" + GENESIS + ".json",
    "/rest/blockfilterheaders/basic/4294967296/" + GENESIS + ".json",
    "/rest/getutxos/" + "/".join([GENESIS_TX + "-0"] * 1000) + ".json",
    "/rest/getutxos/checkmempool/" + GENESIS_TX + "-x.json",
    "/rest/tx/" + "zz" * 32 + ".json",
    "/rest/mempool/info.json?x=" + "y" * 10000,
    "/rest/mempool/contents.json?verbose=maybe",
    "/rest/../../etc/passwd",
    "/rest/" + "a/" * 5000,
    "/rest",
]


@pytest.mark.parametrize("target", REST, ids=[f"r{i}" for i in range(len(REST))])
def test_hostile_rest_target(svc, target):
    out = exchange(svc.host, svc.port,
                   [http_request("GET", target, svc.authority,
                                 headers=svc.auth_headers())],
                   done=http_done, wait=3 * REACT)
    assert_reacts(out, target[:80])
    assert b"root:" not in out.data, "file content leaked"
    parsed = http_split(out.data)
    if parsed:
        assert parsed[0] < 500, f"{target[:80]}: server error {parsed[0]}"
