import type { MetricsSnapshot, RecentEvent, RecentLog, TelemetryBucket } from './live-metrics';
import type { ConnectionStatus } from './reconnecting-websocket';
import type { LogQuery, LogWindow } from './log-window';
import type { EventQuery, EventWindow } from './event-window';
import type { NativeFees } from './native-contract';

export type TelemetryCommand =
  | { type: 'configure'; generation: number; endpoint: string; logFilter: number }
  | { type: 'reset'; generation: number }
  | { type: 'poll'; generation: number; requestId: number }
  | { type: 'pause-display'; generation: number; stream: 'logs' | 'events'; paused: boolean }
  | {
      type: 'poll-events';
      generation: number;
      requestId: number;
      queryVersion: number;
      query: EventQuery;
    }
  | {
      type: 'poll-logs';
      generation: number;
      requestId: number;
      queryVersion: number;
      query: LogQuery;
    }
  | { type: 'native-top'; generation: number; height: number; receivedAt: number }
  | { type: 'native-fees'; generation: number; fees: NativeFees; receivedAt: number }
  | { type: 'clear-logs' | 'clear-events'; generation: number }
  | { type: 'stop' };

export interface MetricsUpdate {
  summary: Omit<MetricsSnapshot, 'buckets' | 'recentLogs' | 'recentEvents'>;
  reset: boolean;
  firstBucketTime: number;
  buckets: readonly TelemetryBucket[];
  recentLogs: readonly RecentLog[] | null;
  recentEvents: readonly RecentEvent[] | null;
}

export type TelemetryResponse =
  | {
      type: 'events';
      generation: number;
      requestId: number;
      queryVersion: number;
      window: EventWindow;
    }
  | { type: 'logs'; generation: number; requestId: number; queryVersion: number; window: LogWindow }
  | {
      type: 'snapshot';
      generation: number;
      requestId: number;
      status: ConnectionStatus;
      update: MetricsUpdate;
    }
  | { type: 'stopped' };

export function createMetricsUpdate(
  current: MetricsSnapshot,
  previous: MetricsSnapshot | null
): MetricsUpdate {
  const { buckets, recentLogs, recentEvents, ...summary } = current;
  const priorBuckets = new Map(previous?.buckets.map((bucket) => [bucket.time, bucket]));
  return {
    summary,
    reset: previous === null,
    firstBucketTime: buckets[0]?.time ?? current.now,
    buckets: buckets.filter((bucket) => priorBuckets.get(bucket.time) !== bucket),
    recentLogs: recentLogs === previous?.recentLogs ? null : recentLogs,
    recentEvents: recentEvents === previous?.recentEvents ? null : recentEvents,
  };
}

export function applyMetricsUpdate(
  previous: MetricsSnapshot,
  update: MetricsUpdate
): MetricsSnapshot {
  const buckets = new Map(
    (update.reset ? [] : previous.buckets)
      .filter((bucket) => bucket.time >= update.firstBucketTime)
      .map((bucket) => [bucket.time, bucket])
  );
  for (const bucket of update.buckets) buckets.set(bucket.time, bucket);
  return {
    ...update.summary,
    buckets: [...buckets.values()],
    recentLogs: reuseRows(update.reset ? [] : previous.recentLogs, update.recentLogs),
    recentEvents: reuseRows(update.reset ? [] : previous.recentEvents, update.recentEvents),
  };
}

export function reuseRows<T extends { id: number }>(
  previous: readonly T[],
  incoming: readonly T[] | null
): readonly T[] {
  if (incoming === null) return previous;
  const prior = new Map(previous.map((row) => [row.id, row]));
  return incoming.map((row) => prior.get(row.id) ?? row);
}
