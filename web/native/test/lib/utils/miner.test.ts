import { expect, test } from 'vitest';
import { identifyMiner } from '@/lib/utils/miner';

const BLOCKFILLS_TAG_UPPER = '2f424c4f434b46494c4c53504f4f4c2f';
const FOUNDRY_TAG_TEXT =
  '4d696e656420627920466f756e6472792055534120506f6f6c2023313233';
const UNTAGGED = '6e6f7468696e672068657265';
const FOUNDRY_ADDRESS = 'bc1qxhmdufsvnuaaaer4ynz88fspdsxq2h9e9cetdj';
const ULTIMUS_ADDRESS = '1EMVSMe1VJUuqv7D7SFzctnVXk4KdjXATi';

test('miner__identify_miner__tag_other_case__pool', () => {
  expect(identifyMiner(BLOCKFILLS_TAG_UPPER, [])).toEqual({ name: 'BlockFills', link: 'https://www.blockfills.com/mining' });
});

test('miner__identify_miner__tag_within_text__pool', () => {
  expect(identifyMiner(FOUNDRY_TAG_TEXT, [])).toEqual({ name: 'Foundry USA', link: 'https://foundrydigital.com' });
});

test('miner__identify_miner__payout_address__pool', () => {
  expect(identifyMiner(UNTAGGED, [FOUNDRY_ADDRESS])).toEqual({ name: 'Foundry USA', link: 'https://foundrydigital.com' });
});

test('miner__identify_miner__address_and_tag_of_different_pools__earlier_listed_pool', () => {
  expect(identifyMiner(FOUNDRY_TAG_TEXT, [ULTIMUS_ADDRESS])?.name).toBe('ULTIMUSPOOL');
});

test('miner__identify_miner__unknown__undefined', () => {
  expect(identifyMiner(UNTAGGED, ['1BoatSLRHtKNngkdXEeobR76b53LETtpyT'])).toBeUndefined();
});

test('miner__identify_miner__empty__undefined', () => {
  expect(identifyMiner('', [])).toBeUndefined();
});
