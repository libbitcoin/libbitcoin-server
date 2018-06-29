#!python3
import sys
import zmq
import struct
from binascii import hexlify, unhexlify

# Connect to the public transaction service.
url = 'tcp://mainnet.libbitcoin.net:9094'
if len(sys.argv) > 1:
    url = sys.argv[1]

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect(url)
socket.setsockopt(zmq.SUBSCRIBE, b'')
socket.setsockopt(zmq.TCP_KEEPALIVE, 1)

while True:
    # Collect the response in parts.
    sequence = struct.unpack('<H', socket.recv())[0]
    transaction = hexlify(socket.recv()).decode('ascii')

    print('[Seq:{}] Transaction received: {}'.format(sequence, transaction))
