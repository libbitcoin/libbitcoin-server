import { expect, test } from 'vitest';
import {
  addressPath,
  blockByHashPath,
  blockByHeightPath,
  blockValidationPath,
  configurationPath,
  feesPath,
  isAddressPath,
  searchPath,
  topPath,
  transactionInputPath,
  transactionOutputPath,
} from '@/lib/utils/routes';

test('routes__block_by_hash_path__hash__hash_path', () => {
  expect(blockByHashPath('00ab')).toBe('/v1/block/hash/00ab');
});

test('routes__block_validation_path__hash__header_context_path', () => {
  expect(blockValidationPath('00ab')).toBe('/v1/block/hash/00ab/header/context');
});

test('routes__configuration_path__default__configuration_path', () => {
  expect(configurationPath()).toBe('/v1/configuration');
});

test('routes__fees_path__default__fees_path', () => {
  expect(feesPath()).toBe('/v1/fees');
});

test('routes__top_path__default__top_path', () => {
  expect(topPath()).toBe('/v1/top');
});

test('routes__block_by_height_path__number__confirmed_path', () => {
  expect(blockByHeightPath(42)).toBe('/v1/block/confirmed/42');
});

test('routes__transaction_input_path__index__input_fragment', () => {
  expect(transactionInputPath('00ab', 3)).toBe('/v1/input/00ab/3');
});

test('routes__transaction_output_path__index__output_fragment', () => {
  expect(transactionOutputPath('00ab', 3)).toBe('/v1/output/00ab/3');
});

test('routes__address_path__reserved_characters__encoded', () => {
  expect(addressPath('a/b c')).toBe('/v1/address/a%2Fb%20c');
});

test('routes__is_address_path__address_path__true', () => {
  expect(isAddressPath('/v1/address/1abc')).toBe(true);
});

test('routes__is_address_path__address_root__false', () => {
  expect(isAddressPath('/v1/address')).toBe(false);
});

test('routes__is_address_path__block_path__false', () => {
  expect(isAddressPath('/v1/block/hash/00ab')).toBe(false);
});

test('routes__search_path__query__encoded_query', () => {
  expect(searchPath('a&b')).toBe('/v1/search?q=a%26b');
});
