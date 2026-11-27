import { expect, test } from 'vitest';
import { inferAddressType } from '@/lib/utils/address';
import { isAddress, MAINNET_PARAMS } from '@/lib/api/providers/libbitcoin-mapper';

const TESTNET_PARAMS = { p2pkh: 0x6f, p2sh: 0xc4, hrp: 'tb' };

test('address__infer_address_type__null__undefined', () => {
  expect(inferAddressType(null)).toBeUndefined();
});

test('address__infer_address_type__whitespace__undefined', () => {
  expect(inferAddressType('  ')).toBeUndefined();
});

test('address__infer_address_type__p2pkh__p2pkh', () => {
  expect(inferAddressType('1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa')).toBe('p2pkh');
});

test('address__infer_address_type__testnet_p2pkh__p2pkh', () => {
  expect(inferAddressType('mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn')).toBe('p2pkh');
});

test('address__infer_address_type__p2sh__p2sh', () => {
  expect(inferAddressType('3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')).toBe('p2sh');
});

test('address__infer_address_type__invalid_base58__undefined', () => {
  expect(inferAddressType('1A1zP1eP5QGefi2DMPTfTL5SLmv7Divf0l')).toBeUndefined();
});

test('address__infer_address_type__short_base58__undefined', () => {
  expect(inferAddressType('1A1zP1eP5QGefi2DMPTf')).toBeUndefined();
});

test('address__infer_address_type__p2wpkh__p2wpkh', () => {
  expect(inferAddressType('bc1qar0srrr7xfkvy5l643lydnw9re59gtzzwf5mdq')).toBe('p2wpkh');
});

test('address__infer_address_type__upper_p2wpkh__p2wpkh', () => {
  expect(inferAddressType('BC1QAR0SRRR7XFKVY5L643LYDNW9RE59GTZZWF5MDQ')).toBe('p2wpkh');
});

test('address__infer_address_type__p2wsh__p2wsh', () => {
  expect(inferAddressType('bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3')).toBe('p2wsh');
});

test('address__infer_address_type__p2tr__p2tr', () => {
  expect(inferAddressType('bc1p5d7rjq7g6rdk2yhzks9smlaqtedr4dekq08ge8ztwac72sfr9rusxg3297')).toBe('p2tr');
});

test('address__infer_address_type__extended_key__undefined', () => {
  expect(inferAddressType('xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8')).toBeUndefined();
});

test('address__is_address__mainnet_p2pkh_mainnet__true', () => {
  expect(isAddress('1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa', MAINNET_PARAMS)).toBe(true);
});

test('address__is_address__mainnet_p2wpkh_mainnet__true', () => {
  expect(isAddress('bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4', MAINNET_PARAMS)).toBe(true);
});

test('address__is_address__testnet_p2wpkh_mainnet__false', () => {
  expect(isAddress('tb1qy58y5sll5j6nrrcz08kk0ja9wp97smqlk3mvzj', MAINNET_PARAMS)).toBe(false);
});

test('address__is_address__testnet_p2wpkh_testnet__true', () => {
  expect(isAddress('tb1qy58y5sll5j6nrrcz08kk0ja9wp97smqlk3mvzj', TESTNET_PARAMS)).toBe(true);
});

test('address__is_address__mainnet_p2pkh_testnet__false', () => {
  expect(isAddress('1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa', TESTNET_PARAMS)).toBe(false);
});

test('address__is_address__bad_checksum_mainnet__false', () => {
  expect(isAddress('1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNb', MAINNET_PARAMS)).toBe(false);
});

test('address__is_address__name_mainnet__false', () => {
  expect(isAddress('browser', MAINNET_PARAMS)).toBe(false);
});
