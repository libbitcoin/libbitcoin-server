import { expect, test } from 'vitest';
import { takeJson } from '@/lib/mining/miner';

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
