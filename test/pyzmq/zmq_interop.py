"""libzmq (pyzmq) interoperability test for the bs bitcoind_zmq service.

Starts bs on a fresh mainnet store with no peers, binds the zmq publisher
clear (NULL) and secured (CURVE), connects one real libzmq SUB socket to
each, and serves mainnet blocks 1..3 to bs from a p2p peer (bs accepts only
blocks that it requests, so the peer announces each block on cue). Verifies:
  - NULL and CURVE handshakes complete (libzmq socket monitor events)
  - an unauthorized CURVE client is refused (zap status 400)
  - libzmq heartbeat PINGs are answered (the connection survives)
  - notification fan-out to both subscribers: hashblock, rawblock, hashtx,
    sequence (contents, byte order and per-topic sequence numbers)
  - CANCEL (unsubscribe) stops delivery of that topic only
  - getzmqnotifications lists both bindings
"""
import argparse
import hashlib
import json
import os
import random
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import traceback
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

# Bitcoin p2p (mainnet) framing for the block serving peer.
MAGIC = bytes.fromhex("f9beb4d9")
PROTOCOL = 70016
SERVICES = 9  # node_network | node_witness
GENESIS = "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"


def sha256d(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def varint(value):
    if value < 0xfd:
        return struct.pack("<B", value)
    if value <= 0xffff:
        return b"\xfd" + struct.pack("<H", value)
    return b"\xfe" + struct.pack("<I", value)


def read_varint(data, at):
    first = data[at]
    if first < 0xfd:
        return first, at + 1
    if first == 0xfd:
        return struct.unpack_from("<H", data, at + 1)[0], at + 3
    if first == 0xfe:
        return struct.unpack_from("<I", data, at + 1)[0], at + 5
    return struct.unpack_from("<Q", data, at + 1)[0], at + 9


def p2p_message(command, payload):
    name = command.encode().ljust(12, b"\x00")
    return MAGIC + name + struct.pack("<I", len(payload)) + sha256d(payload)[:4] + payload


def p2p_address(port):
    ip = bytes.fromhex("00000000000000000000ffff7f000001")
    return struct.pack("<Q", SERVICES) + ip + struct.pack(">H", port)


def p2p_version(port):
    agent = b"/zmq_interop:0.1/"
    return (struct.pack("<iQq", PROTOCOL, SERVICES, int(time.time()))
            + p2p_address(port) + p2p_address(0)
            + struct.pack("<Q", random.getrandbits(64))
            + varint(len(agent)) + agent
            + struct.pack("<i", len(BLOCKS)) + b"\x01")


class Peer(threading.Thread):
    """A p2p peer that serves BLOCKS to bs, announcing them only on release."""

    def __init__(self):
        super().__init__(daemon=True)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]
        self.released = 0
        self.announced = 0
        self.lock = threading.Lock()
        self.connection = None
        self.shaken = threading.Event()
        self.failed = None
        raws = [bytes.fromhex(raw) for _, raw in BLOCKS]
        self.headers = [raw[:80] for raw in raws]
        self.blocks = {sha256d(raw[:80]): raw for raw in raws}
        self.chain = [bytes.fromhex(GENESIS)[::-1]] + [sha256d(h) for h in self.headers]

    def release(self, count):
        """Announce blocks up to count (1 based) to bs."""
        with self.lock:
            self.released = count

    def run(self):
        try:
            self.serve()
        except Exception:
            self.failed = traceback.format_exc()

    def send(self, command, payload):
        if os.environ.get("ZMQ_INTEROP_TRACE"):
            print(f"    peer: -> {command} ({len(payload)})", flush=True)
        self.connection.sendall(p2p_message(command, payload))

    def headers_after(self, locator, limit):
        # Serve the released headers following the best known locator hash.
        start = 0
        for known in locator:
            if known in self.chain[:limit + 1]:
                start = self.chain.index(known)
                break
        return self.headers[start:limit]

    def send_headers(self, headers):
        self.send("headers", varint(len(headers)) + b"".join(h + b"\x00" for h in headers))

    def handle(self, command, payload):
        if os.environ.get("ZMQ_INTEROP_TRACE"):
            print(f"    peer: <- {command} ({len(payload)})", flush=True)
        if command == "version":
            self.send("version", p2p_version(self.port))
            self.send("verack", b"")
        elif command == "verack":
            self.shaken.set()
        elif command == "ping":
            self.send("pong", payload)
        elif command == "getheaders":
            count, at = read_varint(payload, 4)
            locator = [payload[at + 32 * i:at + 32 * (i + 1)] for i in range(count)]
            with self.lock:
                limit = self.released
            self.send_headers(self.headers_after(locator, limit))
        elif command == "getdata":
            count, at = read_varint(payload, 0)
            for i in range(count):
                item = payload[at + 36 * i:at + 36 * (i + 1)]
                block = self.blocks.get(item[4:])
                if block is not None:
                    self.send("block", block)

    def serve(self):
        # bs drops the channel when it snapshots at a checkpoint and then
        # reconnects, so connections are served until the listener closes.
        self.listener.settimeout(60)
        while True:
            self.connection, _ = self.listener.accept()
            self.shaken.clear()
            self.announced = 0
            self.serve_connection()
            self.connection.close()

    def serve_connection(self):
        self.connection.settimeout(0.2)
        buffer = b""
        while True:
            # Announce newly released headers (bs organizes and requests blocks).
            with self.lock:
                limit = self.released
            if self.shaken.is_set() and limit > self.announced:
                self.send_headers(self.headers[:limit])
                self.announced = limit
            try:
                data = self.connection.recv(65536)
            except socket.timeout:
                continue
            if not data:
                return
            buffer += data
            while len(buffer) >= 24:
                if buffer[:4] != MAGIC:
                    raise AssertionError("peer: bad magic from bs")
                (size,) = struct.unpack_from("<I", buffer, 16)
                if len(buffer) < 24 + size:
                    break
                command = buffer[4:16].rstrip(b"\x00").decode()
                payload = buffer[24:24 + size]
                buffer = buffer[24 + size:]
                self.handle(command, payload)

    def close(self):
        self.listener.close()
        if self.connection is not None:
            self.connection.close()


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
                 server_secret, client_public, peer_port):
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
                "[manual]", f"peer = 127.0.0.1:{peer_port}",
                # Blocks are requested only once the header chain is current,
                # so the currency window is disabled (zero) for 2009 blocks.
                "[node]", "delay_inbound = false", "currency_window_minutes = 0",
                "[bitcoin]", "milestone = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0",
                # A header is stored (and its block requested) on arrival only
                # when it is a checkpoint, so each served block is one.
                *[f"checkpoint = {display}:{height + 1}"
                  for height, (display, _) in enumerate(BLOCKS)],
                "[log]", "news = true", "session = true", "protocol = true",
                "[database]", f"path = {store}",
                "[bitcoind]", f"bind = 127.0.0.1:{rpc_port}", "connections = 4",
                "[bitcoind_zmq]",
                f"bind = 127.0.0.1:{clear_port}",
                f"safe = 127.0.0.1:{curve_port}",
                f"key = {server_secret}",
                f"cert = {client_public}",
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

    def __init__(self, context, name, port, server_key=None, keypair=None):
        self.name = name
        self.socket = context.socket(zmq.SUB)
        if server_key is not None:
            public, secret = keypair or zmq.curve_keypair()
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

    def wait_auth_failure(self, timeout=5.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.monitor.poll(200):
                message = recv_monitor_message(self.monitor)
                if message["event"] == zmq.EVENT_HANDSHAKE_FAILED_AUTH:
                    return message["value"]
                if message["event"] == zmq.EVENT_HANDSHAKE_SUCCEEDED:
                    return None
        return None

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
    """Expect the notifications of block index (0 based) on the topics.

    bs prunes (snapshots) the store on its first block, dropping and then
    re-establishing the peer channel, so delivery is allowed a full minute.
    """
    display_hash, raw = BLOCKS[index]
    hash_bytes = bytes.fromhex(display_hash)
    raw_bytes = bytes.fromhex(raw)
    received = subscriber.receive(len(topics), timeout=60.0)
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
    client_public, client_secret = zmq.curve_keypair()
    while b"#" in client_secret or b"#" in client_public:
        client_public, client_secret = zmq.curve_keypair()
    ports = (free_port(), free_port(), free_port())
    peer = Peer()
    peer.start()
    server = Server(args.bs, args.workdir, *ports, server_secret.decode(),
                    client_public.decode(), peer.port)
    context = zmq.Context()
    try:
        print("starting bs (newstore, run)...")
        server.start()
        check(server.call("getblockcount") == 0, "fresh store at genesis")

        clear = Subscriber(context, "null", ports[1])
        curve = Subscriber(context, "curve", ports[2], server_public, (client_public, client_secret))
        check(clear.wait_handshake(), "null: ZMTP handshake succeeded (libzmq monitor)")
        check(curve.wait_handshake(), "curve: CURVE handshake succeeded for the authorized client (libzmq monitor)")

        stranger = Subscriber(context, "stranger", ports[2], server_public)
        check(stranger.wait_auth_failure() == 400, "stranger: CURVE handshake refused with zap status 400 (libzmq monitor)")
        stranger.socket.close(linger=0)

        clear.subscribe(b"hashblock", b"rawblock", b"hashtx", b"sequence")
        curve.subscribe(b"hashblock", b"sequence")
        time.sleep(0.5)

        # Heartbeats: several intervals with no traffic, connections survive.
        time.sleep(2.5)
        check(not clear.disconnected(), "null: alive through heartbeats (PING answered)")
        check(not curve.disconnected(), "curve: alive through heartbeats (PING answered)")

        check(peer.shaken.wait(30), "peer: bs connected and completed the p2p handshake")

        print("releasing block 1...")
        peer.release(1)
        expect_block(clear, 0, [b"hashblock", b"rawblock", b"hashtx", b"sequence"])
        expect_block(curve, 0, [b"hashblock", b"sequence"])

        print("releasing block 2...")
        peer.release(2)
        expect_block(clear, 1, [b"hashblock", b"rawblock", b"hashtx", b"sequence"])
        expect_block(curve, 1, [b"hashblock", b"sequence"])

        print("cancelling hashblock on null, releasing block 3...")
        clear.unsubscribe(b"hashblock")
        time.sleep(0.5)
        peer.release(3)
        expect_block(clear, 2, [b"rawblock", b"hashtx", b"sequence"])
        expect_block(curve, 2, [b"hashblock", b"sequence"])
        check(not clear.receive(1, timeout=1.0), "null: no hashblock after CANCEL")
        check(server.call("getblockcount") == 3, "chain height 3")
        check(peer.failed is None, f"peer: served without error ({peer.failed})")


        notifications = server.call("getzmqnotifications")
        addresses = {entry["address"] for entry in notifications}
        check(f"tcp://127.0.0.1:{ports[1]}" in addresses, "getzmqnotifications lists the clear binding")
        check(f"tcp://127.0.0.1:{ports[2]}" in addresses, "getzmqnotifications lists the CURVE binding")
        print("PASS")
        return 0
    except Exception as error:
        print("FAIL:", error)
        if peer.failed is not None:
            print("peer failed:", repr(peer.failed))
        return 1
    finally:
        context.destroy(linger=0)
        server.close()
        peer.close()


if __name__ == "__main__":
    sys.exit(main())
