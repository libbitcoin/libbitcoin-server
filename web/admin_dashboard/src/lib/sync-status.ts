import type { NativeHeader, NativeTop, NativeTops } from './native-contract';

export const SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS = 18;

export interface ChainSync {
  confirmed: NativeTop;
  candidate: NativeTop;
  blocksRemaining: number;
  progress: number;
  diverged: boolean;
}

// The branch header is the candidate header at the confirmed height, if known.
export function chainSync(tops: NativeTops | null, branch: NativeHeader | null): ChainSync | null {
  if (tops === null) return null;

  const { confirmed, candidate } = tops;
  return {
    confirmed,
    candidate,
    blocksRemaining: Math.max(0, candidate.height - confirmed.height),
    progress:
      candidate.height === 0 ? 100 : Math.min(100, (confirmed.height / candidate.height) * 100),
    diverged: isDiverged(confirmed, candidate, branch),
  };
}

function isDiverged(confirmed: NativeTop, candidate: NativeTop, branch: NativeHeader | null) {
  if (candidate.height < confirmed.height) return true;
  if (candidate.height === confirmed.height) return candidate.hash !== confirmed.hash;
  return branch !== null && branch.height === confirmed.height && branch.hash !== confirmed.hash;
}
