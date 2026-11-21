import { expect, test } from 'vitest';
import { ALL_LOG_FILTER } from '@/lib/admin-contract';
import type { RecentLog } from '@/lib/live-metrics';
import { selectLogWindow } from '@/lib/log-window';

const APPLICATION = 0;
const NEWS = 1;
const FAULT = 6;

function log(id: number, level: number, message: string): RecentLog {
  return { id, level, message, time: id, receivedAt: id };
}

function logs(count: number): RecentLog[] {
  return Array.from({ length: count }, (_, id) => log(id, NEWS, `news ${id}`));
}

test('log_window__select_log_window__empty__empty_window', () => {
  expect(selectLogWindow([], { search: '', filter: ALL_LOG_FILTER, offset: 0 })).toEqual({ logs: [], retained: 0, matched: 0, offset: 0 });
});

test('log_window__select_log_window__filter__matching_levels', () => {
  const window = selectLogWindow([log(1, APPLICATION, 'a'), log(2, FAULT, 'b')], { search: '', filter: 1 << FAULT, offset: 0 });
  expect(window.logs.map((item) => item.id)).toEqual([2]);
  expect(window.retained).toBe(2);
  expect(window.matched).toBe(1);
});

test('log_window__select_log_window__message_search__case_insensitive_match', () => {
  const window = selectLogWindow([log(1, NEWS, 'Block Organized'), log(2, NEWS, 'peer')], { search: ' block ', filter: ALL_LOG_FILTER, offset: 0 });
  expect(window.logs.map((item) => item.id)).toEqual([1]);
});

test('log_window__select_log_window__level_name_search__level_match', () => {
  const window = selectLogWindow([log(1, NEWS, 'x'), log(2, FAULT, 'y')], { search: 'fault', filter: ALL_LOG_FILTER, offset: 0 });
  expect(window.logs.map((item) => item.id)).toEqual([2]);
});

test('log_window__select_log_window__over_limit__first_forty', () => {
  const window = selectLogWindow(logs(50), { search: '', filter: ALL_LOG_FILTER, offset: 0 });
  expect(window.logs.length).toBe(40);
  expect(window.logs[0].id).toBe(0);
});

test('log_window__select_log_window__offset_past_end__last_full_window', () => {
  const window = selectLogWindow(logs(50), { search: '', filter: ALL_LOG_FILTER, offset: 45 });
  expect(window.offset).toBe(10);
  expect(window.logs[0].id).toBe(10);
});

test('log_window__select_log_window__fractional_offset__floored', () => {
  expect(selectLogWindow(logs(50), { search: '', filter: ALL_LOG_FILTER, offset: 3.7 }).offset).toBe(3);
});

test('log_window__select_log_window__not_finite_offset__zero', () => {
  expect(selectLogWindow(logs(50), { search: '', filter: ALL_LOG_FILTER, offset: Number.NaN }).offset).toBe(0);
});
