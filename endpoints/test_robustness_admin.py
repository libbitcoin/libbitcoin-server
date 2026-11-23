"""
Robustness of the admin service: the log and event subscription targets over
http GET and as websocket text messages, which push every log line or event
to the subscriber.

Covers hostile targets and filters, subscribers that never read their pushes
(the server logs every connection, so the suite itself generates the
traffic), and subscription churn.

After every case the autouse `contained` fixture checks that all services
still answer, the connection was released and no thread is pegged.

Run with:
    pytest test_robustness_admin.py --robust-services=admin,native
"""

from __future__ import annotations

import time

import pytest

from robustness import (REACT, WAIT, RawWs, SILENT, assert_reacts, connect,
                        exchange, http_done, http_request, http_split, reset,
                        run_parallel, survived, ws_text)


@pytest.fixture(autouse=True)
def contained(robust_env):
    yield
    survived(robust_env)


@pytest.fixture
def svc(robust_env):
    return robust_env.service("admin")


def open_ws(svc, **kwargs) -> RawWs:
    try:
        return RawWs(svc.host, svc.port,
                     headers=svc.auth_headers(), **kwargs)
    except ConnectionError as exc:
        pytest.skip(f"admin websocket unavailable ({exc})")


TARGETS = [
    "/v1/log/subscribe?format=json",
    "/v1/event/subscribe?format=json",
    "/v1/log/subscribe?format=json&filter=18446744073709551615",
    "/v1/log/subscribe?format=json&filter=18446744073709551616",
    "/v1/log/subscribe?format=json&filter=-1",
    "/v1/log/subscribe?format=json&filter=x",
    "/v1/log/subscribe?format=json&filter=" + "9" * 10000,
    "/v1/event/subscribe?format=text",
    "/v1/event/subscribe?format=xml",
    "/v1/event/subscribe?format=json&format=html",
    "/v1/log",
    "/v1/log/",
    "/v1/log/unsubscribe",
    "/v1/log/subscribe/x",
    "/v1/event",
    "/v255/log/subscribe",
    "/v256/log/subscribe",
    "/vx/log/subscribe",
    "/v1",
    "/v1/",
    "/v1/bogus",
    "/../../etc/passwd",
    "/" + "a/" * 5000,
]


@pytest.mark.parametrize("target", TARGETS,
                         ids=[f"t{i}" for i in range(len(TARGETS))])
def test_hostile_http_target(svc, target):
    out = exchange(svc.host, svc.port,
                   [http_request("GET", target, svc.authority,
                                 headers=svc.auth_headers())],
                   done=http_done, wait=2 * REACT)
    assert_reacts(out, target[:80])
    assert b"root:" not in out.data, "file content leaked"
    parsed = http_split(out.data)
    if parsed:
        assert parsed[0] < 500, f"{target[:80]}: server error {parsed[0]}"


@pytest.mark.parametrize("target", TARGETS + [
    "", " ", "x" * 1000000, "/v1/log/subscribe\x00",
    "GET /v1/log/subscribe HTTP/1.1",
], ids=[f"w{i}" for i in range(len(TARGETS) + 5)])
def test_hostile_ws_target(svc, target):
    """Every ws message must be answered (pushes count) or end the session;
    an ignored message leaves the client without a reaction."""
    ws = open_ws(svc)
    try:
        ws.text(target)
        outcome = ws.outcome()
        if outcome == SILENT and not target.strip():
            pytest.skip("empty message ignored")
        assert outcome != SILENT, f"{target[:40]!r}: no reaction"
    finally:
        ws.close()


@pytest.mark.parametrize("targets", [
    ["/v1/log/subscribe?format=json"],
    ["/v1/event/subscribe?format=json"],
    ["/v1/log/subscribe?format=json", "/v1/event/subscribe?format=json"],
])
def test_subscription_never_read(svc, robust_env, targets):
    """Subscribe to every log line and stop reading. Connections made to
    probe the server are logged, so the pushes keep coming: the backlog must
    be bounded while everyone else is served."""
    ws = open_ws(svc, rcvbuf=4096)
    try:
        for target in targets:
            ws.text(target)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            # Generate log traffic and check the others are served.
            for _ in range(20):
                reset(connect(svc.host, svc.port))
            start = time.monotonic()
            out = exchange(svc.host, svc.port, [svc.http_probe()],
                           done=http_done)
            parsed = http_split(out.data)
            assert parsed and svc.http_ok(parsed[0], parsed[2]), \
                f"others not served: {out}"
            assert time.monotonic() - start < REACT
    finally:
        ws.reset()


def test_subscribe_repeatedly(svc):
    ws = open_ws(svc)
    try:
        ws.send(b"".join(ws_text(t) for t in
                         ["/v1/log/subscribe?format=json",
                          "/v1/event/subscribe?format=json"] * 300))
        assert ws.outcome() != SILENT
    finally:
        ws.close()


def test_subscription_churn(svc):
    def work(index):
        for round_ in range(10):
            try:
                ws = RawWs(svc.host, svc.port, headers=svc.auth_headers())
            except (OSError, ConnectionError):
                continue
            ws.text("/v1/log/subscribe?format=json" if round_ % 2 else
                    "/v1/event/subscribe?format=json")
            (ws.reset if index % 2 else ws.close)()

    run_parallel(12, work)
    time.sleep(WAIT)
