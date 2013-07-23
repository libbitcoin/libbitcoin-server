import zmq
import hashlib
import struct

context = zmq.Context(1)
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:9093")
socket.setsockopt(zmq.SUBSCRIBE, "")
height = struct.unpack("<I", socket.recv())[0]
blk_hash = socket.recv().encode("hex")
blk_data = socket.recv()
print "Block (%s bytes): #%s %s" % (len(blk_data), height, blk_hash)
def hash_block(data):
    return hashlib.sha256(hashlib.sha256(data[:80]).digest()).digest()[::-1].encode("hex")
assert hash_block(blk_data) == blk_hash

