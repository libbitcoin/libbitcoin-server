import { expect, test } from 'vitest';
import { chunkRate } from '@/lib/pool-template';
import { chunk, linearize, pack, stubPoolTemplates } from '@/lib/pool-template-stub';

const transaction = (size: number, fee: number, parents: number[] = []) => ({ size, fee, parents });

test('pool_template_stub__linearize__high_rate_child__ancestor_set_first', () => {
  const transactions = [transaction(100, 100), transaction(100, 5_000, [0]), transaction(100, 1_000)];
  expect(linearize(transactions, [0, 1, 2])).toEqual([0, 1, 2]);
});

test('pool_template_stub__linearize__low_rate_child__parent_first', () => {
  const transactions = [transaction(100, 5_000), transaction(100, 100, [0])];
  expect(linearize(transactions, [0, 1])).toEqual([0, 1]);
});

test('pool_template_stub__chunk__child_pays__one_chunk', () => {
  const transactions = [transaction(100, 100), transaction(100, 5_000, [0])];
  expect(chunk(transactions, [0, 1])).toEqual([{ transactions: [0, 1], size: 200, fee: 5_100 }]);
});

test('pool_template_stub__chunk__descending__separate_chunks', () => {
  const transactions = [transaction(100, 5_000), transaction(100, 100, [0])];
  expect(chunk(transactions, [0, 1])).toEqual([
    { transactions: [0], size: 100, fee: 5_000 },
    { transactions: [1], size: 100, fee: 100 },
  ]);
});

test('pool_template_stub__pack__over_capacity__descending_rate_templates', () => {
  const chunks = [
    { cluster: 0, transactions: [0], size: 600, fee: 600 },
    { cluster: 1, transactions: [1], size: 600, fee: 6_000 },
    { cluster: 2, transactions: [2], size: 300, fee: 900 },
  ];
  expect(pack(chunks, 1_000)).toEqual([
    { chunks: [1, 2], count: 2, size: 900, fee: 6_900 },
    { chunks: [0], count: 1, size: 600, fee: 600 },
  ]);
});

test('pool_template_stub__stub_pool_templates__seed__deterministic', () => {
  const first = stubPoolTemplates(3, 200_000, 50_000);
  const second = stubPoolTemplates(3, 200_000, 50_000);
  expect(second.transactions.map((entry) => entry.hash)).toEqual(
    first.transactions.map((entry) => entry.hash)
  );
});

test('pool_template_stub__stub_pool_templates__pool__consistent', () => {
  const pool = stubPoolTemplates(11, 400_000, 100_000);
  const size = pool.transactions.reduce((total, entry) => total + entry.size, 0);
  expect(size).toBeGreaterThanOrEqual(400_000);
  expect(pool.templates.reduce((total, entry) => total + entry.size, 0)).toBe(size);

  for (const template of pool.templates) {
    expect(template.size).toBeLessThanOrEqual(pool.capacity);
    const rates = template.chunks.map((index) => chunkRate(pool.chunks[index]));
    rates.slice(1).forEach((rate, index) => {
      expect(rate).toBeLessThanOrEqual(rates[index]);
    });
  }

  for (const cluster of pool.clusters) {
    const rates = cluster.chunks.map((index) => chunkRate(pool.chunks[index]));
    rates.slice(1).forEach((rate, index) => {
      expect(rate).toBeLessThanOrEqual(rates[index]);
    });
  }

  // Every parent is mined before its children.
  const order = (index: number) => {
    const entry = pool.transactions[index];
    return entry.template * 1_000_000 + entry.position;
  };
  pool.transactions.forEach((entry, index) => {
    expect(entry.template).toBeGreaterThanOrEqual(0);
    expect(pool.chunks[entry.chunk].transactions).toContain(index);
    for (const parent of entry.parents) expect(order(parent)).toBeLessThan(order(index));
  });
});
