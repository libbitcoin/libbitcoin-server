from websocket import create_connection
from popular_addrs import popular_addrs
import json

ws = create_connection("ws://ws.blockchain.info/inv")
ws.send('{"op":"unconfirmed_sub"}')

addrs = []
while len(addrs) < 20:
    result = ws.recv()
    tx = json.loads(result)["x"]
    for input in tx["inputs"]:
        addr = input["prev_out"]["addr"]
        if addr in popular_addrs:
            continue
        addrs.append(addr)
    for output in tx["out"]:
        addr = output["addr"]
        if addr in popular_addrs:
            continue
        addrs.append(addr)
ws.close()
for addr in addrs:
    print addr

