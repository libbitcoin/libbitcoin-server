.. _tut-publisher:

*********
Publisher
*********

The publisher is for discovering about new confirmed blocks and unconfirmed
(but validated) transactions.

In your config file, you should see something similar to::

    # Valid values: "enabled" / "disabled"
    # Next 2 values ignored if publisher equals "disabled"
    publisher = "disabled"
    block-publish = "tcp://*:9093"
    tx-publish = "tcp://*:9094"

Set publisher to "enabled". The worker will now make 2 PUBSUB ports available.
In the example above, blocks arrive on port 9093, and transactions on port 9094.

TODO: Obelisk should send the last update when a client connects. ZeroMQ has
this feature, but I have to investigate how to implement this [genjix].

Confirmed Blocks
================

======================= ========================================================
Fields                  Type(Size)
======================= ========================================================
height                  uint32(4)
hash                    sha256 hash(32)
header                  block header(80)
number of transactions  uint32(4)
transaction hashes      sha256 hash(variable number of fields of 32 bytes)
======================= ========================================================

The hash can be used as a checksum by hashing the block_header. The
transactions can be verified by checking the merkle root within the block
header.

The client should be buffering unconfirmed transactions, and only requesting
the transactions which they need but don't have in their buffer from the server.

Unconfirmed Validated Transactions
==================================

=========== ===============
Fields      Type(Size)
=========== ===============
hash        sha256 hash(32)
transaction data
=========== ===============

Currently Obelisk doesn't send an update when you first connect, but this will
be changed. You should be able to handle being giving the last update (for
both blocks and transactions) when you first connect.

