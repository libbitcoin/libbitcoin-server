[![Build Status](https://github.com/libbitcoin/libbitcoin-server/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/libbitcoin/libbitcoin-server/actions/workflows/ci.yml?branch=master)
[![Coverage Status](https://coveralls.io/repos/github/libbitcoin/libbitcoin-server/badge.svg?branch=master)](https://coveralls.io/github/libbitcoin/libbitcoin-server?branch=master)

# libbitcoin-server

*The high performance Bitcoin full node server based on [libbitcoin-node](https://github.com/libbitcoin/libbitcoin-node), [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system), [libbitcoin-database](https://github.com/libbitcoin/libbitcoin-database), and [libbitcoin-network](https://github.com/libbitcoin/libbitcoin-network).*

<div align="center">

<a href="/docs/images/libbitcoin-components-server.svg">
  <img src="/docs/images/libbitcoin-components-server.svg" alt="libbitcoin components" width="500" />
</a>

</div>

## tl;dr
This is the **server component of libbitcoin v4**. It integrates the basic components and offers a **high performance Bitcoin full node** with a comprehensive set of client-server interfaces like **bitcoind, electrum and stratum** as well as an intergrated **block explorer**. It makes heavy use of the available hardware, internet connection and availabe RAM and CPU so you can sync the Bitcoin blockchain from genesis, today and in the future.

If you follow this README you will be able to build, run and monitor your own libbitcoin server (bs). Libbitcoin is a multi platform software that works on Linux, Windows and OSX (Intel and ARM).

Now, If you want to see how fast your setup can sync the Bitcoin Blockchain, read on and get your libbitcoin server running today. It's pretty easy actually.

**License Overview**

All files in this repository fall under the license specified in [COPYING](COPYING). The project is licensed as [AGPL with a lesser clause](https://wiki.unsystem.net/en/index.php/Libbitcoin/License). It may be used within a proprietary project, but the core library and any changes to it must be published online. Source code for this library must always remain free for everybody to access.

**About libbitcoin**

The libbitcoin toolkit is a set of cross platform C++ libraries for building Bitcoin applications. The toolkit consists of several libraries, most of which depend on the foundational [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system) library. Each library's repository can be cloned and built separately. There are no packages yet in distribution, however each library includes an installation script (described below) which is regularly verified via [github actions](https://github.com/features/actions).

## Contents
- [libbitcoin-server](#libbitcoin-server)
  - [tl;dr](#tldr)
  - [Contents](#contents)
  - [Installation](#installation)
    - [Linux](#linux)
      - [Requirements](#requirements)
      - [Build](#build)
      - [CPU Extensions](#cpu-extensions)
    - [macOS](#macos)
    - [Windows](#windows)
  - [Running bs](#running-bs)
    - [Startup Process](#startup-process)
    - [Logging](#logging)
    - [Using the console](#using-the-console)
    - [Performance](#performance)
    - [Memory Management](#memory-management)
      - [Kernel VM Tuning Parameters](#kernel-vm-tuning-parameters)

## Installation
Depending on what operating system you use, the build process will be different. Detailed instructions are provided below.

- [Linux (Ubuntu)](#linux)
- [macOS](#macos)
- [Windows](#windows)

### Linux

Linux users build libbitcoin node with the provided installation script `install.sh` that uses Autotools and pkg-config to first build the needed dependencies (boost, libpsec256) and then fetches the latest libbitcoin projects (libbitcoin-system, libbitcoin-database, libbitcoin-network, libbitcoin-node) from github and builds the stack bottom up to the libbitcoin-server executable - `bs`.

You can issue the following command to check for further parameterization of the install script:

```bash
$ install.sh --help
```

which brings up the following options:

```
Usage: ./install.sh [OPTION]...
Manage the installation of libbitcoin-server.
Script options:
  --with-icu               Compile with International Components for Unicode.
                             Since the addition of BIP-39 and later BIP-38 
                             support, libbitcoin conditionally incorporates ICU 
                             to provide BIP-38 and BIP-39 passphrase 
                             normalization features. Currently 
                             libbitcoin-explorer is the only other library that 
                             accesses this feature, so if you do not intend to 
                             use passphrase normalization this dependency can 
                             be avoided.
  --build-icu              Build ICU libraries.
  --build-boost            Build Boost libraries.
  --build-secp256k1        Build libsecp256k1 libraries.
  --build-dir=<path>       Location of downloaded and intermediate files.
  --prefix=<absolute-path> Library install location (defaults to /usr/local).
  --disable-shared         Disables shared library builds.
  --disable-static         Disables static library builds.
  --help, -h               Display usage, overriding script execution.

All unrecognized options provided shall be passed as configuration options for 
all dependencies.
```

In order to successfully execute `install.sh` a few requirements need to be met. This walkthrough is based on a 'current' installation of the following components. (That doesn't necessarily mean these are the minimum requirements though).

#### Requirements
* Base OS: Ubuntu 24.04.3 LTS (Noble Numbat)
* C++13 compiler (gcc version 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04))
* [Autoconf](https://www.gnu.org/software/autoconf/)
* [Automake](https://www.gnu.org/software/automake/)
* [libtool](https://www.gnu.org/software/libtool/)
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
* [git](https://git-scm.com/)
* [curl](https://www.gnu.org/software/curl/)

The corresponding packages can be installed with the following command:

```bash
$ sudo apt install build-essential curl git autoconf automake pkg-config libtool
```

#### Build
Create a new directory (e.g. `/home/user/libbitcoin`), then use git to fetch the latest repository from github by issuing the following command from within this directory:

```bash
git clone https://github.com/libbitcoin/libbitcoin-server
```

Enter the project directory `cd libbitcoin-server` and start the build with the following command:

```bash
$ ./install.sh --prefix=/home/user/libbitcoin/build/release_static/ --build-secp256k1 --build-boost --disable-shared
```

This will build the libbitcoin-server executable as a static release with no [CPU extensions](#cpu-extensions) built in. Use only absolute path names for prefix dir. A successful build will create the following directory/file structure:

```
~/bitcoin/libbitcoin/
~/bitcoin/libbitcoin/build/
~/bitcoin/libbitcoin/build/release_static/
~/bitcoin/libbitcoin/build/release_static/bin/
~/bitcoin/libbitcoin/build/release_static/etc/
~/bitcoin/libbitcoin/build/release_static/include/
~/bitcoin/libbitcoin/build/release_static/lib/
~/bitcoin/libbitcoin/build/release_static/share/
~/bitcoin/libbitcoin/build/release_static/share/doc/
~/bitcoin/libbitcoin/build/release_static/share/doc/libbitcoin-database/
~/bitcoin/libbitcoin/build/release_static/share/doc/libbitcoin-network/
~/bitcoin/libbitcoin/build/release_static/share/doc/libbitcoin-node/
~/bitcoin/libbitcoin/build/release_static/share/doc/libbitcoin-server/
~/bitcoin/libbitcoin/build/release_static/share/doc/libbitcoin-system/
~/bitcoin/libbitcoin/libbitcoin-node
```
Now enter the bin directory and [fire up](#running-libbitcoin) your libbitcoin server.

#### CPU Extensions
CPU Extensions are specific optimizations your CPU might or might not have. libbitcoin(-server) can be built with support for the following CPU extensions if your architecture supports them:

* [SHA-NI](https://en.wikipedia.org/wiki/SHA_instruction_set)
* [SSE4.1](https://en.wikipedia.org/wiki/SSE4)
* [AVX2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions)
* [AVX512](https://en.wikipedia.org/wiki/AVX-512)

To build libbitcoin-server with these extensions use the `--enable-feature` parameters like:

- --enable-shani
- --enable-avx2
- --enable-sse41
- --enable-avx512

This command will create a static release build with all supported CPU extensions (if supported by the system). Building libbitcoin with unsupported CPU extensions might work but will lead to crashes at runtime.

```bash
$ CFLAGS="-O3" CXXFLAGS="-O3" ./install.sh --prefix=/home/user/libbitcoin/build/release_static/ --build-dir=/home/user/libbitcoin/build/temp --build-secp256k1 --build-boost --disable-shared --enable-ndebug --enable-shani --enable-avx2 --enable-sse41 --enable-avx512 --enable-isystem
```

You can check if the optimizations have been built in your binary with the following command:

```bash
$ ./bs --hardware
```

which will show your CPU architecture as well as the built in status of the supported optimizations:

```
Hardware configuration...
arm..... platform:0.
intel... platform:1.
avx512.. platform:1 compiled:1.
avx2.... platform:1 compiled:1.
sse41... platform:1 compiled:1.
shani... platform:1 compiled:1.
```

### macOS
TBD

### Windows
TBD

## Running bs
Let's see what options we have for the server:

```bash
$ ./bs --help
```

The response should look like this:

```
Usage: bs [-abdfhiklnrstvw] [--config value]                             

Info: Runs a full bitcoin server.                                          

Options (named):

-a [--slabs]         Scan and display store slab measures.               
-b [--backup]        Backup to a snapshot (can also do live).            
-c [--config]        Specify path to a configuration settings file.      
-d [--hardware]      Display hardware compatibility.                     
-f [--flags]         Scan and display all flag transitions.              
-h [--help]          Display command line options.                       
-i [--information]   Scan and display store information.                 
-k [--buckets]       Scan and display all bucket densities.              
-l [--collisions]    Scan and display hashmap collision stats (may exceed
                     RAM and result in SIGKILL).                         
-n [--newstore]      Create new store in configured directory.           
-r [--restore]       Restore from most recent snapshot.                  
-s [--settings]      Display all configuration settings.                 
-t [--test]          Run built-in read test and display.                 
-v [--version]       Display version information.                        
-w [--write]         Run built-in write test and display.
```

Did you check if all supported [CPU Extensions](#cpu-extensions) are built in (`./bn --hardware`)?

### Startup Process

Starting the server without passing any parameters like

```
$ ./bs
```

will do the following:
- If there is no data store at `./bitcoin/`, the node will create one. The location can be changed in the [config](/docs/configuration.md) file (`bs.cfg`).
- Start multiple threads to setup peer connections, to download and to process blocks in default mode (milestone sync). See [Theory of operation](/docs/theory-of-operation.md#) for more information about operating modes.
- Expose an interactive console to do basic communication with the node -> [Using the console](#using-the-console)
- Write various logfiles -> [Logging](#logging)

Furthermore the standard parameters will perform a **milestone sync** with the Bitcoin Blockchain, as opposed to a **full validation sync**.

Although this works in principle, you may want to do at least a few basic settings. You do this by passing a [config file](/docs/configuration.md) to the node:

```bash
$ ./bs --config ./bs.cfg
```

The node will now expect to find a valid [config file](/docs/configuration.md) in the current folder.

Starting a **full validation sync** on the node requires to set the latest milestone block back to genesis. This is done by adding the following lines to the config file:

```
[bitcoin]
checkpoint   = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
milestone = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
```

Further information on how to adjust the config file as well as example configurations can be found [here](/docs/configuration.md).

### Logging
libbitcoin-server creates the following files at start:

* events.log
* bs_begin.log (rotated to bs_end.log)
* hosts.cache

The events.log file records all information about the sync process, providing the current status and speed of the sync. Read [Performance](#performance) to learn how to check your sync speed through the logs.

### Using the console
Pressing **`m` + Enter** in the terminal will bring up the interactive console. This interface is mainly used for low level logging and configuration and it looks like this:

```
2025-10-17T23:47:58Z.0 Toggle: [v]erbose
2025-10-17T23:47:58Z.0 Toggle: [o]bjects
2025-10-17T23:47:58Z.0 Toggle: [q]uitting
2025-10-17T23:47:58Z.0 Toggle: [f]ault
2025-10-17T23:47:58Z.0 Toggle: [r]emote
2025-10-17T23:47:58Z.0 Toggle: [x]proxy
2025-10-17T23:47:58Z.0 Toggle: [p]rotocol
2025-10-17T23:47:58Z.0 Toggle: [s]ession
2025-10-17T23:47:58Z.0 Toggle: [n]ews
2025-10-17T23:47:58Z.0 Toggle: [a]pplication
2025-10-17T23:47:58Z.0 Option: [z]eroize disk full error
2025-10-17T23:47:58Z.0 Option: [w]ork distribution
2025-10-17T23:47:58Z.0 Option: [t]est built-in case
2025-10-17T23:47:58Z.0 Option: [m]enu of options and toggles
2025-10-17T23:47:58Z.0 Option: [i]nfo about store
2025-10-17T23:47:58Z.0 Option: [h]old network communication
2025-10-17T23:47:58Z.0 Option: [g]o network communication
2025-10-17T23:47:58Z.0 Option: [e]rrors in store
2025-10-17T23:47:58Z.0 Option: [c]lose the node
2025-10-17T23:47:58Z.0 Option: [b]ackup the store
```
Further information about how to use the interactive console can be found [here](/docs/tbd.md)

### Performance
You can check your server's performance using the log files. To do this, check the events.log file as follows. Let's check for the **organization** of Block **700,000**. The command

```bash
$ cat ./events.log | grep 'block_organized..... 700000'
```

will give you something like this, with the number on the right representing the seconds since the server started until block 700k was organized:

```
block_organized..... 700000 11053
```

In this case it took the node 11053 seconds (or 184 minutes) to sync up to Block 700k.

As the node aims to sync the Blockchain as fast as possible, we assume that if the computer is powerful enough, the limiting factor for syncing the chain is your internet connection - you simply can't sync faster than the time needed to download the Blockchain.

The following diagram shows the performance data extracted from various IBDs (bs milestone and full validation, Core 30 Standard and AssumeValid) on a 64 Core, 256MB RAM machine running Ubuntu.

<details>
<summary><strong>brunella - Dell Precision Tower 7810</strong></summary>

- Dual Intel® Xeon® CPU E5-2683 v4 @ 2.10 GHz (32 Kerne / 64 Threads)
- **256 GB** DDR4 ECC Registered (RDIMM)
- 4 TB SSD Samsung 990 PRO (PCIe Gen4)
- 2 GBit/s Fiber Internet

</details>

<div align="center">

<a href="/docs/images/brunella_compare.png">
  <img src="/docs/images/brunella_compare.png" alt="brunella performance compare" width="400" />
</a>

</div>

Check [this](/docs/tbd.md) to learn more about how performance profiling and optimization works.

### Memory Management

In order to improve the performance on your Linux system, you probably have to alter the kernel parameters that determine the memory management of the system ([Memory Paging](https://en.wikipedia.org/wiki/Memory_paging)).

#### Kernel VM Tuning Parameters

These sysctl parameters optimize Linux VM dirty page writeback for high I/O workloads (e.g., databases). Set via `/etc/sysctl.conf` or `sudo sysctl`. This table shows the stock values of the current Ubuntu LTS.

| Parameter                      | Value  | Description                                                       |
|--------------------------------|--------|-------------------------------------------------------------------|
| `vm.dirty_background_ratio`    | `10`   | Initiates background writeback at 10% dirty RAM (lowers latency). |
| `vm.dirty_ratio`               | `20`   | Blocks apps at 20% dirty RAM (prevents OOM).                      |
| `vm.dirty_expire_centisecs`    | `3000` | Marks pages "old" after 30s (1/100s units; triggers writeback).   |
| `vm.dirty_writeback_centisecs` | `500`  | Wakes kworker every 5s to flush old pages.                        |

Check your actual kernel parameters by issuing the following command:

```bash
$ sysctl vm.dirty_background_ratio vm.dirty_ratio vm.dirty_expire_centisecs vm.dirty_writeback_centisecs
```

The following parameters have been identified to be the most effective for the most computers we use for testing. They basically allow the server process to use as much RAM as possible (90%) and flush the dirty pages after 2 minutes:

* vm.dirty_background_ratio = 90
* vm.dirty_ratio = 90
* vm.dirty_expire_centisecs = 12000
* vm.dirty_writeback_centisecs = 12000

Set them with the following commands:

```bash
$ sudo sysctl vm.dirty_background_ratio=90
$ sudo sysctl vm.dirty_ratio=90
$ sudo sysctl vm.dirty_expire_centisecs=12000
$ sudo sysctl vm.dirty_writeback_centisecs=12000
```

**Tuning:** As these parameters apply globally, you can adjust them under load from a different terminal.

If set this way the settings are only temporary and reset after reboot. Although there are options to set them permanently we suggest to play around with session based parameters until you found the setting that works best for your system. Note that these parameters apply to the OS and therefore to all running applications.

Further information on how to adjust dirty page writeback on other operating systems can be found [here](/docs/theory-of-operation.md#linux-dirty-page-writeback-tuning-notes).