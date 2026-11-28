import { describe, expect, test } from 'vitest';
import type { MiningChannel } from '@/lib/admin-contract';
import { FLEET, FLEET_COLORS, OTHER, assignSlots, canPin, fleetSeries, retain } from '@/lib/mining/fleet';

function channel(id: string, hashrate: number, history: [number, number][] = []): MiningChannel {
  return {
    id,
    service: 'stratum_v1',
    transport: 'tcp',
    address: '127.0.0.1:1',
    client: '',
    workers: [id],
    connected: 0,
    difficulty: 1,
    hashrate,
    accepted: 0,
    rejected: 0,
    rejections: {},
    last_share: null,
    shares: [],
    history,
  };
}

// Channels d0..d(count-1), d0 the highest hash rate.
function fleet(count: number): MiningChannel[] {
  return Array.from({ length: count }, (_, index) => channel(`d${String(index)}`, 1000 - index));
}

const none = new Set<string>();

describe('assignSlots', () => {
  test('colors the channels of highest hash rate', () => {
    const slots = assignSlots({}, fleet(10), none);
    expect(Object.keys(slots).sort()).toEqual(['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7']);
    expect(new Set(Object.values(slots)).size).toBe(FLEET_COLORS);
  });

  test('keeps slots as hash rates change', () => {
    const first = assignSlots({}, fleet(10), none);
    const changed = fleet(10).map((entry) => (entry.id === 'd9' ? { ...entry, hashrate: 5000 } : entry));
    expect(assignSlots(first, changed, none)).toBe(first);
  });

  test('frees the slot of a removed channel for the next highest', () => {
    const first = assignSlots({}, fleet(10), none);
    const second = assignSlots(first, fleet(10).filter((entry) => entry.id !== 'd3'), none);
    expect(second.d3).toBeUndefined();
    expect(second.d8).toBe(first.d3);
  });

  test('a pinned channel takes the slot of the lowest unpinned colored channel', () => {
    const first = assignSlots({}, fleet(10), none);
    const second = assignSlots(first, fleet(10), new Set(['d9']));
    expect(second.d9).toBe(first.d7);
    expect(second.d7).toBeUndefined();
  });

  test('a pinned channel is not displaced by another pin', () => {
    const pins = new Set(['d9', 'd8']);
    const first = assignSlots(assignSlots({}, fleet(10), new Set(['d9'])), fleet(10), pins);
    expect(first.d9).toBeDefined();
    expect(first.d8).toBeDefined();
    expect(first.d7).toBeUndefined();
    expect(first.d6).toBeUndefined();
  });

  test('canPin is false only when all colors are pinned', () => {
    const channels = fleet(10);
    const pins = new Set(channels.slice(0, FLEET_COLORS).map((entry) => entry.id));
    const slots = assignSlots({}, channels, pins);
    expect(canPin('d9', slots, pins)).toBe(false);
    expect(canPin('d0', slots, pins)).toBe(true);
    expect(canPin('d9', slots, new Set(['d0']))).toBe(true);
  });
});

describe('retain', () => {
  test('drops the ids of channels no longer connected', () => {
    const keys = new Set(['d0', 'd9', 'gone', OTHER, FLEET]);
    expect([...retain(keys, fleet(10))].sort()).toEqual(['d0', 'd9', FLEET, OTHER].sort());
  });

  test('returns the keys object when nothing is dropped', () => {
    const keys = new Set(['d0', OTHER]);
    expect(retain(keys, fleet(10))).toBe(keys);
  });
});

describe('fleetSeries', () => {
  const minute = 60_000;

  test('sums the channels on the union of their minutes', () => {
    const channels = [
      channel('a', 3, [
        [0, 1],
        [minute, 2],
      ]),
      channel('b', 2, [
        [minute, 3],
        [2 * minute, 4],
      ]),
    ];
    const series = fleetSeries(channels, assignSlots({}, channels, none), none);
    expect(series.times).toEqual([0, minute, 2 * minute]);
    expect(series.total).toEqual([1, 5, 4]);
    expect(series.current).toBe(5);
    expect(series.bands.map((band) => band.key)).toEqual(['a', 'b']);
    expect(series.others).toBe(0);
  });

  test('sums the uncolored channels as other, below the colored', () => {
    const channels = fleet(10).map((entry) => ({ ...entry, history: [[0, 1]] as [number, number][] }));
    const series = fleetSeries(channels, assignSlots({}, channels, none), none);
    expect(series.others).toBe(2);
    expect(series.bands[0]).toMatchObject({ key: OTHER, rates: [2], current: 991 + 992 });
    expect(series.bands).toHaveLength(FLEET_COLORS + 1);
    expect(series.total).toEqual([10]);
  });

  test('leaves hidden channels and a hidden other out of the total', () => {
    const channels = fleet(10).map((entry) => ({ ...entry, history: [[0, 1]] as [number, number][] }));
    const slots = assignSlots({}, channels, none);
    const series = fleetSeries(channels, slots, new Set(['d0', OTHER]));
    expect(series.bands.map((band) => band.key)).not.toContain('d0');
    expect(series.bands.map((band) => band.key)).not.toContain(OTHER);
    expect(series.total).toEqual([7]);
    expect(series.others).toBe(2);
  });
});
