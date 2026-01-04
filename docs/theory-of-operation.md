# Theory of Operation

## Architecture Overview

Libbitcoin is a full-stack C++ development framework for Bitcoin. It includes everything from basic hash functions (in libbitcoin-system) up to a high performance full node (libbitcoin-node) and a comprehensive client-server connection layer on top (libbitcoin-server)

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
Milestone Sync (the default mode) is optimized for speed. Up to a configured milestone (a specific block in the past considered trustworthy), certain expensive validation steps are skipped.

Specifically: Blocks at or below the highest checkpoint/milestone are not fully validated. This primarily means that signature verifications (script validation) in old blocks are skipped or performed in a reduced manner.

Other checks (e.g. Proof-of-Work, basic block rules) are still performed, but resource-intensive operations like full ECDSA verifications are avoided.

Starting from the milestone (and for all new blocks), full validation is applied. As the chain height progresses, the current milestone is updated upward every 50,000 blocks. The most recent milestone is currently block 900,000.

This approach aligns with the security model of Bitcoin Core's "AssumeValid." It assumes the chain up to that point is correct (based on prior validation or community consensus). This makes the IBD significantly faster while providing practically the same level of security for most users.

The following diagram shows the runtime behaviour of `bs` in milestone sync mode on a 256GB RAM, 64 Cores computer running Ubuntu:

<div align="center">

<a href="/docs/images/brunella_milestone.svg">
  <img src="/docs/images/brunella_milestone.svg" alt="Diagram" width="500" />
</a>

</div>

As the CPU load is significantly lower compared to a full validation sync, one can see clearly how the network performace is the bottleneck. In other words, the faster the network, the faster the sync - until the bottleneck moves to validation power again.

### Full Validation Sync
In full validation mode, the entire blockchain is fully validated from the genesis block onward, without any optimizations or shortcuts. This means every block is checked against all consensus rules: Proof-of-Work, block header validity, Merkle root, timestamp rules, etc.

Every transaction in every block is fully validated, including complete script and signature verification (ECDSA signatures for all inputs).

There are no trust assumptions for historical blocks—everything is independently verified.

This is the mode with the highest security, as no part of the chain is assumed to be "pre-validated." However, it is slower and more CPU-intensive.

The following diagram shows the runtime behaviour of `bs` in full validation sync mode on a 256GB RAM, 64 Cores computer running Ubuntu (same system as above):

<div align="center">

<a href="/docs/images/brunella_full_validation.svg">
  <img src="/docs/images/brunella_full_validation.svg" alt="Diagram" width="500" />
</a>

</div>

## Checkpoints and Milestone
**Checkpoints** are a list of fixed block hashes at specific heights (e.g., genesis plus others). They primarily protect against reorganization attacks. The node rejects any alternative chain that violates a checkpoint, enforcing acceptance of the canonical chain up to those points.

The **milestone** is a special (highest) checkpoint that additionally controls validation optimization: Up to this block, full validation is skipped.

In the default configuration, there are multiple checkpoints and one milestone at a relatively recent height (currently block 900,000).

To perform a full validation sync, set both values in the [config](configuration.md#) file to the genesis block (hash: 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f, height 0):

```
[bitcoin]
checkpoint = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
milestone = 000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f:0
```

This disables all optimizations. Everything is fully validated.

Checkpoints and milestones rely on trust in the developers/community (they are hardcoded or configurable), but they don't prevent real attacks on old blocks—they only accelerate syncing. For maximum sovereignty, Full Validation is recommended. For everyday use, Milestone Sync is usually sufficient.

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