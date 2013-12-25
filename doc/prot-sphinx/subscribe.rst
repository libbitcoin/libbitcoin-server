.. _tut-subscribe:

*********
Subscribe
*********

Obelisk tries to avoid storing state as much as possible, but for listening
to changes to a Bitcoin address we need a subscription system. There are 2
strong reasons for this.

#. We want to see the changes for an address come from the same blockchain,
   rather than updates from various inconsistent blockchains.

#. Polling is inefficient for both client and server.

Subscriptions are managed through a series of request-reply messages.

#. Send `address.subscribe`, receive reply for successful subscription.
#. Receive `address.update` messages.
#. Periodicially send `address.renew` every 2 minutes.

============= ==========================================
subscribe
============= ==========================================
Request       address_version_byte(1) + address_hash(20)
Reply         ec(4)
============= ==========================================

`subscribe` begins a new subscription.

====== ============================================================================
update
====== ============================================================================
Reply  address_version_byte(1) + address_hash(20) + height(4) + block_hash(32) + tx
====== ============================================================================

The server can send `address.update` messages at any time.

======= ==========================================
renew
======= ==========================================
Request address_version_byte(1) + address_hash(20)
Reply   ec
======= ==========================================

Periodicially sent every 2 minutes to keep our subscription renewed.

A process flow for working with an address might look like:

#. Subscribe to address, start buffering any changes that come in.
#. Fetch history for address.
#. Play buffered changes on top. Receive updates.

When combined with a client that saves state on exit, the client will need to:

#. Probe the previously subscribed worker is up. Otherwise re-fetch history.
#. Fetch current history from last point (using saved block height and
   :func:`fetch_history` `from_height` parameter).
#. Check the block hash at the `from_height` hasn't changed (because of a
   reorganization). If it has changed then discard history and re-fetch
   `from_height=0`.

