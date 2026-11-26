import { afterEach, expect, test, vi } from 'vitest';
import {
  LOAD,
  MOVE,
  followHeight,
  followMove,
  followWhen,
  isOvertaken,
} from '@/lib/utils/template-follow';

afterEach(() => {
  vi.useRealTimers();
});

test('template_follow__is_overtaken__mined_while_open__true', () => {
  expect(isOvertaken({ height: 101, top: 100 }, 101, 101)).toBe(true);
});

test('template_follow__is_overtaken__not_yet_mined__false', () => {
  expect(isOvertaken({ height: 101, top: 100 }, 101, 100)).toBe(false);
});

test('template_follow__is_overtaken__mined_when_opened__false', () => {
  expect(isOvertaken({ height: 101, top: 101 }, 101, 102)).toBe(false);
});

test('template_follow__is_overtaken__other_height__false', () => {
  expect(isOvertaken({ height: 101, top: 100 }, 102, 102)).toBe(false);
});

test('template_follow__is_overtaken__unknown__false', () => {
  expect(isOvertaken(undefined, 101, 101)).toBe(false);
  expect(isOvertaken({ height: 101, top: 100 }, 101)).toBe(false);
});

test('template_follow__follow_height__overtaken__next_template', () => {
  expect(followHeight({ height: 101, top: 100 }, 101, 101)).toBe(102);
});

test('template_follow__follow_height__overtaken_by_several__template_above_top', () => {
  expect(followHeight({ height: 101, top: 100 }, 101, 103)).toBe(104);
});

test('template_follow__follow_height__not_overtaken__undefined', () => {
  expect(followHeight({ height: 101, top: 100 }, 101, 100)).toBeUndefined();
});

test('template_follow__follow_move__3d__move', () => {
  expect(followMove('3d', false, false)).toBe(MOVE);
});

test('template_follow__follow_move__2d__no_move', () => {
  expect(followMove('2d', false, false)).toBe(0);
});

test('template_follow__follow_move__hidden__switches_at_once', () => {
  expect(followMove('3d', true, false)).toBeNull();
});

test('template_follow__follow_move__reduced_motion__switches_at_once', () => {
  expect(followMove('3d', false, true)).toBeNull();
  expect(followMove('2d', false, true)).toBeNull();
});

// The times at which followWhen resolves, with the template loaded at a time
// (or never).
async function resolvedAt(move: number, loaded?: number): Promise<number> {
  vi.useFakeTimers();
  const load =
    loaded === undefined
      ? new Promise<void>(() => undefined)
      : new Promise<void>((resolve) => {
          setTimeout(resolve, loaded);
        });
  let at = -1;
  void followWhen(move, load).then(() => {
    at = Date.now();
  });
  const start = Date.now();
  for (let step = 0; step < 100 && at < 0; step++) await vi.advanceTimersByTimeAsync(50);
  return at - start;
}

test('template_follow__follow_when__loaded_during_move__after_move', async () => {
  expect(await resolvedAt(MOVE, 200)).toBe(MOVE);
});

test('template_follow__follow_when__loaded_after_move__when_loaded', async () => {
  expect(await resolvedAt(MOVE, MOVE + 500)).toBe(MOVE + 500);
});

test('template_follow__follow_when__never_loaded__load_after_move', async () => {
  expect(await resolvedAt(MOVE)).toBe(MOVE + LOAD);
});

test('template_follow__follow_when__no_move__when_loaded', async () => {
  expect(await resolvedAt(0, 300)).toBe(300);
});
