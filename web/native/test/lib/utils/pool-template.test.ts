import { expect, test } from 'vitest';
import {
  describePlacement,
  featuredTransaction,
  rateBlend,
  ratePosition,
  squarify,
  templateRates,
  type PoolTemplates,
} from '@/lib/utils/pool-template';

const pool: PoolTemplates = {
  transactions: [
    { hash: 'a', size: 100, fee: 100, parents: [], children: [1], cluster: 0, chunk: 0, template: 0, position: 1 },
    { hash: 'b', size: 100, fee: 9_900, parents: [0], children: [], cluster: 0, chunk: 0, template: 0, position: 2 },
    { hash: 'c', size: 200, fee: 2_000, parents: [], children: [], cluster: 1, chunk: 1, template: 0, position: 3 },
    { hash: 'd', size: 600, fee: 600, parents: [], children: [], cluster: 2, chunk: 2, template: 0, position: 4 },
  ],
  chunks: [
    { cluster: 0, transactions: [0, 1], size: 200, fee: 10_000 },
    { cluster: 1, transactions: [2], size: 200, fee: 2_000 },
    { cluster: 2, transactions: [3], size: 600, fee: 600 },
  ],
  clusters: [
    { transactions: [0, 1], chunks: [0] },
    { transactions: [2], chunks: [1] },
    { transactions: [3], chunks: [2] },
  ],
  templates: [{ chunks: [0, 1, 2], count: 4, size: 1_000, fee: 12_600 }],
  capacity: 1_000,
  first: 0,
};

test('pool_template__template_rates__chunks__minimum_median_maximum', () => {
  expect(templateRates(pool, pool.templates[0])).toEqual({ minimum: 1, median: 1, maximum: 50 });
});

test('pool_template__template_rates__empty__zero', () => {
  expect(templateRates(pool, { chunks: [], count: 0, size: 0, fee: 0 })).toEqual({
    minimum: 0,
    median: 0,
    maximum: 0,
  });
});

test('pool_template__featured_transaction__no_large_chunk__first_transaction', () => {
  expect(featuredTransaction(pool, 0)).toBe(0);
});

test('pool_template__featured_transaction__missing_template__null', () => {
  expect(featuredTransaction(pool, 5)).toBeNull();
});

test('pool_template__describe_placement__low_own_rate__cluster_pays_more', () => {
  expect(describePlacement(pool.chunks[0], 1)).toContain('more than its own');
});

test('pool_template__describe_placement__high_own_rate__more_than_cluster', () => {
  expect(describePlacement(pool.chunks[0], 99)).toContain("more than its cluster's");
});

test('pool_template__describe_placement__single__own_rate', () => {
  expect(describePlacement(pool.chunks[1], 10)).toContain('its own fee rate');
});

test('pool_template__rate_position__bounds__clamped', () => {
  expect(ratePosition(0.5)).toBe(0);
  expect(ratePosition(1)).toBe(0);
  expect(ratePosition(300)).toBe(1);
  expect(ratePosition(10_000)).toBe(1);
});

test('pool_template__squarify__full__covers_area_within_bounds', () => {
  const areas = [0.4, 0.3, 0.2, 0.25, 0.15, 0.1, 0.12, 0.08];
  const rects = squarify(areas, 1.6, 1);
  expect(rects).toHaveLength(areas.length);
  rects.forEach((rect, index) => {
    expect(rect.width * rect.height).toBeCloseTo(areas[index], 9);
    expect(rect.x).toBeGreaterThanOrEqual(-1e-9);
    expect(rect.y).toBeGreaterThanOrEqual(-1e-9);
    expect(rect.x + rect.width).toBeLessThanOrEqual(1.6 + 1e-9);
    expect(rect.y + rect.height).toBeLessThanOrEqual(1 + 1e-9);
  });
});

test('pool_template__squarify__first__at_origin', () => {
  const [first] = squarify([0.5, 0.5], 1, 1);
  expect(first.x).toBe(0);
  expect(first.y).toBe(0);
});

test('pool_template__squarify__partial__leaves_space', () => {
  const rects = squarify([0.25, 0.25], 1, 1);
  const covered = rects.reduce((total, rect) => total + rect.width * rect.height, 0);
  expect(covered).toBeCloseTo(0.5, 9);
});

test('pool_template__squarify__empty__empty', () => {
  expect(squarify([], 1, 1)).toEqual([]);
});

test('pool_template__rate_blend__ends__surface_to_text', () => {
  expect(rateBlend(1)).toEqual({ tint: 'accent', base: 'surface', share: 22 });
  const top = rateBlend(300);
  expect([top.tint, top.base]).toEqual(['text', 'accent']);
  expect(top.share).toBeCloseTo(55, 9);
});

test('pool_template__rate_blend__accent_position__continuous', () => {
  const rate = Math.exp(0.7 * Math.log(300));
  expect(rateBlend(rate * 0.999).share).toBeCloseTo(100, 0);
  expect(rateBlend(rate * 1.001).share).toBeCloseTo(0, 0);
});
