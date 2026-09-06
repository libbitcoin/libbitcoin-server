"""libzmq (pyzmq) interoperability test for the bs bitcoind_zmq service.

Starts bs on a fresh mainnet store with no peers, binds the zmq publisher
clear (NULL) and secured (CURVE), connects one real libzmq SUB socket to
each. Verifies:
  - NULL and CURVE handshakes complete (libzmq socket monitor events)
  - an unauthorized CURVE client is refused (zap status 400)
  - libzmq heartbeat PINGs are answered (the connection survives)
  - subscriptions are accepted and getzmqnotifications lists both bindings
Notification delivery is not exercised: bs has no mempool, so there is no
block template and submitblock is disabled (a node accepts only blocks that
it requests).
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
                 server_secret, client_public):
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
    server = Server(args.bs, args.workdir, *ports, server_secret.decode(),
                    client_public.decode())
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
