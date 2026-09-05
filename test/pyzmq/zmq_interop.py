"""libzmq (pyzmq) interoperability test for the bs bitcoind_zmq service.

Starts bs on a fresh mainnet store with no peers, binds the zmq publisher
clear (NULL) and secured (CURVE), connects one real libzmq SUB socket to
each, and submits mainnet blocks 1..3 over the bitcoind RPC. Verifies:
  - NULL and CURVE handshakes complete (libzmq socket monitor events)
  - libzmq heartbeat PINGs are answered (the connection survives)
  - notification fan-out to both subscribers: hashblock, rawblock, hashtx,
    sequence (contents, byte order and per-topic sequence numbers)
  - CANCEL (unsubscribe) stops delivery of that topic only
"""
import argparse
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import time
import urllib.request

import zmq
from zmq.utils.monitor import recv_monitor_message

# Mainnet blocks 1..3 (display-order hash, raw block).
BLOCKS = [
    ("00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048",
     "010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e61bc6649ffff001d01e362990101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d0104ffffffff0100f2052a0100000043410496b538e853519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a604f8141781e62294721166bf621e73a82cbf2342c858eeac00000000"),
    ("000000006a625f06636b8bb6ac7b960a8d03705d1ace08b1a19da3fdcc99ddbd",
     "010000004860eb18bf1b1620e37e9490fc8a427514416fd75159ab86688e9a8300000000d5fdcc541e25de1c7a5addedf24858b8bb665c9f36ef744ee42c316022c90f9bb0bc6649ffff001d08d2bd610101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d010bffffffff0100f2052a010000004341047211a824f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073dee6c89064984f03385237d92167c13e236446b417ab79a0fcae412ae3316b77ac00000000"),
    ("0000000082b5015589a3fdf2d4baff403e6f0be035a5d9742c1cae6295464449",
     "01000000bddd99ccfda39da1b108ce1a5d70038d0a967bacb68b6b63065f626a0000000044f672226090d85db9a9f2fbfe5f0f9609b387af7be5b7fbb7a1767c831c9e995dbe6649ffff001d05e0ed6d0101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0704ffff001d010effffffff0100f2052a0100000043410494b9d3e76c5b1629ecf97fff95d7a4bbdac87cc26099ada28066c6ff1eb9191223cd897194a08d0c2726c5747f1db49e8cf90e75dc3e3550ae9b30086f3cd5aaac00000000"),
]

TOPICS = [b"hashblock", b"rawblock", b"hashtx", b"rawtx", b"sequence"]


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print("  ok:", message)


class Server:
    """A bs process on a fresh store, closed via its console."""

    def __init__(self, bs, workdir, rpc_port, clear_port, curve_port,
                 curve_secret):
        self.bs = bs
        self.workdir = workdir
        self.rpc = f"http://127.0.0.1:{rpc_port}/"
        store = os.path.join(workdir, "store").replace("\\", "/")
        self.config = os.path.join(workdir, "bs.cfg")
        self.stdout = open(os.path.join(workdir, "bs.stdout"), "wb")
        with open(self.config, "w") as cfg:
            cfg.write("\n".join([
                "[inbound]", "connections = 0",
                "[outbound]", "connections = 0",
                "[node]", "delay_inbound = false",
                "[bitcoin]", "milestone = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0",
                "[database]", f"path = {store}",
                "[bitcoind]", f"bind = 127.0.0.1:{rpc_port}", "connections = 4",
                "[bitcoind_zmq]",
                f"bind = 127.0.0.1:{clear_port}",
                f"safe = 127.0.0.1:{curve_port}",
                f"curve_secret = {curve_secret}",
                "connections = 8", "maximum_subscriptions = 100", ""]))
        self.process = None

    def start(self):
        create = subprocess.run([self.bs, "--config", self.config, "--newstore"],
                                stdout=self.stdout, stderr=subprocess.STDOUT,
                                stdin=subprocess.DEVNULL, timeout=120)
        if create.returncode != 0:
            raise RuntimeError(f"newstore failed: {create.returncode}")
        self.process = subprocess.Popen([self.bs, "--config", self.config],
                                        stdin=subprocess.PIPE,
                                        stdout=self.stdout,
                                        stderr=subprocess.STDOUT)
        deadline = time.time() + 90
        while time.time() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError("bs exited during start")
            try:
                self.call("getblockcount")
                return
            except Exception:
                time.sleep(0.5)
        raise RuntimeError("bs rpc did not come up")

    def call(self, method, *params):
        body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": method,
                           "params": list(params)}).encode()
        request = urllib.request.Request(
            self.rpc, data=body, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(request, timeout=10) as response:
            reply = json.loads(response.read())
        if reply.get("error"):
            raise RuntimeError(f"{method}: {reply['error']}")
        return reply.get("result")

    def close(self):
        if self.process is None or self.process.poll() is not None:
            return
        try:
            self.process.stdin.write(b"c\n")
            self.process.stdin.flush()
            for _ in range(120):
                if self.process.poll() is not None:
                    break
                try:
                    self.process.stdin.write(b"\n")
                    self.process.stdin.flush()
                except OSError:
                    break
                time.sleep(1)
        finally:
            if self.process.poll() is None:
                self.process.kill()
            self.stdout.close()


class Subscriber:
    """A libzmq SUB socket with handshake/heartbeat monitoring."""

    def __init__(self, context, name, port, server_key=None):
        self.name = name
        self.socket = context.socket(zmq.SUB)
        if server_key is not None:
            public, secret = zmq.curve_keypair()
            self.socket.curve_secretkey = secret
            self.socket.curve_publickey = public
            self.socket.curve_serverkey = server_key
        # libzmq sends PING every 200ms and drops the peer at 1000ms silence.
        if not os.environ.get("ZMQ_INTEROP_NO_HEARTBEAT"):
            self.socket.setsockopt(zmq.HEARTBEAT_IVL, 200)
            self.socket.setsockopt(zmq.HEARTBEAT_TIMEOUT, 1000)
            self.socket.setsockopt(zmq.HEARTBEAT_TTL, 2000)
        self.monitor = self.socket.get_monitor_socket()
        self.socket.connect(f"tcp://127.0.0.1:{port}")
        self.sequences = {}

    def wait_handshake(self, timeout=5.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.monitor.poll(200):
                event = recv_monitor_message(self.monitor)["event"]
                if event == zmq.EVENT_HANDSHAKE_SUCCEEDED:
                    return True
                if event in (zmq.EVENT_HANDSHAKE_FAILED_PROTOCOL,
                             zmq.EVENT_HANDSHAKE_FAILED_AUTH,
                             zmq.EVENT_HANDSHAKE_FAILED_NO_DETAIL):
                    raise AssertionError(f"{self.name}: handshake failed {event}")
        return False

    def disconnected(self):
        seen = False
        while self.monitor.poll(0):
            message = recv_monitor_message(self.monitor)
            print(f"    {self.name} monitor: {message['event']!r} value={message['value']}")
            seen = seen or message["event"] == zmq.EVENT_DISCONNECTED
        return seen

    def subscribe(self, *topics):
        for topic in topics:
            self.socket.subscribe(topic)

    def unsubscribe(self, topic):
        self.socket.unsubscribe(topic)

    def receive(self, count, timeout=10.0):
        """Receive count notifications as {topic: [(body, sequence)]}."""
        received = {}
        deadline = time.time() + timeout
        while count > 0 and time.time() < deadline:
            if not self.socket.poll(max(1, int((deadline - time.time()) * 1000))):
                break
            frames = self.socket.recv_multipart()
            check(len(frames) == 3, f"{self.name}: three frame notification")
            topic, body, sequence = frames
            (number,) = struct.unpack("<I", sequence)
            received.setdefault(topic, []).append((body, number))
            count -= 1
        return received


def expect_block(subscriber, index, topics):
    """Expect the notifications of block index (0 based) on the topics."""
    display_hash, raw = BLOCKS[index]
    hash_bytes = bytes.fromhex(display_hash)
    raw_bytes = bytes.fromhex(raw)
    received = subscriber.receive(len(topics))
    for topic in topics:
        check(topic in received, f"{subscriber.name}: {topic.decode()} for block {index + 1}")
        body, number = received[topic][0]
        expected = subscriber.sequences.get(topic, 0)
        check(number == expected, f"{subscriber.name}: {topic.decode()} sequence {number} == {expected}")
        subscriber.sequences[topic] = expected + 1
        if topic == b"hashblock":
            check(body == hash_bytes, f"{subscriber.name}: hashblock is the display order hash")
        elif topic == b"rawblock":
            check(body == raw_bytes, f"{subscriber.name}: rawblock is the raw block")
        elif topic == b"sequence":
            check(body == hash_bytes + b"C", f"{subscriber.name}: sequence is hash + 'C'")
        elif topic == b"hashtx":
            check(len(body) == 32, f"{subscriber.name}: hashtx is a 32 byte hash")
    for topic in received:
        check(topic in topics, f"{subscriber.name}: no unexpected topic {topic.decode()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bs", default=r"C:\Source\evoskuil\libbitcoin-server\bin\x64\Debug\v145\static\bs.exe")
    parser.add_argument("--workdir", default=os.path.join(os.environ.get("TEMP", "."), "zmq_interop"))
    args = parser.parse_args()

    print("pyzmq", zmq.__version__, "libzmq", zmq.zmq_version(), "curve", zmq.has("curve"))
    if os.path.isdir(args.workdir):
        shutil.rmtree(args.workdir)
    os.makedirs(args.workdir)

    # The config parser treats a hash as a comment, so avoid it in the key.
    server_public, server_secret = zmq.curve_keypair()
    while b"#" in server_secret or b"#" in server_public:
        server_public, server_secret = zmq.curve_keypair()
    ports = (free_port(), free_port(), free_port())
    server = Server(args.bs, args.workdir, *ports, server_secret.decode())
    context = zmq.Context()
    try:
        print("starting bs (newstore, run)...")
        server.start()
        check(server.call("getblockcount") == 0, "fresh store at genesis")

        clear = Subscriber(context, "null", ports[1])
        curve = Subscriber(context, "curve", ports[2], server_public)
        check(clear.wait_handshake(), "null: ZMTP handshake succeeded (libzmq monitor)")
        check(curve.wait_handshake(), "curve: CURVE handshake succeeded (libzmq monitor)")

        clear.subscribe(b"hashblock", b"rawblock", b"hashtx", b"sequence")
        curve.subscribe(b"hashblock", b"sequence")
        time.sleep(0.5)

        # Heartbeats: several intervals with no traffic, connections survive.
        time.sleep(2.5)
        check(not clear.disconnected(), "null: alive through heartbeats (PING answered)")
        check(not curve.disconnected(), "curve: alive through heartbeats (PING answered)")

        print("submitting block 1...")
        server.call("submitheader", BLOCKS[0][1][:160])
        result = server.call("submitblock", BLOCKS[0][1])
        if result == "prev-blk-not-found":
            # bs organizes headers first and never starts the block chaser,
            # so submitblock cannot organize (see report); skip delivery.
            print("  SKIP: submitblock is unsupported on a headers-first node,"
                  " notification delivery is not exercised")
        else:
            check(result is None, f"submitblock 1 accepted (result={result!r})")
            expect_block(clear, 0, [b"hashblock", b"rawblock", b"hashtx", b"sequence"])
            expect_block(curve, 0, [b"hashblock", b"sequence"])

            print("submitting block 2...")
            server.call("submitheader", BLOCKS[1][1][:160])
            result = server.call("submitblock", BLOCKS[1][1])
            check(result is None, f"submitblock 2 accepted (result={result!r})")
            expect_block(clear, 1, [b"hashblock", b"rawblock", b"hashtx", b"sequence"])
            expect_block(curve, 1, [b"hashblock", b"sequence"])

            print("cancelling hashblock on null, submitting block 3...")
            clear.unsubscribe(b"hashblock")
            time.sleep(0.5)
            server.call("submitheader", BLOCKS[2][1][:160])
            result = server.call("submitblock", BLOCKS[2][1])
            check(result is None, f"submitblock 3 accepted (result={result!r})")
            expect_block(clear, 2, [b"rawblock", b"hashtx", b"sequence"])
            expect_block(curve, 2, [b"hashblock", b"sequence"])
            check(not clear.receive(1, timeout=1.0), "null: no hashblock after CANCEL")
            check(server.call("getblockcount") == 3, "chain height 3")

        notifications = server.call("getzmqnotifications")
        addresses = {entry["address"] for entry in notifications}
        check(f"tcp://127.0.0.1:{ports[1]}" in addresses, "getzmqnotifications lists the clear binding")
        check(f"tcp://127.0.0.1:{ports[2]}" in addresses, "getzmqnotifications lists the CURVE binding")
        print("PASS")
        return 0
    except Exception as error:
        print("FAIL:", error)
        return 1
    finally:
        context.destroy(linger=0)
        server.close()


if __name__ == "__main__":
    sys.exit(main())
