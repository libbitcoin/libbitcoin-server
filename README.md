[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-server.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-server)

[![Coverage Status](https://coveralls.io/repos/libbitcoin/libbitcoin-server/badge.svg)](https://coveralls.io/r/libbitcoin/libbitcoin-server)

# Libbitcoin Server

*Bitcoin full node and query server*

[Documentation](https://github.com/libbitcoin/libbitcoin-server/wiki) is available on the wiki.

[Downloads](https://github.com/libbitcoin/libbitcoin-server/wiki/Download-BS) are available for Linux, Macintosh and Windows.

**License Overview**

All files in this repository fall under the license specified in [COPYING](https://github.com/libbitcoin/libbitcoin-server/blob/master/COPYING). The project is licensed as [AGPL with a lesser clause](https://wiki.unsystem.net/en/index.php/Libbitcoin/License). It may be used within a proprietary project, but the core library and any changes to it must be published on-line. Source code for this library must always remain free for everybody to access.

**About Libbitcoin**

The libbitcoin toolkit is a set of cross platform C++ libraries for building bitcoin applications. The toolkit consists of several libraries, most of which depend on the foundational [libbitcoin](https://github.com/libbitcoin/libbitcoin) library. Each library's repository can be cloned and built using common [Automake](http://www.gnu.org/software/automake) instructions.

**About Libbitcoin Server**

A full Bitcoin peer-to-peer node, Libbitcoin Server is also a high performance blockchain query server. It can be built as a single portable executable for Linux, macOS or Windows and is available for download as a signed single executable for each. It is trivial to deploy, just run the single process and allow it about two days to synchronize the Bitcoin blockchain.

Libbitcoin Server exposes a custom query TCP API built based on the [ZeroMQ](http://zeromq.org) networking stack. It supports server, and optionally client, identity certificates and wire encryption via [CurveZMQ](http://curvezmq.org) and the [Sodium](http://libsodium.org) cryptographic library.

The API is backward compatible with its predecessor [Obelisk](https://github.com/spesmilo/obelisk) and supports simple and advanced scenarios, including stealth payment queries. The [libbitcoin-client](https://github.com/libbitcoin/libbitcoin-client) library provides a calling API for building client applications. The server is complimented by [libbitcoin-explorer (BX)](https://github.com/libbitcoin/libbitcoin-explorer), the Bitcoin command line tool and successor to [SX](https://github.com/spesmilo/sx).

## Requirements.

Growing file storage (SSD preferred), with swap enabled and at least 4Gb RAM (8Gb preferred).  See [storage space](https://github.com/libbitcoin/libbitcoin-server/wiki/Hardware#storage-space) for the estimated growth rate.

## Installation

Libbitcoin Server can be built from sources or downloaded as a signed portable [single file executable](https://github.com/libbitcoin/libbitcoin-server/wiki/Download-BS).

The master branch is a staging area for the next major release and should be used only by libbitcoin developers. The current release branch is version3. Detailed installation instructions are provided below.

* [Debian/Ubuntu](#debianubuntu)
* [Macintosh](#macintosh)
* [Windows](#windows)

### Autotools (advanced users)

On Linux and macOS Libbitcoin Server is built using Autotools as follows.
```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install # optional
$ sudo ldconfig     # optional
```

### Debian/Ubuntu

Libbitcoin requires a C++11 compiler, currently minimum [GCC 4.8.0](https://gcc.gnu.org/projects/cxx0x.html) or Clang based on [LLVM 3.5](http://llvm.org/releases/3.5.0/docs/ReleaseNotes.html).

To see your GCC version:
```sh
$ g++ --version
```
```
g++ (Ubuntu 4.8.2-19ubuntu1) 4.8.2
Copyright (C) 2013 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
If necessary, upgrade your compiler as follows:
```sh
$ sudo apt-get install g++-4.8
$ sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 50
$ sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 50
```
Next install the [build system](http://wikipedia.org/wiki/GNU_build_system):
```sh
$ sudo apt-get install build-essential autoconf automake libtool pkg-config
```
Next download the [install script](https://github.com/libbitcoin/libbitcoin-server/blob/version3/install.sh) and enable execution:
```sh
$ wget https://raw.githubusercontent.com/libbitcoin/libbitcoin-server/version3/install.sh
$ chmod +x install.sh
```
Finally install Libbitcoin Server with recommended [build options](#build-notes-for-linux--macos):
```sh
$ ./install.sh --prefix=/home/me/myprefix --build-boost --disable-shared
```
Libbitcoin Server is now installed in `/home/me/myprefix` and can be invoked as `$ bs`.

### Macintosh

The macOS installation differs from Linux in the installation of the compiler and packaged dependencies. Libbitcoin Server supports both [Homebrew](http://brew.sh) and [MacPorts](https://www.macports.org) package managers. Both require Apple's [Xcode](https://developer.apple.com/xcode) command line tools. Neither requires Xcode as the tools may be installed independently.

Libbitcoin Server compiles with Clang on macOS and requires C++11 support. Installation has been verified using Clang based on [LLVM 3.5](http://llvm.org/releases/3.5.0/docs/ReleaseNotes.html). This version or newer should be installed as part of the Xcode command line tools.

To see your Clang/LLVM  version:
```sh
$ clang++ --version
```
You may encounter a prompt to install the Xcode command line developer tools, in which case accept the prompt.
```
Apple LLVM version 6.0 (clang-600.0.54) (based on LLVM 3.5svn)
Target: x86_64-apple-darwin14.0.0
Thread model: posix
```
If required update your version of the command line tools as follows:
```sh
$ xcode-select --install
```

#### Using Homebrew

First install [Homebrew](https://brew.sh).
```sh
$ ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
Next install the [build system](http://wikipedia.org/wiki/GNU_build_system) and [wget](http://www.gnu.org/software/wget):
```sh
$ brew install autoconf automake libtool pkgconfig wget
```
Next download the [install script](https://github.com/libbitcoin/libbitcoin-server/blob/version3/install.sh) and enable execution:
```sh
$ wget https://raw.githubusercontent.com/libbitcoin/libbitcoin-server/version3/install.sh
$ chmod +x install.sh
```
Finally install Libbitcoin Server with recommended [build options](#build-notes-for-linux--macos):
```sh
$ ./install.sh --prefix=/home/me/myprefix --build-boost --disable-shared
```
Libbitcoin Server is now installed in `/home/me/myprefix` and can be invoked as `$ bs`.

##### Installing from Formula

Instead of building, libbitcoin-server can be installed from a formula:
```sh
$ brew install libbitcoin-server
```
or
```sh
$ brew install bs
```

#### Using MacPorts

First install [MacPorts](https://www.macports.org/install.php).

Next install the [build system](http://wikipedia.org/wiki/GNU_build_system) and [wget](http://www.gnu.org/software/wget):
```sh
$ sudo port install autoconf automake libtool pkgconfig wget
```
Next download the [install script](https://github.com/libbitcoin/libbitcoin-server/blob/version3/install.sh) and enable execution:
```sh
$ wget https://raw.githubusercontent.com/libbitcoin/libbitcoin-server/version3/install.sh
$ chmod +x install.sh
```
Finally install Libbitcoin Server with recommended [build options](#build-notes-for-linux--macos):
```sh
$ ./install.sh --prefix=/home/me/myprefix --build-boost --disable-shared
```
Libbitcoin Server is now installed in `/home/me/myprefix` and can be invoked as `$ bs`.

### Build Notes for Linux / macOS

Any set of `./configure` options can be passed via the build script, several examples follow.

Building for minimum size and with debug symbols stripped:
```sh
$ ./install.sh CXXFLAGS="-Os -s" --prefix=/home/me/myprefix
```

> The `-s` option is not supported by the Clang compiler. Instead use the command `$ strip bs` after the build.

Building without NDEBUG (i.e. with debug assertions) defined:
```sh
$ ./install.sh --disable-ndebug --build-boost --disable-shared --prefix=/home/me/myprefix
```
Building without building tests:
```sh
$ ./install.sh --without-tests --build-boost --disable-shared --prefix=/home/me/myprefix
```
Building from a specified directory, such as `/home/me/mybuild`:
```sh
$ ./install.sh --build-dir=/home/me/mybuild --build-boost --disable-shared --prefix=/home/me/myprefix
```
Building into a directory other than `/usr/local`, such as `/home/me/myprefix`:
```sh
$ ./install.sh --prefix=/home/me/myprefix
```
Building and linking with a private copy of the Boost dependency:
```sh
$ ./install.sh --build-boost --prefix=/home/me/myprefix
```
Building and linking with a private copy of the ZeroMQ dependency:
```sh
$ ./install.sh --build-zmq --prefix=/home/me/myprefix
```
Building a statically-linked executable:
```sh
$ ./install.sh --disable-shared --build-boost --build-zmq --prefix=/home/me/myprefix
```
Building a small statically-linked executable most quickly:
```sh
$ ./install.sh CXXFLAGS="-Os -s" --without-tests --disable-shared --build-boost --build-zmq --prefix=/home/me/myprefix
```
Building with bash-completion support:

> If your target system does not have it pre-installed you must first install the [bash-completion](http://bash-completion.alioth.debian.org) package. Packages are available for common package managers, including apt-get, homebrew and macports.

```sh
$ ./install.sh --with-bash-completion-dir
```

### Windows

Visual Studio solutions are maintained for all libbitcoin libraries and dependencies. See the [libbitcoin](https://github.com/libbitcoin/libbitcoin/blob/master/README.md#windows) repository general information about building the Visual Studio solutions. To build Libbitcoin Server you must also download and build its **libbitcoin dependencies**, as these are not yet packaged.

Build these solutions in order:

1. [libbitcoin/libbitcoin](https://github.com/libbitcoin/libbitcoin)
2. [libbitcoin/libbitcoin-consensus](https://github.com/libbitcoin/libbitcoin-consensus)
3. [libbitcoin/libbitcoin-database](https://github.com/libbitcoin/libbitcoin-database)
4. [libbitcoin/libbitcoin-blockchain](https://github.com/libbitcoin/libbitcoin-blockchain)
5. [libbitcoin/libbitcoin-network](https://github.com/libbitcoin/libbitcoin-network)
6. [libbitcoin/libbitcoin-node](https://github.com/libbitcoin/libbitcoin-node)
7. [libbitcoin/libbitcoin-protocol](https://github.com/libbitcoin/libbitcoin-protocol)
8. [libbitcoin/libbitcoin-server](https://github.com/libbitcoin/libbitcoin-server)

