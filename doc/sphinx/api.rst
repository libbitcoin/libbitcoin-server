.. _tut-api:

*********
API Calls
*********

The values in brackets indicate the size of the value.

Blockchain
==========

These commands are prefixed with "blockchain.". For instance
"blockchain.fetch_transaction".

Fetch a transaction by hash from the blockchain:

================= ===========
fetch_transaction
================= ===========
Request           tx_hash(32)
Reply             ec(4) + tx
================= ===========

Fetch the last height of the latest block:

================= ==================
fetch_last_height
================= ==================
Request           
Reply             ec(4) + height(4)
================= ==================

Fetch a block header by height:

================== ==================
fetch_block_header
================== ==================
Request            height(4)
Reply              ec(4) + header(80)
================== ==================

Fetch a block header by hash:

================== ==================
fetch_block_header
================== ==================
Request            block_hash(32)
Reply              ec(4) + header(80)
================== ==================

Fetch a list of ordered transaction hashes belonging to a block.

============================== ==============================================
fetch_block_transaction_hashes
============================== ==============================================
Request                        height(4)
Reply                          ec(4) + hashes_size(4) + hashes(32 bytes each)
============================== ==============================================

============================== ==============================================
fetch_block_transaction_hashes
============================== ==============================================
Request                        block_hash(32)
Reply                          ec(4) + hashes_size(4) + hashes(32 bytes each)
============================== ==============================================

Transaction pool
================

These commands are prefixed with "transaction_pool.". For instance
"transaction_pool.validate".

======== ============================================================
validate
======== ============================================================
Request  tx
Reply    ec(4) + index_list_size(4) + index_list(index_list_size * 4)
======== ============================================================

Validates an unconfirmed transaction using the transaction pool. The index
list is a collection of unconfirmed indexes if a transaction is validated
successfully. The unconfirmed indexes are the inputs which depend on
other transactions in the pool that are unconfirmed. Clients may wish to
use it but it can be safely ignored.

In the case of `error::input_not_found` or `error::validate_inputs_failed`,
then the unconfirmed index list will be used to indicate the first problematic
input, and the index_list_size will be 1.

Protocol
========

These commands are prefixed with "protocol.". For instance
"protocol.broadcast_transaction".

===================== =====
broadcast_transaction
===================== =====
Request               tx
Reply                 ec(4)
===================== =====

Broadcasts a transaction to the currently connected nodes on the backend.
Clients are advised to find their own broadcast method and use leave this
as a backup method. An example might be running a broadcast daemon on 
localhost that is responsible for actually checking a transaction gets
delivered to the network and responding back if it wasn't accepted. Obelisk
does not do this - it simply relays the transaction straight onto connected
nodes.

Address
=======

These commands are prefixed with "address.". For instance
"address.fetch_history".

============= ===========================================================
fetch_history
============= ===========================================================
Request       address_version_byte(1) + address_hash(20) + from_height(4)
Reply         ec(4) + row_list
============= ===========================================================

`from_height` indicates the starting height of the :func:`fetch_history`
operation. This is to avoid potentially redundant re-fetching of the entire
history when repeatedly calling :func:`fetch_history`. Typically the client
before shutdown will save the current version of their history along with the
latest block height and hash. Upon restarting, the client will call
:func:`fetch_history` with the saved block height. Then the client would
query :func:`blockchain::fetch_block_header` to compare the block hash is
the same as during shutdown. If the block hash has changed then it's apt
to call :func:`fetch_history` with a `from_height` of 0.

The `row_list` size is calculated using `(reply_data.size() - 4) / row_size`.
Each row represents a credit/debit pairing. The output sending credit, and
the spend of that money (if any exists). If the output/spend is unconfirmed
then the block height is set to 0.

================ ========== ====================================================
Row Fields       Type       Description
================ ========== ====================================================
output hash      hash(32)   Transaction hash of the output.
output index     uint32(4)  Index of the output.
output height    uint32(4)  Block height containing output transaction.
value (Satoshis) uint64(8)  Satoshi value of credit.
spend hash       hash(32)   Transaction hash of input spend (0x00...00 if none).
spend index      uint32(4)  Input index.
spend height     uint32(4)  Block height containing input spend transaction.
================ ========== ====================================================

By polling the latest block height, the client can display the number of
confirmations. We don't need to worried about consistency and polling from
the same worker as confirmations are aesthetic and part of gradual network
consensus.

