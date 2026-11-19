import type { NativeHeader, NativeTop, NativeTops } from './native-contract';
import type { SyncFill, SyncFrontier } from './sync-fill';

export const SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS = 18;
const EXPECTED_BLOCK_INTERVAL_MS = 10 * 60 * 1_000;

export interface SyncTop {
  height: number;
  hash?: string;
}

export interface ChainSync {
  confirmed: SyncTop;
  candidate: SyncTop;
  blocksRemaining: number;
  progress: number;
  diverged: boolean;
  fill: SyncFill | null;
  estimatedTop: number;
}

// The branch header is the candidate header at the confirmed height, if known.
// Frontier events received after the tops advance them between requests. The
// candidate top header projects the network top from its timestamp.
export function chainSync(
  tops: NativeTops | null,
  branch: NativeHeader | null,
  fill: SyncFill | null = null,
  receivedAt: number | null = null,
  topHeader: NativeHeader | null = null,
  now = Date.now()
): ChainSync | null {
  if (tops === null) return null;

  const confirmed = advance(tops.confirmed, fill?.confirmed ?? null, receivedAt);
  const candidate = advance(tops.candidate, fill?.candidate ?? null, receivedAt);
  return {
    confirmed,
    candidate,
    blocksRemaining: Math.max(0, candidate.height - confirmed.height),
    progress:
      candidate.height === 0 ? 100 : Math.min(100, (confirmed.height / candidate.height) * 100),
    diverged: isDiverged(tops.confirmed, tops.candidate, branch),
    fill,
    estimatedTop: estimateTop(candidate.height, topHeader, now),
  };
}

function estimateTop(candidate: number, topHeader: NativeHeader | null, now: number): number {
  if (topHeader === null) return candidate;
  const elapsed = Math.max(0, now - topHeader.timestamp * 1_000);
  return Math.max(candidate, topHeader.height + Math.ceil(elapsed / EXPECTED_BLOCK_INTERVAL_MS));
}

function advance(top: NativeTop, event: SyncFrontier | null, receivedAt: number | null): SyncTop {
  if (event === null || receivedAt === null || event.receivedAt <= receivedAt) return top;
  return event.height === top.height ? top : { height: event.height };
}

function isDiverged(confirmed: NativeTop, candidate: NativeTop, branch: NativeHeader | null) {
  if (candidate.height < confirmed.height) return true;
  if (candidate.height === confirmed.height) return candidate.hash !== confirmed.hash;
  return branch !== null && branch.height === confirmed.height && branch.hash !== confirmed.hash;
}
