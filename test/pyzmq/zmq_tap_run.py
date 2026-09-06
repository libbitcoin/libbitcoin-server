"""Start bs, tap a CURVE SUB session with heartbeats, dump the frames."""
import os
import shutil
import subprocess
import sys
import threading
import time

import zmq

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from zmq_interop import Server, Subscriber, free_port  # noqa: E402


def main():
    workdir = os.path.join(os.environ.get("TEMP", "."), "zmq_tap")
    if os.path.isdir(workdir):
        shutil.rmtree(workdir)
    os.makedirs(workdir)

    server_public, server_secret = zmq.curve_keypair()
    while b"#" in server_secret or b"#" in server_public:
        server_public, server_secret = zmq.curve_keypair()

    rpc, clear, curve, tap = free_port(), free_port(), free_port(), free_port()
    bs = r"C:\Source\evoskuil\libbitcoin-server\bin\x64\Debug\v145\static\bs.exe"
    server = Server(bs, workdir, rpc, clear, curve, server_secret.decode())
    context = zmq.Context()
    try:
        server.start()
        tap_process = subprocess.Popen(
            [sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "zmq_tap.py"),
             str(tap), str(curve), "4"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        time.sleep(0.5)
        subscriber = Subscriber(context, "curve", tap, server_public)
        print("handshake:", subscriber.wait_handshake())
        subscriber.subscribe(b"hashblock")
        time.sleep(3.0)
        print("disconnected:", subscriber.disconnected())
        output, _ = tap_process.communicate(timeout=10)
        print(output.decode("utf-8", "replace"))
    finally:
        context.destroy(linger=0)
        server.close()


if __name__ == "__main__":
    main()
