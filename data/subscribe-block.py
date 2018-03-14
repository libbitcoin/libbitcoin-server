#!python3
import sys
import zmq
import struct
from binascii import hexlify, unhexlify

# Connect to the public block service.
url = 'tcp://mainnet.libbitcoin.net:9093'
if len(sys.argv) > 1:
    url = sys.argv[1]

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect(url)
socket.setsockopt(zmq.SUBSCRIBE, '')
    
while True:
    # Collect the response in parts.
    sequence = struct.unpack('<H', socket.recv())[0]
    height = struct.unpack('<I', socket.recv())[0]
    block_data = hexlify(socket.recv()).decode('ascii')

    print '[Seq:{}] Block {} received {}'.format(
        sequence, height, block_data)
