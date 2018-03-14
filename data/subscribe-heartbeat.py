#!python3
import sys
import zmq
import struct

# Connect to the public heartbeat service.
url = 'tcp://mainnet.libbitcoin.net:9092'
if len(sys.argv) > 1:
    url = sys.argv[1]

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect(url)
socket.setsockopt(zmq.SUBSCRIBE, '')
    
while True:
    # Collect the response in parts.
    sequence = struct.unpack('<H', socket.recv())[0]
    height = struct.unpack('<Q', socket.recv())[0]

    print '[Seq:{}] Heartbeat received with height {}'.format(
        sequence, height)
