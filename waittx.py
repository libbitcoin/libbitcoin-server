import zmq
import hashlib
context = zmq.Context(1)
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:9094")
socket.setsockopt(zmq.SUBSCRIBE, "")
tx_hash = socket.recv().encode("hex")
tx_data = socket.recv()
print "Transaction (%s bytes): %s" % (len(tx_data), tx_hash)
#print tx_data.encode("hex")
def hash_transaction(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()[::-1].encode("hex")
assert hash_transaction(tx_data) == tx_hash

