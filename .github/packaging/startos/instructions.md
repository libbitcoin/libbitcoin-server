# Bitcoin Server

Bitcoin Server (`bs`) is a full node built on the Libbitcoin libraries. It
starts on its own, creates its store in the service's volume, and syncs the
chain from peers. There is nothing to configure before it starts.

## Opening the console

1. Open Bitcoin Server's **Dashboard** tab.
2. Click the **Web UI** interface to open the management console.
3. Click the **Block Explorer** interface to open the block explorer.

The console reads chain state from the block explorer's interface. Set both
addresses in the console's endpoint settings.

## Peers

The **Peer** interface accepts incoming connections from the bitcoin network.

## Documentation

- [Configuration](https://libbitcoin.org/configuration/index.html)
- [Source](https://github.com/libbitcoin/libbitcoin-server)
