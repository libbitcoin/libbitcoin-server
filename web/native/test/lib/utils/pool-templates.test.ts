import { expect, test } from 'vitest';
import { toPoolTemplates } from '@/lib/utils/pool-templates';

const element = (virtual: number, fee: number, cluster: number) => ({
  segregated: false,
  nominal: virtual,
  maximal: virtual,
  weight: virtual * 4,
  virtual,
  value: fee,
  spend: 0,
  fee,
  cluster,
});

test('pool_templates__to_pool_templates__clusters__chunks_in_template_order', () => {
  const pool = toPoolTemplates([[element(100, 1000, 0), element(110, 1100, 1), element(208, 9400, 1)]]);
  expect(pool.chunks).toEqual([
    { cluster: 0, transactions: [0], size: 100, fee: 1000 },
    { cluster: 1, transactions: [1, 2], size: 318, fee: 10500 },
  ]);
  expect(pool.templates).toEqual([{ chunks: [0, 1], count: 3, size: 418, fee: 11500 }]);
});

test('pool_templates__to_pool_templates__transactions__positions_from_one', () => {
  const pool = toPoolTemplates([[element(100, 1000, 0), element(110, 1100, 1)]]);
  expect(pool.transactions.map((transaction) => transaction.position)).toEqual([1, 2]);
  expect(pool.transactions.map((transaction) => transaction.chunk)).toEqual([0, 1]);
});

test('pool_templates__to_pool_templates__second_template__global_clusters', () => {
  const pool = toPoolTemplates([[element(100, 1000, 0)], [element(110, 1100, 0)]]);
  expect(pool.transactions.map((transaction) => transaction.cluster)).toEqual([0, 1]);
  expect(pool.transactions.map((transaction) => transaction.template)).toEqual([0, 1]);
  expect(pool.templates.map((template) => template.chunks)).toEqual([[0], [1]]);
});

test('pool_templates__to_pool_templates__first__height_of_first_template', () => {
  expect(toPoolTemplates([[element(100, 1000, 0)]], 977271).first).toBe(977271);
});

test('pool_templates__to_pool_templates__empty_template__empty_block', () => {
  const pool = toPoolTemplates([[]]);
  expect(pool.templates).toEqual([{ chunks: [], count: 0, size: 0, fee: 0 }]);
  expect(pool.transactions).toEqual([]);
});
