import { expect, test } from 'vitest';
import { carried, takeJson, type MinerState } from '@/lib/mining/miner';

const run: MinerState = {
  status: 'stopped',
  settings: { url: 'ws://host:48880', worker: 'browser', password: '', threads: 4 },
  jobs: 3,
  hashrate: 0,
  history: [{ time: 1, rate: 2 }],
  hashes: 100,
  accepted: 5,
  rejected: 1,
  blocks: 0,
  shares: [],
};

const take = (text: string) => {
  const holder = { text };
  return { values: takeJson(holder), rest: holder.text };
};

test('takeJson takes whole messages and keeps a partial one', () => {
  expect(take('{"id":1}\n{"id":2}\n{"id"')).toEqual({ values: ['{"id":1}', '{"id":2}'], rest: '{"id"' });
});

test('takeJson separates messages without a newline between them', () => {
  expect(take('{"a":1}{"b":[1,{"c":2}]}').values).toEqual(['{"a":1}', '{"b":[1,{"c":2}]}']);
});

test('takeJson ignores brackets and quotes within strings', () => {
  expect(take('{"m":"a}b\\"{["}').values).toEqual(['{"m":"a}b\\"{["}']);
});

test('takeJson takes arrays and drops stray text', () => {
  expect(take('noise [{"id":3},{"id":4}] tail')).toEqual({
    values: ['[{"id":3},{"id":4}]'],
    rest: '',
  });
});

test('takeJson joins a message split across frames', () => {
  const holder = { text: '{"id":5,"res' };
  expect(takeJson(holder)).toEqual([]);
  holder.text += 'ult":true}\n';
  expect(takeJson(holder)).toEqual(['{"id":5,"result":true}']);
});

test('carried keeps the run for the same url and worker', () => {
  expect(carried(run, { url: 'ws://host:48880', worker: 'browser', password: 'x', threads: 8 })).toEqual({ jobs: 3, history: [{ time: 1, rate: 2 }], hashes: 100, accepted: 5, rejected: 1, blocks: 0, shares: [] });
});

test('carried drops the run for another url', () => {
  expect(carried(run, { url: 'ws://other:48880', worker: 'browser', password: '', threads: 4 })).toEqual({});
});

test('carried drops the run for another worker', () => {
  expect(carried(run, { url: 'ws://host:48880', worker: 'laptop', password: '', threads: 4 })).toEqual({});
});

test('carried keeps nothing for a first run', () => {
  expect(carried({ ...run, settings: undefined }, { url: 'ws://host:48880', worker: 'browser', password: '', threads: 4 })).toEqual({});
});
