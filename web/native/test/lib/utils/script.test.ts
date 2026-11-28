import { expect, test } from 'vitest';
import { isNullData } from '@/lib/utils/script';

test('isNullData is true of a null data script', () => {
  expect(isNullData('return [4f55543a39]')).toBe(true);
  expect(isNullData('return')).toBe(true);
});

test('isNullData is false of other scripts', () => {
  expect(isNullData('zero [84dbe2f37cfa8b1b41b7eddfce8638fbf61d7629]')).toBe(false);
  expect(isNullData('')).toBe(false);
  expect(isNullData(undefined)).toBe(false);
});
