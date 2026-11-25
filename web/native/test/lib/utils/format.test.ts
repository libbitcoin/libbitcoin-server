import { expect, test } from 'vitest';
import {
  formatBtc,
  formatBytes,
  formatHash,
  formatHex,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatShortRelativeTime,
  satsToBtc,
} from '@/lib/utils/format';

const NOW = 1_700_000_000_000;
const NOW_SECONDS = 1_700_000_000;

test('format__format_number__null__dash', () => {
  expect(formatNumber(null)).toBe('—');
});

test('format__format_number__fraction__two_digits_grouped', () => {
  expect(formatNumber(1234.5678)).toBe('1,234.57');
});

test('format__format_number__options__applied', () => {
  expect(formatNumber(1234.5678, { maximumFractionDigits: 0 })).toBe('1,235');
});

test('format__format_hash__long__elided', () => {
  expect(formatHash('0123456789abcdef0123456789', 4)).toBe('0123…6789');
});

test('format__format_hash__short__unchanged', () => {
  expect(formatHash('01234567', 4)).toBe('01234567');
});

test('format__format_hash__empty__dash', () => {
  expect(formatHash('')).toBe('—');
});

test('format__sats_to_btc__sats__btc', () => {
  expect(satsToBtc(150_000_000)).toBe(1.5);
});

test('format__sats_to_btc__undefined__undefined', () => {
  expect(satsToBtc(undefined)).toBeUndefined();
});

test('format__format_sats__sats__grouped_sats', () => {
  expect(formatSats(1_000_000)).toBe('1,000,000 sats');
});

test('format__format_btc__value__eight_digits', () => {
  expect(formatBtc(0.5)).toBe('0.50000000 btc');
});

test('format__format_bytes__under_kilobyte__bytes', () => {
  expect(formatBytes(999)).toBe('999 bytes');
});

test('format__format_bytes__kilobyte__kilobytes', () => {
  expect(formatBytes(1_550)).toBe('1.6 kB');
});

test('format__format_bytes__megabyte__megabytes', () => {
  expect(formatBytes(1_234_567)).toBe('1.23 MB');
});

test('format__format_relative_time__zero__dash', () => {
  expect(formatRelativeTime(0, NOW)).toBe('—');
});

test('format__format_relative_time__seconds_ago__seconds', () => {
  expect(formatRelativeTime(NOW_SECONDS - 30, NOW)).toBe('30 seconds ago');
});

test('format__format_relative_time__minutes_ago__minutes', () => {
  expect(formatRelativeTime(NOW_SECONDS - 300, NOW)).toBe('5 minutes ago');
});

test('format__format_relative_time__hours_ago__hours', () => {
  expect(formatRelativeTime(NOW_SECONDS - 7_200, NOW)).toBe('2 hours ago');
});

test('format__format_relative_time__one_day_ago__yesterday', () => {
  expect(formatRelativeTime(NOW_SECONDS - 86_400, NOW)).toBe('yesterday');
});

test('format__format_relative_time__minute_ahead__in_minute', () => {
  expect(formatRelativeTime(NOW_SECONDS + 60, NOW)).toBe('in 1 minute');
});

test('format__format_hex__value__prefixed_lower_hex', () => {
  expect(formatHex(255)).toBe('0xff');
});

test('format__format_short_relative_time__missing__dash', () => {
  expect(formatShortRelativeTime(undefined, 0)).toBe('—');
});

test('format__format_short_relative_time__seconds_past__secs_ago', () => {
  expect(formatShortRelativeTime(1000, 1_030_000)).toBe('30 secs ago');
});

test('format__format_short_relative_time__one_minute_past__min_ago', () => {
  expect(formatShortRelativeTime(1000, 1_060_000)).toBe('1 min ago');
});

test('format__format_short_relative_time__minutes_past__mins_ago', () => {
  expect(formatShortRelativeTime(1000, 1000 * 1000 + 38 * 60_000)).toBe('38 mins ago');
});

test('format__format_short_relative_time__hours_future__in_hrs', () => {
  expect(formatShortRelativeTime(1000 + 2 * 3600, 1_000_000)).toBe('in 2 hrs');
});

test('format__format_short_relative_time__days_past__days_ago', () => {
  expect(formatShortRelativeTime(1000, 1_000_000 + 3 * 86_400_000)).toBe('3 days ago');
});
