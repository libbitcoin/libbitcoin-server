import { expect, test } from 'vitest';
import type {
  NativeHeader,
  NativeTop,
  NativeTops,
} from '@/lib/native-contract';
import { chainSync, isHeaderCurrent } from '@/lib/sync-status';

const HASH_A = 'a'.repeat(64);
const HASH_B = 'b'.repeat(64);
const NOW = 1_700_000_000_000;
const NOW_SECONDS = NOW / 1_000;

function top(height: number, hash = HASH_A): NativeTop {
  return { hash, height, header: { timestamp: NOW_SECONDS } };
}

function tops(confirmed: NativeTop, candidate: NativeTop): NativeTops {
  return { confirmed, candidate };
}

function header(
  height: number,
  timestamp = NOW_SECONDS,
  hash = HASH_A
): NativeHeader {
  return { hash, height, timestamp };
}

test('sync_status__chain_sync__no_tops__null', () => {
  expect(chainSync(null, null)).toBeNull();
});

test('sync_status__chain_sync__current_top_header__blocks_remaining', () => {
  const sync = chainSync(tops(top(50), top(200, HASH_B)), null, null, null, header(200), NOW);
  expect(sync?.blocksRemaining).toBe(150);
  expect(sync?.progress).toBe(25);
});

test('sync_status__chain_sync__stale_top_header__progress_unknown', () => {
  const sync = chainSync(tops(top(50), top(200, HASH_B)), null, null, null, header(200, NOW_SECONDS - 7_200), NOW);
  expect(sync?.blocksRemaining).toBeNull();
  expect(sync?.progress).toBe(0);
});

test('sync_status__chain_sync__stale_top_header__estimated_top_from_elapsed', () => {
  const sync = chainSync(tops(top(50), top(200, HASH_B)), null, null, null, header(200, NOW_SECONDS - 7_200), NOW);
  expect(sync?.estimatedTop).toBe(212);
});

test('sync_status__chain_sync__equal_heights_same_hash__not_diverged', () => {
  expect(chainSync(tops(top(100), top(100)), null)?.diverged).toBe(false);
});

test('sync_status__chain_sync__equal_heights_other_hash__diverged', () => {
  expect(chainSync(tops(top(100), top(100, HASH_B)), null)?.diverged).toBe(true);
});

test('sync_status__chain_sync__candidate_below_confirmed__diverged', () => {
  expect(chainSync(tops(top(100), top(99)), null)?.diverged).toBe(true);
});

test('sync_status__chain_sync__branch_other_hash_at_confirmed__diverged', () => {
  expect(chainSync(tops(top(100), top(200, HASH_B)), header(100, NOW_SECONDS, HASH_B))?.diverged).toBe(true);
});

test('sync_status__chain_sync__branch_same_hash_at_confirmed__not_diverged', () => {
  expect(chainSync(tops(top(100), top(200, HASH_B)), header(100))?.diverged).toBe(false);
});

test('sync_status__is_header_current__null__false', () => {
  expect(isHeaderCurrent(null, NOW)).toBe(false);
});

test('sync_status__is_header_current__hour_old__true', () => {
  expect(isHeaderCurrent(header(1, NOW_SECONDS - 3_600), NOW)).toBe(true);
});

test('sync_status__is_header_current__over_hour_old__false', () => {
  expect(isHeaderCurrent(header(1, NOW_SECONDS - 3_601), NOW)).toBe(false);
});
