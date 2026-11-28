import { expect, test } from 'vitest';
import { LiveMetricsStore } from '@/lib/live-metrics';

// Header archived events (0) of consecutive values from one.
function ingestHeaders(store: LiveMetricsStore, count: number): void {
  for (let value = 1; value <= count; value += 1) store.ingest({ event: 0, value, time: value });
}

test('live_metrics__recent_events__busy_category__quiet_category_retained', () => {
  const store = new LiveMetricsStore(() => 0, false);
  store.ingest({ event: 3, value: 42, time: 0 });
  ingestHeaders(store, 1_005);
  const events = store.getRecentEvents();
  expect(events.filter((event) => event.event === 0)).toHaveLength(1_000);
  expect(events.filter((event) => event.event === 3)).toHaveLength(1);
  expect(events[0]?.value).toBe(1_005);
});

test('live_metrics__recent_logs__busy_level__fault_retained', () => {
  const store = new LiveMetricsStore(() => 0, false);
  store.ingest({ level: 6, message: 'fault', time: 0 });
  ingestLogs(store, 1_005);
  const logs = store.getRecentLogs();
  expect(logs.filter((log) => log.level === 6)).toHaveLength(1);
  expect(logs.filter((log) => log.level === 1)).toHaveLength(1_000);
});

// News logs (1) at consecutive times from one.
function ingestLogs(store: LiveMetricsStore, count: number): void {
  for (let time = 1; time <= count; time += 1) store.ingest({ level: 1, message: 'news', time });
}
