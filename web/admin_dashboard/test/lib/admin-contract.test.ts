import { expect, test } from 'vitest';
import { getLogLevel, isAdminMiningFrame, parseAdminFrame } from '@/lib/admin-contract';

test('admin_contract__get_log_level__fault__fault', () => {
  expect(getLogLevel(6).name).toBe('fault');
});

test('admin_contract__get_log_level__verbose__verbose', () => {
  expect(getLogLevel(9).name).toBe('verbose');
});

test('admin_contract__get_log_level__unlisted__numbered', () => {
  expect(getLogLevel(8)).toEqual({ id: 8, name: 'level_8', label: 'Level 8' });
});

const CHANNEL = {
  id: '0000000000000001:0',
  service: 'stratum_v2',
  transport: 'ws',
  address: '[127.0.0.1]:42',
  client: 'libbitcoin-web/1.0',
  workers: ['browser'],
  connected: 1764200000000,
  difficulty: 0.02,
  hashrate: 0,
  accepted: 1,
  rejected: 0,
  rejections: {},
  last_share: 1764200001000,
  shares: [{ time: 1764200001000, height: 42, difficulty: 0.02, result: 'accepted' }],
  history: [[1764199980000, 1.5]],
};

test('admin_contract__parse_admin_frame__mining__mining_frame', () => {
  const frame = parseAdminFrame(JSON.stringify({ channels: [CHANNEL], blocks: [] }));
  expect(frame).not.toBeNull();
  expect(frame !== null && isAdminMiningFrame(frame)).toBe(true);
});

test('admin_contract__parse_admin_frame__mining_without_last_share__mining_frame', () => {
  const frame = parseAdminFrame(JSON.stringify({ channels: [{ ...CHANNEL, last_share: null, shares: [] }], blocks: [] }));
  expect(frame !== null && isAdminMiningFrame(frame)).toBe(true);
});

test('admin_contract__parse_admin_frame__mining_invalid_channel__null', () => {
  expect(parseAdminFrame(JSON.stringify({ channels: [{ id: 'x' }], blocks: [] }))).toBeNull();
});
