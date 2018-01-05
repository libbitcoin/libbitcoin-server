#!python3
import zmq
import struct
from binascii import hexlify, unhexlify

# Connect to the server.
context = zmq.Context()
socket = context.socket(zmq.DEALER)
socket.connect("tcp://mainnet.libbitcoin.net:9091")

# Make the request in parts.
socket.send(b'blockchain.fetch_transaction2', zmq.SNDMORE)
socket.send(struct.pack('I', 42), zmq.SNDMORE)
socket.send(unhexlify('60e030aba2f593caf913a7d96880ff227143bfc370d03dbaee37d582801ebd83')[::-1])

# Collect the response in parts.
response_command = socket.recv()
response_id = socket.recv()
response_body = socket.recv()

# Decode and print transaction from message payload.
print(hexlify(response_body[4:]).decode('ascii'))
