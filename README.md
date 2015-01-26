[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-node.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-server)

# Libbitcoin Server

*Bitcoin full node and query server*

Note that you need g++ 4.8 or higher. For this reason Ubuntu 12.04 and older are not supported. Make sure you have installed [libbitcoin-node](https://github.com/libbitcoin/libbitcoin-node) beforehand according to its build instructions.

```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
$ sudo ldconfig
```

libbitcoin-server is now installed in `/usr/local/`.