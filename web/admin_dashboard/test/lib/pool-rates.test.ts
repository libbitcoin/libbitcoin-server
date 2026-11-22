import { expect, test } from 'vitest';
import { averageRate, nextBlockRate } from '@/lib/pool-rates';

test('pool_rates__average_rate__empty__null', () => {
  expect(averageRate({ count: 0, virtual: 0, fee: 0, histogram: [] })).toBeNull();
});

test('pool_rates__average_rate__pool__fee_over_virtual_size', () => {
  expect(averageRate({ count: 2, virtual: 400, fee: 2_000, histogram: [] })).toBe(5);
});

test('pool_rates__next_block_rate__empty__null', () => {
  expect(nextBlockRate({ count: 0, virtual: 0, fee: 0, histogram: [] })).toBeNull();
});

test('pool_rates__next_block_rate__under_block__null', () => {
  expect(nextBlockRate({ count: 3, virtual: 600_000, fee: 0, histogram: [[1, 300_000], [10, 200_000], [50, 100_000]] })).toBeNull();
});

test('pool_rates__next_block_rate__over_block__rate_at_block_size', () => {
  expect(nextBlockRate({ count: 3, virtual: 1_500_000, fee: 0, histogram: [[1, 600_000], [10, 500_000], [50, 400_000]] })).toBe(1);
});

test('pool_rates__next_block_rate__exact_block__rate_filling_block', () => {
  expect(nextBlockRate({ count: 3, virtual: 1_200_000, fee: 0, histogram: [[2, 200_000], [10, 600_000], [50, 400_000]] })).toBe(10);
});
