import type { NativeFees } from './native-contract';

export function averageRate(pool: NativeFees): number | null {
  return pool.virtual > 0 ? pool.fee / pool.virtual : null;
}

export const BLOCK_VIRTUAL_SIZE = 1_000_000;

// The lowest rate within the highest-rate block of pool virtual size, or null
// if the pool fits in one block.
export function nextBlockRate(pool: NativeFees): number | null {
  const bins = [...pool.histogram]
    .filter(([, size]) => size > 0)
    .sort(([left], [right]) => right - left);
  let covered = 0;
  for (const [rate, size] of bins) {
    covered += size;
    if (covered >= BLOCK_VIRTUAL_SIZE) return rate;
  }

  return null;
}
