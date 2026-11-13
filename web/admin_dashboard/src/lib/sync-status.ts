import type { NativeHeader } from './native-contract';

export const SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS = 18;
const EXPECTED_BLOCK_INTERVAL_MS = 10 * 60 * 1_000;

export interface SyncEstimate {
  blocksRemaining: number;
  estimatedTop: number;
  progress: number;
}

export function estimateSyncProgress(
  confirmed: number | null,
  header: NativeHeader | null,
  now: number
): SyncEstimate | null {
  if (
    confirmed === null ||
    !Number.isSafeInteger(confirmed) ||
    confirmed < 0 ||
    header === null ||
    !Number.isSafeInteger(header.height) ||
    header.height < 0 ||
    header.height > confirmed ||
    !Number.isFinite(header.timestamp) ||
    header.timestamp <= 0 ||
    !Number.isFinite(now) ||
    now < 0
  )
    return null;

  const elapsed = Math.max(0, now - header.timestamp * 1_000);
  const estimatedTop = Math.max(
    confirmed,
    header.height + Math.ceil(elapsed / EXPECTED_BLOCK_INTERVAL_MS)
  );
  const blocksRemaining = estimatedTop - confirmed;
  return {
    blocksRemaining,
    estimatedTop,
    progress: estimatedTop === 0 ? 100 : (confirmed / estimatedTop) * 100,
  };
}
