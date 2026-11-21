import { expect, test } from 'vitest';
import { getLogLevel } from '@/lib/admin-contract';

test('admin_contract__get_log_level__fault__fault', () => {
  expect(getLogLevel(6).name).toBe('fault');
});

test('admin_contract__get_log_level__verbose__verbose', () => {
  expect(getLogLevel(9).name).toBe('verbose');
});

test('admin_contract__get_log_level__unlisted__numbered', () => {
  expect(getLogLevel(8)).toEqual({ id: 8, name: 'level_8', label: 'Level 8' });
});
