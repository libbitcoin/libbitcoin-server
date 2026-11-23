#!/usr/bin/env python3
"""
Synchronize bs on each network from its peers, then test endpoints.

Source: bs (the built console executable) and endpoints/ (pytest suite).
Output: pass/fail per network, nonzero exit on any failure.

Run from the repository root:
  python3 tools/sync_networks.py <bs> [network:height ...]

Each network syncs into a fresh store under a temporary directory, to the
given maximum height, within the timeout. The smoke tests
(endpoints/test_smoke.py) then run against each synced node, and the full
endpoint suite against mainnet (its reference data is mainnet). Regtest has no
public peers, so one node mines the blocks (by submitblock) and a second syncs
them from the first.
Defaults to mainnet:10000, testnet3:2000, testnet4:2000, testnet5:1000,
signet:2000 and regtest:200.

Requires python3, and pytest, requests and websocket-client for the tests.
"""

import argparse
import hashlib
import json
import os
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request

DEFAULTS = ["mainnet:10000", "testnet3:2000", "testnet4:2000",
    "testnet5:1000", "signet:2000", "regtest:200"]
POLL_SECONDS = 5
CLOSE_SECONDS = 120

# Interface ports of the first node, offset for the second (regtest).
ADMIN = 8080
NATIVE = 8181
ESPLORA = 3000
BITCOIND = 8332
BTCD = 8334
ELECTRUM = 50001
OFFSET = 10

# The network-neutral smoke tests, and the suite of mainnet reference data.
SMOKE = os.path.join("endpoints", "test_smoke.py")
ENDPOINTS = "endpoints"
REGTEST_PEER = 18444

CONFIG = """[folders]
store = {store}
console = {folder}
events = {folder}
peers = {folder}

[node]
maximum_height = {height}
delay_inbound = false

[native]
bind = 127.0.0.1:{native}
connections = 16

[esplora]
bind = 127.0.0.1:{esplora}
connections = 16

[bitcoind]
bind = 127.0.0.1:{bitcoind}
connections = 16
enable_utxo = true

[btcd]
bind = 127.0.0.1:{btcd}
connections = 16

[electrum]
bind = 127.0.0.1:{electrum}
connections = 16

[admin]
bind = 127.0.0.1:{admin}

{network}"""

# Syncs from public peers, accepting none.
PUBLIC = """[inbound]
connections = 0
"""

# Accepts the syncing node, connecting to none.
MINER = """[outbound]
connections = 0

[inbound]
bind = 127.0.0.1:{peer}
connections = 8
enable_loopback = true
"""

# Syncs from the miner only.
FOLLOWER = """[outbound]
connections = 0

[inbound]
connections = 0

[manual]
peer = 127.0.0.1:{peer}
"""

# Regtest genesis hash and timestamp, and the proof of work limit (bits).
REGTEST_GENESIS = (
    "0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")
REGTEST_BITS = 0x207fffff
REGTEST_SUBSIDY_INTERVAL = 150

# ── node ────────────────────────────────────────────────────────────────────

def confirmed_height(offset=0):
    """The confirmed top height from the native interface, or None."""
    url = "http://127.0.0.1:{}/v1/top?format=json".format(NATIVE + offset)
    try:
        with urllib.request.urlopen(url, timeout=5) as response:
            return json.load(response)["confirmed"]["height"]
    except Exception:
        return None


def start(bs, folder, network, height, peering=PUBLIC, offset=0):
    """Write the configuration and start bs, returning the process."""
    os.makedirs(folder)
    config = os.path.join(folder, "bs.cfg")
    with open(config, "w") as file:
        file.write(CONFIG.format(store=os.path.join(folder, "store"),
            folder=folder, height=height, admin=ADMIN + offset,
            native=NATIVE + offset,
            esplora=ESPLORA + offset, bitcoind=BITCOIND + offset,
            btcd=BTCD + offset, electrum=ELECTRUM + offset,
            network=peering.format(peer=REGTEST_PEER)))

    log = open(os.path.join(folder, "run.stdout"), "w")
    return subprocess.Popen([bs, "--config", config, "--net", network,
        "--accept"], stdin=subprocess.PIPE, stdout=log,
        stderr=subprocess.STDOUT)


def close(process, folder):
    """Close bs gracefully (interrupt, then the exit prompt), True if clean."""
    if process.poll() is not None:
        return False

    process.send_signal(signal.SIGINT)
    log = os.path.join(folder, "run.stdout")
    deadline = time.time() + CLOSE_SECONDS
    while time.time() < deadline:
        with open(log, errors="replace") as file:
            if "== shutdown ==" in file.read():
                break
        time.sleep(1)
    else:
        process.kill()
        return False

    # bs waits at "Press <enter> to exit..." after shutdown.
    try:
        process.communicate(b"\n", timeout=30)
    except subprocess.TimeoutExpired:
        process.kill()

    return True


def tail(folder, lines=60):
    """Print the end of the node log, less store file operations."""
    with open(os.path.join(folder, "run.stdout"), errors="replace") as file:
        kept = [line for line in file
            if "_file(" not in line and "_table(" not in line]
        for line in kept[-lines:]:
            print("    " + line.rstrip(), flush=True)

def wait(process, folder, name, height, timeout, offset=0):
    """Wait for the node to confirm height, True if it does in time."""
    begin = time.time()
    reached = None
    while time.time() - begin < timeout:
        if process.poll() is not None:
            print("{}: bs exited with {}".format(name, process.returncode))
            tail(folder)
            return False

        with open(os.path.join(folder, "run.stdout"), errors="replace") as file:
            if "== shutdown ==" in file.read():
                print("{}: bs shut down".format(name))
                tail(folder)
                return False

        reached = confirmed_height(offset)
        if reached is not None and reached >= height:
            break
        time.sleep(POLL_SECONDS)

    seconds = int(time.time() - begin)
    if reached is None or reached < height:
        print("{}: reached {} of {} in {}s (timeout)".format(name,
            reached, height, seconds))
        tail(folder)
        return False

    print("{}: reached {} in {}s".format(name, reached, seconds), flush=True)
    return True

# ── regtest mining ──────────────────────────────────────────────────────────

def sha256d(data):
    """The bitcoin double sha256 hash."""
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def script_number(value):
    """A bip34 minimal push of the height."""
    if 1 <= value <= 16:
        return bytes([0x50 + value])

    data = b""
    while value:
        data += bytes([value & 0xff])
        value >>= 8
    if data[-1] & 0x80:
        data += b"\x00"

    return bytes([len(data)]) + data


def coinbase(height):
    """A coinbase of the subsidy to op_true, by bip34 and bip54 rules."""
    script = script_number(height) + b"\x04" + struct.pack("<I", height)
    subsidy = (50 * 100000000) >> (height // REGTEST_SUBSIDY_INTERVAL)
    return (struct.pack("<I", 1) + b"\x01" + b"\x00" * 32 +
        struct.pack("<I", 0xffffffff) + bytes([len(script)]) + script +
        struct.pack("<I", 0xfffffffe) + b"\x01" + struct.pack("<Q", subsidy) +
        b"\x01\x51" + struct.pack("<I", height - 1))


def mine(previous, height, timestamp):
    """A serialized block of only a coinbase, solved at the regtest limit."""
    tx = coinbase(height)
    target = (REGTEST_BITS & 0xffffff) << (8 * ((REGTEST_BITS >> 24) - 3))
    prefix = (struct.pack("<I", 0x20000000) + previous + sha256d(tx) +
        struct.pack("<II", timestamp, REGTEST_BITS))
    for nonce in range(1 << 32):
        header = prefix + struct.pack("<I", nonce)
        digest = sha256d(header)
        if int.from_bytes(digest, "little") <= target:
            return digest, header + b"\x01" + tx

    raise RuntimeError("no regtest solution")


def submit(block):
    """Submit a block to the miner, returning the bitcoind result."""
    payload = json.dumps({"jsonrpc": "1.0", "id": 1, "method": "submitblock",
        "params": [block.hex()]}).encode()
    request = urllib.request.Request("http://127.0.0.1:{}/".format(BITCOIND),
        data=payload, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def generate(count):
    """Mine count blocks above genesis to the miner, True if all accepted."""
    previous = bytes.fromhex(REGTEST_GENESIS)[::-1]
    start = int(time.time()) - count
    for height in range(1, count + 1):
        previous, block = mine(previous, height, start + height)
        response = submit(block)
        if response.get("error") is not None or response.get("result"):
            print("regtest: block {} refused: {}".format(height, response))
            return False

    return True


def regtest(bs, root, height, timeout):
    """Mine height blocks to one node and sync a second node from it."""
    print("regtest: mining {}".format(height), flush=True)
    miner = os.path.join(root, "regtest-miner")
    follower = os.path.join(root, "regtest-follower")
    mining = start(bs, miner, "regtest", height, MINER)
    syncing = None
    passed = False
    try:
        if not wait(mining, miner, "regtest-miner", 0, timeout):
            return False

        if not generate(height):
            tail(miner)
            return False

        if not wait(mining, miner, "regtest-miner", height, timeout):
            return False

        print("regtest: syncing to {}".format(height), flush=True)
        syncing = start(bs, follower, "regtest", height, FOLLOWER, OFFSET)
        passed = (wait(syncing, follower, "regtest-follower", height,
            timeout, OFFSET) and
            suite(follower, "regtest-follower", SMOKE, OFFSET))
    finally:
        for process, folder in ((syncing, follower), (mining, miner)):
            if process is not None and not close(process, folder):
                print("regtest: bs did not close cleanly")
                tail(folder)
                passed = False

    return passed

# ── run ─────────────────────────────────────────────────────────────────────

def suite(folder, name, tests, offset=0):
    """Run endpoint tests against the node, True if all pass."""
    # Joined values, else pytest takes the log path as a test path (and root).
    result = subprocess.run([sys.executable, "-m", "pytest", "-q",
        "-m", "not slow", tests,
        "--native-port={}".format(NATIVE + offset),
        "--admin-port={}".format(ADMIN + offset),
        "--esplora-port={}".format(ESPLORA + offset),
        "--bitcoind-rpc-port={}".format(BITCOIND + offset),
        "--btcd-port={}".format(BTCD + offset),
        "--electrum-port={}".format(ELECTRUM + offset),
        "--log={}".format(os.path.join(folder, "run.stdout"))])
    passed = result.returncode == 0
    print("{}: {} {}".format(name, tests, "passed" if passed else "failed"),
        flush=True)
    return passed


def sync(bs, root, network, height, timeout):
    """Sync one network to height, then test endpoints on mainnet."""
    if network == "regtest":
        return regtest(bs, root, height, timeout)

    folder = os.path.join(root, network)
    print("{}: syncing to {}".format(network, height), flush=True)
    process = start(bs, folder, network, height)
    if not wait(process, folder, network, height, timeout):
        close(process, folder)
        return False

    tests = ENDPOINTS if network == "mainnet" else SMOKE
    passed = suite(folder, network, tests)

    if not close(process, folder):
        print("{}: bs did not close cleanly".format(network))
        tail(folder)
        return False

    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    parser.add_argument("bs", help="bs executable")
    parser.add_argument("networks", nargs="*", default=DEFAULTS,
        help="network:height pairs")
    parser.add_argument("--timeout", type=int, default=1800,
        help="seconds allowed per network sync")
    options = parser.parse_args()

    root = tempfile.mkdtemp(prefix="bs-networks-")
    failed = []
    try:
        for pair in options.networks:
            network, height = pair.split(":")
            if not sync(options.bs, root, network, int(height),
                options.timeout):
                failed.append(network)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    if failed:
        sys.exit("Failed: {}".format(", ".join(failed)))

    print("All networks passed.")


if __name__ == "__main__":
    main()
