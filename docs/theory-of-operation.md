# Theory of Operation

## Architecture Overview

Libbitcoin is a full-stack C++ development toolkit for Bitcoin. It includes everything from basic hash functions (in libbitcoin-system) up to a high performance full node (libbitcoin-node) and a comprehensive client-server connection layer on top (libbitcoin-server)

## Components

<div align="center">

<a href="/docs/images/libbitcoin-components-server.svg">
  <img src="/docs/images/libbitcoin-components-server.svg" alt="libbitcoin components" width="500" />
</a>

</div>

## Startup Process
TBD

## Store Creation
TBD

## Peer Networking

### Peer connections
TBD

### Sync start
TBD

## Sync Mechanisms
libbitcoin-node distinguishes between two different methods for the initial block download (IBD).

### Milestone Sync
Milestone Sync (the default mode) is optimized for speed. If the user has previously fully validated the chain up to the configured milestone hash, expensive block validation steps below that height are skipped on subsequent synchronizations.

Specifically: For blocks at or below the milestone height, all validation except identity checks (block hash, Merkle root, and witness commitment) is skipped. Block headers are always fully validated. Unlike Bitcoin Core's AssumeValid (which skips only signature verification), Libbitcoin skips nearly all block-level checks below the milestone.

Starting from the milestone (and for all new blocks), full validation is applied. There are no trust assumptions — milestone optimization is secure only because the user has previously validated to that point themselves. If the user has not previously validated the chain, they should perform a full validation sync from genesis and record a new milestone for future use.

There is only one level of security: full independent validation. Milestone Sync provides the same security as full validation for users who have previously validated the chain themselves, while significantly accelerating subsequent synchronizations.

The following diagram shows the runtime behaviour of `bs` in milestone sync mode on a 256GB RAM, 64 Cores computer running Ubuntu:

<div align="center">

<a href="/docs/images/brunella_milestone.svg">
  <img src="/docs/images/brunella_milestone.svg" alt="Diagram" width="500" />
</a>

</div>

As the CPU load is significantly lower compared to a full validation sync, one can see clearly how the network performace is the bottleneck. In other words, the faster the network, the faster the sync - until the bottleneck moves to validation power again.

### Full Validation Sync
In full validation mode, the entire blockchain is fully validated from the genesis block onward, without any optimizations or shortcuts. Every block and transaction is checked against all consensus rules, including complete script and signature verification.

There are never any trust assumptions — everything is independently verified. This is recommended for the initial synchronization or whenever the user has not previously validated the chain themselves. Once complete, a milestone can be recorded for faster future synchronizations while maintaining the same single level of security.

The following diagram shows the runtime behaviour of `bs` in full validation sync mode on a 256GB RAM, 64 Cores computer running Ubuntu (same system as above):

<div align="center">

<a href="/docs/images/brunella_full_validation.svg">
  <img src="/docs/images/brunella_full_validation.svg" alt="Diagram" width="500" />
</a>

</div>

## Checkpoints and Milestones

**Checkpoints** are fixed block hashes at specific heights that represent activated consensus rules (soft forks). They are required consensus points: the node rejects any chain that violates a checkpoint, enforcing the canonical chain up to those heights.

**Milestones** are distinct from checkpoints. A milestone is an optional optimization point (not a consensus requirement) that allows skipping full block validation below its height — but only if the user has previously validated the chain to that milestone hash themselves.

In the default configuration, multiple checkpoints are defined (as consensus rules), along with one milestone at a relatively recent height (currently block 900,000).

To perform a full validation sync (disabling milestone optimization while preserving consensus checkpoints), set only the milestone value in the configuration file to the genesis block::

```
[bitcoin]
milestone = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
```

Checkpoints are consensus rules and should not be disabled. Milestones introduce no trust assumptions — they rely solely on the user's prior independent validation. For maximum sovereignty on first use or after potential compromise, perform a full validation sync from genesis. For subsequent synchronizations or everyday use (after initial full validation), Milestone Sync provides the same security with significantly faster performance.

## Linux Dirty Page Writeback Tuning Notes
While standard Ubuntu installations adhere to the upstream Linux kernel defaults (`vm.dirty_bytes=0` and `vm.dirty_background_bytes=0`), enabling percentage-based control via `vm.dirty_ratio=20` and `vm.dirty_background_ratio=10`, some Ubuntu-based derivatives (e.g., Pop!_OS, Kubuntu) override these in their default sysctl configuration with fixed byte thresholds (commonly `vm.dirty_bytes=268435456` or similar values around 256 MiB).

These absolute byte limits take precedence over the percentage-based settings, which can unnecessarily restrict writeback caching on systems with large amounts of RAM and result in suboptimal I/O performance even after manually increasing the ratios.

In contrast, Arch Linux follows a minimalistic philosophy and ships with no default sysctl overrides that set non-zero byte values, preserving the upstream kernel defaults unless explicitly modified by the user or third-party packages.

If your system—particularly on Arch—exhibits limited caching behavior despite `vm.dirty_bytes` and `vm.dirty_background_bytes` being zero (i.e., ratio adjustments have no observable effect), check for unintended overrides introduced by installed software, custom scripts, or kernel modules.

### Verification and Correction

To inspect the active values:

```bash
$ sysctl vm.dirty_bytes vm.dirty_background_bytes vm.dirty_ratio vm.dirty_background_ratio
```

If the byte parameters are non-zero, locate the responsible configuration file (typically under `/etc/sysctl.d/`), comment out or remove the byte entries, then reload:

```bash
$ sudo sysctl --system
```

After removing any byte overrides, your desired ratio settings will take effect as intended.

If the byte values are already zero but performance remains constrained, the issue may stem from per-device dirty throttling or other kernel mechanisms outside these global parameters.