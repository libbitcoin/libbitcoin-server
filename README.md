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
This is the **server component of libbitcoin v4**. It integrates the basic components and offers a **high performance Bitcoin full node** with a comprehensive set of client-server interfaces like **bitcoind, electrum and stratum** as well as an integrated **block explorer**. It makes full use of the available hardware, internet connection and available RAM and CPU so you can sync the Bitcoin blockchain from genesis, today and in the future.

Libbitcoin is a multi-platform software that works on Linux, Windows and macOS (Intel and ARM).

**License Overview**

All files in this repository fall under the license specified in [COPYING](COPYING). The project is licensed under the [GNU Affero General Public License v3.0](https://www.gnu.org/licenses/agpl-3.0.en.html). It may be used within a proprietary project, but the core library and any changes to it must be published online. Source code for this library must always remain free for everybody to access.

**About libbitcoin**

The libbitcoin toolkit is a set of cross platform C++ libraries for building Bitcoin applications. The toolkit consists of several libraries, most of which depend on the foundational [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system) library. Each library's repository can be cloned and built separately.

## Contents
- [libbitcoin-server](#libbitcoin-server)
  - [tl;dr](#tldr)
  - [Contents](#contents)
  - [Download](#download)
  - [Build from Source](#build-from-source)
    - [Linux](#linux)
      - [Requirements](#requirements)
      - [GNU Autotools Build](#gnu-autotools-build)
      - [CMake Build](#cmake-build)
      - [CMake Presets Build](#cmake-presets-build)
      - [CPU Extensions](#cpu-extensions)
    - [Windows](#windows)
    - [macOS](#macos)
  - [Running bs](#running-bs)
    - [Startup Process](#startup-process)
    - [Using the integrated Block Explorer](#using-the-integrated-block-explorer)
    - [Using the interactive console](#using-the-interactive-console)
    - [Logging](#logging)
    - [Performance](#performance)
    - [Memory Management](#memory-management)
      - [Kernel VM Tuning Parameters](#kernel-vm-tuning-parameters)

---

## Download

> **Prebuilt binaries are coming soon.**
>
> We are working on providing ready-to-run binaries for Linux and Windows. Once available, downloading and running `bs` will require no compiler or build tools — just download, configure, and run.
>
> Check back here or watch the [releases page](https://github.com/libbitcoin/libbitcoin-server/releases) for updates.

In the meantime, [build from source](#build-from-source) using one of the provided installation scripts below.

---

## Build from Source

Building from source is intended for advanced users and developers who want full control over the build. The build process clones and compiles all dependencies automatically.

libbitcoin-server ships with **three installation scripts**, each targeting a different toolchain:

| Script | Toolchain | Best for |
|--------|-----------|----------|
| `builds/gnu/install-gnu.sh` | GNU Autotools (make) | Linux, macOS — traditional |
| `builds/cmake/install-cmake.sh` | CMake | Linux, macOS — flexible |
| `builds/cmake/install-presets.sh` | CMake with Presets | Linux — simplified, named configurations |

All scripts download and build Boost, libsecp256k1, and the full libbitcoin stack from source. A complete build typically takes **30–90 minutes** depending on your hardware.

### Linux

#### Requirements

The following are the minimum requirements for Ubuntu 24.04 LTS. Other distributions require equivalent packages.

| Requirement | Package |
|-------------|---------|
| C++20 compiler | `build-essential` (GCC ≥ 13 or Clang ≥ 16) |
| Autoconf / Automake / Libtool | `autoconf automake libtool` |
| pkg-config | `pkg-config` |
| git | `git` |
| CMake ≥ 3.21 *(CMake builds only)* | `cmake` |

Install everything at once:

```bash
# For GNU Autotools builds:
sudo apt install build-essential git autoconf automake libtool pkg-config

# Additionally for CMake builds:
sudo apt install cmake
```

---

#### GNU Autotools Build

Uses `make` and Autotools (`autoconf`, `automake`, `libtool`). Well-tested and the reference build for CI.

Clone the repository and run the build script:

```bash
git clone https://github.com/libbitcoin/libbitcoin-server
cd libbitcoin-server

./builds/gnu/install-gnu.sh \
  --prefix=$HOME/libbitcoin \
  --build-secp256k1 \
  --build-boost \
  --build-config=release \
  --build-link=static \
  --build-post-install-clean
```

On success, the `bs` binary is at `$HOME/libbitcoin/bin/bs`.

**Key options:**

| Option | Values | Description |
|--------|--------|-------------|
| `--prefix=<path>` | absolute path | Installation destination |
| `--build-config=<mode>` | `release`, `debug` | Build configuration |
| `--build-secp256k1` | — | Build libsecp256k1 from source |
| `--build-boost` | — | Build Boost from source |
| `--build-use-local-src` | — | Reuse already-present source directories instead of cloning from GitHub |
| `--build-parallel=<n>` | integer | Number of parallel compile jobs |
| `--enable-shani` | — | Enable SHA-NI hardware acceleration |
| `--enable-avx2` | — | Enable AVX2 SIMD acceleration |
| `--enable-sse41` | — | Enable SSE4.1 acceleration |
| `--enable-avx512` | — | Enable AVX-512 acceleration |

See all options:

```bash
./builds/gnu/install-gnu.sh --help
```

---

#### CMake Build

Uses CMake instead of Autotools. CPU extension flags use CMake's `-D` syntax.

```bash
git clone https://github.com/libbitcoin/libbitcoin-server
cd libbitcoin-server

./builds/cmake/install-cmake.sh \
  --prefix=$HOME/libbitcoin \
  --build-secp256k1 \
  --build-boost \
  --build-config=release \
  --build-link=static \
  --build-post-install-clean
```

**Key options:**

| Option | Values | Description |
|--------|--------|-------------|
| `--prefix=<path>` | absolute path | Installation destination |
| `--build-config=<mode>` | `release`, `debug` | Build configuration |
| `--build-secp256k1` | — | Build libsecp256k1 from source |
| `--build-boost` | — | Build Boost from source |
| `--build-use-local-src` | — | Reuse already-present source directories instead of cloning from GitHub |
| `-Denable-shani=ON` | `ON`, `OFF` | Enable SHA-NI hardware acceleration |
| `-Denable-avx2=ON` | `ON`, `OFF` | Enable AVX2 SIMD acceleration |
| `-Denable-sse41=ON` | `ON`, `OFF` | Enable SSE4.1 acceleration |
| `-Denable-avx512=ON` | `ON`, `OFF` | Enable AVX-512 acceleration |

See all options:

```bash
./builds/cmake/install-cmake.sh --help
```

---

#### CMake Presets Build

The simplest scripted build on Linux. Named presets combine all configuration choices (toolchain, build type, link mode) into a single `--build-preset` parameter. The install prefix and build directory are set automatically relative to the source tree.

Available presets:

| Preset | Config | Link |
|--------|--------|------|
| `nix-gnu-release-static` | release | static |
| `nix-gnu-release-shared` | release | dynamic |
| `nix-gnu-debug-static` | debug | static |
| `nix-gnu-debug-shared` | debug | dynamic |

**For most users, `nix-gnu-release-static` is the recommended choice:**

```bash
git clone https://github.com/libbitcoin/libbitcoin-server
cd libbitcoin-server

./builds/cmake/install-presets.sh \
  --build-preset=nix-gnu-release-static \
  --build-secp256k1 \
  --build-boost \
```

The `bs` binary is placed at `prefix/nix-gnu-release-static/bin/bs` inside the repository directory.

The presets script supports `--build-use-local-src` and `--build-post-install-clean` as well.

See all options:

```bash
./builds/cmake/install-presets.sh --help
```

---

#### CPU Extensions

libbitcoin can be compiled with optional CPU acceleration. If your processor supports them, these significantly improve hashing and signature verification throughput.

| Extension | Flag (GNU) | Flag (CMake) | Description |
|-----------|-----------|--------------|-------------|
| SHA-NI | `--enable-shani` | `-Denable-shani=ON` | SHA hardware instructions (Intel) |
| SSE4.1 | `--enable-sse41` | `-Denable-sse41=ON` | SIMD integer ops |
| AVX2 | `--enable-avx2` | `-Denable-avx2=ON` | 256-bit SIMD |
| AVX-512 | `--enable-avx512` | `-Denable-avx512=ON` | 512-bit SIMD |

> **Important:** Enabling an extension your CPU does not support will compile successfully but crash at runtime. Always verify support first.

Check which extensions your CPU supports:

```bash
# After building, run:
./bs --hardware
```

Example output on a supported system:

```
Hardware configuration...
arm..... platform:0.
intel... platform:1.
avx512.. platform:1 compiled:1.
avx2.... platform:1 compiled:1.
sse41... platform:1 compiled:1.
shani... platform:1 compiled:1.
```

`platform:1 compiled:1` means the CPU supports it and it was compiled in.

Example build with all extensions enabled (GNU):

```bash
./builds/gnu/install-gnu.sh \
  --prefix=$HOME/libbitcoin \
  --build-secp256k1 \
  --build-boost \
  --build-config=release \
  --build-link=static \
  --enable-shani \
  --enable-sse41 \
  --enable-avx2
```

---

### Windows

Windows builds use the provided Visual Studio 2022 project files in `builds/msvc/vs2022/`.

Detailed instructions are **TBD**.

---

### macOS

The GNU Autotools and CMake build scripts work on macOS with clang. Install the build prerequisites via Homebrew:

```bash
brew install autoconf automake libtool pkg-config cmake
```

Then follow the [GNU Autotools Build](#gnu-autotools-build) or [CMake Build](#cmake-build) instructions above.

Detailed instructions are **TBD**.

## Running bs

After a successful build, navigate to the `bin/` directory and confirm the binary is working:

```bash
./bs --help
```

```
Usage: bs [-abdfhiklnrsv] [--config value] [--test value] [--write value]

Info: Runs a full node bitcoin server.                                   

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

### Startup Process

Start the server with a configuration file:

```bash
./bs --config ./bs.cfg
```

On first launch, `bs` will:
- Create a data store at the path specified in `bs.cfg` (defaults to `./bitcoin/`).
- Establish peer connections and begin syncing the Bitcoin blockchain in **milestone sync** mode.
- Open an interactive console — see [Using the console](#using-the-console).
- Write log files — see [Logging](#logging).
- Create a database snapshot when reaching current block height

**Milestone sync** (the default sync mode) skips expensive block validation for blocks below the configured checkpoint, making the initial sync significantly faster. There are no trust assumptions — milestone optimization is secure only because the user has previously validated to that point themselves. If the user has not previously validated the chain, they should perform a full validation sync from genesis and record a new milestone for future use.

For a **full validation sync** from genesis, add to your config file:

```ini
[bitcoin]
checkpoint = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
milestone  = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
```

After the sync has finished and the snapshot has been created, retrieve an arbitrary hash via the native REST interface and use it for future syncs in milestone mode. Query the REST API with curl or just look up the hash in the [integrated Block Explorer](#using-the-integrated-block-explorer):

```bash
curl -s http://localhost:8181/v1/block/height/900000/header?format=json
```

Information on how to setup the native REST endpoint and other configuration options are documented in [docs/configuration.md](/docs/configuration.md).

### Using the integrated Block Explorer

The integrated Block Explorer gives you easy access to the Bitcoin blockchain. It uses the same query interface as the native REST endpoint and is configured alongside it. Connect to the Block explorer by visiting the configured URL in any modern browser, e.g. <a href="http://localhost:8181">http://localhost:8181</a>.

<div align="center">

<a href="/docs/images/block_explorer.png">
  <img src="/docs/images/block_explorer.png" alt="Block Explorer - libbitcoin-server v4" width="400" />
</a>

</div>

### Using the interactive console

Press **`m` + Enter** in the terminal to open the interactive console:

```
2026-05-06T23:56:47Z.0 Toggle: [v]erbose
2026-05-06T23:56:47Z.0 Toggle: [o]bjects
2026-05-06T23:56:47Z.0 Toggle: [q]uitting
2026-05-06T23:56:47Z.0 Toggle: [f]ault
2026-05-06T23:56:47Z.0 Toggle: [r]emote
2026-05-06T23:56:47Z.0 Toggle: [x]proxy
2026-05-06T23:56:47Z.0 Toggle: [p]rotocol
2026-05-06T23:56:47Z.0 Toggle: [s]ession
2026-05-06T23:56:47Z.0 Toggle: [n]ews
2026-05-06T23:56:47Z.0 Toggle: [a]pplication
2026-05-06T23:56:47Z.0 Option: [z]eroize disk full error
2026-05-06T23:56:47Z.0 Option: [w]ork distribution
2026-05-06T23:56:47Z.0 Option: [t]est built-in case
2026-05-06T23:56:47Z.0 Option: [m]enu of options and toggles
2026-05-06T23:56:47Z.0 Option: [i]nfo about store
2026-05-06T23:56:47Z.0 Option: [h]old network communication
2026-05-06T23:56:47Z.0 Option: [g]o network communication
2026-05-06T23:56:47Z.0 Option: [e]rrors in store
2026-05-06T23:56:47Z.0 Option: [c]lose the node
2026-05-06T23:56:47Z.0 Option: [b]ackup the store

```

The console lets you toggle log verbosity, pause/resume networking, trigger a backup, and shut down cleanly without risking store corruption.

### Logging

`bs` creates the following files at startup:

| File | Description |
|------|-------------|
| `events.log` | Sync log: block heights, timing |
| `bs_begin.log` | Session start log (rotated to `bs_end.log` on clean shutdown) |
| `hosts.cache` | Peer address cache |

### Performance

Monitor sync progress via `events.log`. To find when a specific block was organized:

```bash
grep 'block_organized..... 700000' ./events.log
```

```
block_organized..... 700000 11053
```

The number on the right is elapsed seconds since startup. In this example, block 700,000 was reached in 11,053 seconds (≈ 3 hours).

The following chart shows IBD performance from a reference machine:

<details>
<summary><strong>brunella - Dell Precision Tower 7810</strong></summary>

- Dual Intel® Xeon® CPU E5-2683 v4 @ 2.10 GHz (32 cores / 64 threads)
- **256 GB** DDR4 ECC Registered (RDIMM)
- 4 TB SSD Samsung 990 PRO (PCIe Gen4)
- 2 GBit/s fiber internet

</details>

<div align="center">

<a href="/docs/images/brunella_compare.png">
  <img src="/docs/images/brunella_compare.png" alt="brunella performance compare" width="400" />
</a>

</div>

### Memory Management

On Linux, tuning the kernel's dirty-page writeback parameters can significantly improve sync throughput by allowing the OS to buffer more writes in RAM before flushing to disk.

#### Kernel VM Tuning Parameters

These are the stock Ubuntu LTS values:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `vm.dirty_background_ratio` | `10` | Start background writeback at this % of dirty RAM |
| `vm.dirty_ratio` | `20` | Block applications at this % of dirty RAM |
| `vm.dirty_expire_centisecs` | `3000` | Mark pages stale after 30 s (unit: 1/100 s) |
| `vm.dirty_writeback_centisecs` | `500` | Wake writeback worker every 5 s |

Check current values:

```bash
sysctl vm.dirty_background_ratio vm.dirty_ratio vm.dirty_expire_centisecs vm.dirty_writeback_centisecs
```

For libbitcoin workloads the following values have proven most effective — they allow the server to use up to 95 % of RAM as a write buffer and flush every 2 minutes:

```bash
sudo sysctl vm.dirty_background_ratio=95
sudo sysctl vm.dirty_ratio=95
sudo sysctl vm.dirty_expire_centisecs=12000
sudo sysctl vm.dirty_writeback_centisecs=12000
```

**Tuning:** As these parameters apply globally, you can adjust them under load from a different terminal.

If set this way the settings are only temporary and reset after reboot. Although there are options to set them permanently we suggest to play around with session based parameters until you found the setting that works best for your system. Note that these parameters apply to the OS and therefore to all running applications.

Further information on how to adjust dirty page writeback on other operating systems can be found [here](/docs/theory-of-operation.md#linux-dirty-page-writeback-tuning-notes).
