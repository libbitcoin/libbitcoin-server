import { expect, test } from 'vitest';
import { coinbaseText } from '@/lib/utils/coinbase';

const GENESIS =
  '[ffff001d] [04] [5468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73]';

test('coinbase__coinbase_text__genesis__all_data_ops', () => {
  expect(coinbaseText(GENESIS, false)).toEqual(['****', '*', 'The Times 03/Jan/2009 Chancellor on brink of second bailout for banks']);
});

test('coinbase__coinbase_text__bip34__ops_after_height', () => {
  expect(coinbaseText('[76e90e] [2f5669614254432f] [a1b2]', true)).toEqual(['/ViaBTC/', '**']);
});

test('coinbase__coinbase_text__non_bip34__all_data_ops', () => {
  expect(coinbaseText('[76e90e] [2f5669614254432f]', false)).toEqual(['v**', '/ViaBTC/']);
});

test('coinbase__coinbase_text__bip34_small_height_opcode__ops_after_height', () => {
  expect(coinbaseText('5 [2f5669614254432f]', true)).toEqual(['/ViaBTC/']);
});

test('coinbase__coinbase_text__prefixed_data_op__data', () => {
  expect(coinbaseText('[76e90e] [1.2f5669614254432f]', true)).toEqual(['/ViaBTC/']);
});

test('coinbase__coinbase_text__non_ascii__stars', () => {
  expect(coinbaseText('[41ff0042]', false)).toEqual(['A**B']);
});

test('coinbase__coinbase_text__opcodes__skipped', () => {
  expect(coinbaseText('nop [41424344] checksig', false)).toEqual(['ABCD']);
});

test('coinbase__coinbase_text__unparsed__data', () => {
  expect(coinbaseText('[76e90e] [c76535c200] (2f466f756e647279)', true)).toEqual(['*e5**', '/Foundry']);
});
