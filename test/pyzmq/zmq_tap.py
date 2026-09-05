"""Forward one tcp connection to bs, dumping ZMTP frames in both directions.

Usage: zmq_tap.py <listen_port> <server_port> [seconds]
Then connect a libzmq client to tcp://127.0.0.1:<listen_port>.
"""
import socket
import sys
import threading
import time

START = time.time()


def stamp():
    return f"{time.time() - START:7.3f}"


def dump(direction, data):
    print(f"{stamp()} {direction} {len(data):4d} {data[:64].hex()}{'...' if len(data) > 64 else ''}", flush=True)


def parse_frames(direction, buffer, greeted):
    """Split a tcp segment into greeting/frames where possible (best effort)."""
    offset = 0
    if not greeted[0]:
        if len(buffer) < 64:
            return buffer
        mechanism = buffer[12:32].rstrip(bytes([0]))
        print(f"{stamp()} {direction} greeting mechanism={mechanism!r} as_server={buffer[32]}", flush=True)
        offset = 64
        greeted[0] = True
    while offset < len(buffer):
        flags = buffer[offset]
        if flags & 0x02:
            if offset + 9 > len(buffer):
                break
            length = int.from_bytes(buffer[offset + 1:offset + 9], "big")
            head = 9
        else:
            if offset + 2 > len(buffer):
                break
            length = buffer[offset + 1]
            head = 2
        body = buffer[offset + head:offset + head + length]
        if len(body) < length:
            print(f"{stamp()} {direction} partial frame flags={flags:#04x} length={length} have={len(body)}", flush=True)
            break
        name = ""
        if flags & 0x04 and body:
            name = body[1:1 + body[0]].decode("ascii", "replace")
        print(f"{stamp()} {direction} frame flags={flags:#04x} length={length} {name} {body.hex()}", flush=True)
        offset += head + length
    return buffer[offset:]


def pump(source, sink, direction, stop):
    buffer = b""
    greeted = [False]
    try:
        while not stop.is_set():
            data = source.recv(65536)
            if not data:
                print(f"{stamp()} {direction} closed by sender", flush=True)
                break
            buffer = parse_frames(direction, buffer + data, greeted)
            sink.sendall(data)
    except OSError as error:
        print(f"{stamp()} {direction} error {error}", flush=True)
    finally:
        stop.set()
        try:
            sink.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass


def main():
    listen_port, server_port = int(sys.argv[1]), int(sys.argv[2])
    seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 15.0
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", listen_port))
    listener.listen(1)
    listener.settimeout(seconds)
    print(f"{stamp()} listening on {listen_port} -> {server_port}", flush=True)
    client, _ = listener.accept()
    server = socket.create_connection(("127.0.0.1", server_port))
    stop = threading.Event()
    threads = [
        threading.Thread(target=pump, args=(client, server, "C->S", stop), daemon=True),
        threading.Thread(target=pump, args=(server, client, "S->C", stop), daemon=True),
    ]
    for thread in threads:
        thread.start()
    stop.wait(seconds)
    stop.set()
    for sock in (client, server):
        try:
            sock.close()
        except OSError:
            pass
    print(f"{stamp()} tap done", flush=True)


if __name__ == "__main__":
    main()
