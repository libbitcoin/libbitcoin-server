import { expect, test } from 'vitest';
import { classifySearchQuery } from '@/lib/utils/search';

const HASH =
  '000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f';
const HASH_UPPER =
  '000000000019D6689C085AE165831E934FF763AE46A2A6C172B3F1B60A8CE26F';

test('search__classify_search_query__whitespace__unknown', () => {
  expect(classifySearchQuery('   ')).toEqual({ type: 'unknown' });
});

test('search__classify_search_query__upper_hash__lower_hash', () => {
  expect(classifySearchQuery(HASH_UPPER)).toEqual({ type: 'hash', value: HASH });
});

test('search__classify_search_query__padded_hash__trimmed_hash', () => {
  expect(classifySearchQuery(` ${HASH} `)).toEqual({ type: 'hash', value: HASH });
});

test('search__classify_search_query__outpoint__outpoint', () => {
  expect(classifySearchQuery(`${HASH_UPPER}:7`)).toEqual({ type: 'outpoint', value: HASH, outputIndex: 7 });
});

test('search__classify_search_query__outpoint_maximum_index__outpoint', () => {
  expect(classifySearchQuery(`${HASH}:4294967295`)).toEqual({ type: 'outpoint', value: HASH, outputIndex: 4294967295 });
});

test('search__classify_search_query__outpoint_index_overflow__unknown', () => {
  expect(classifySearchQuery(`${HASH}:4294967296`)).toEqual({ type: 'unknown' });
});

test('search__classify_search_query__height__height', () => {
  expect(classifySearchQuery('840000')).toEqual({ type: 'block-height', value: '840000' });
});

test('search__classify_search_query__leading_zero_height__normalized_height', () => {
  expect(classifySearchQuery('007')).toEqual({ type: 'block-height', value: '7' });
});

test('search__classify_search_query__zeros__zero_height', () => {
  expect(classifySearchQuery('000')).toEqual({ type: 'block-height', value: '0' });
});

test('search__classify_search_query__address__address', () => {
  expect(classifySearchQuery('1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa')).toEqual({ type: 'address', value: '1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa' });
});

test('search__classify_search_query__text__unknown', () => {
  expect(classifySearchQuery('satoshi')).toEqual({ type: 'unknown' });
});
