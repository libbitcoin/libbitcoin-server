.. _tut-introduction:

************
Introduction
************

Obelisk is a blockchain query infrastructure. Obelisk is based off our
experiences as developers working on Electrum. Blockchain servers are playing
an increasingly vital role in this emerging economy. Their uses range from
wallets, payment backends, blockchain analysis and protocol layers on top of
Bitcoin which rely on blockchain data.

Obelisk differs from classic backend setups in that it uses a load balancer
to distribute requests across multiple backend workers for scalability and
redundancy.

Scale up by starting more workers. Each worker maintains their own blockchain
and connects to a load balancer. If a worker is unavailable then requests are
routed to the other available workers (redundancy).

* setup
** testing
* atomic reqs

