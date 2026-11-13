import {
  type AdminEventFrame,
  type AdminFrame,
  type AdminLogFrame,
  isAdminAckFrame,
  isAdminEventFrame,
  isAdminLogFrame,
} from './admin-contract';
import { POOL_STALE_AFTER_MS, type NativeFees, type NativeFeesSnapshot } from './native-contract';
import {
  NATIVE_TOP_SIGNAL_ID,
  POOL_SIGNALS,
  eventSignalId,
  logSignalId,
  normalizeEventPayload,
} from './signal-catalog';

const RETENTION_MS = 60 * 60 * 1_000;
const PUBLISH_INTERVAL_MS = 1_000;
const RECENT_LOG_LIMIT = 1_000;
const RECENT_EVENT_LIMIT = 1_000;

export interface NodeActivity {
  label:
    | 'Synchronizing headers'
    | 'Archiving blocks'
    | 'Validating blocks'
    | 'Confirming blocks'
    | 'Reorganizing'
    | 'Idle';
  rate: number;
}

export interface RecentLog extends AdminLogFrame {
  id: number;
  receivedAt: number;
}

export interface RecentEvent extends AdminEventFrame {
  id: number;
  receivedAt: number;
}

export interface SignalValueSnapshot {
  readonly value: number;
  readonly time: number;
}

export interface SignalBucketSnapshot {
  count: number;
  samples: number;
  sum: number;
  minimum: number | null;
  maximum: number | null;
  first: number | null;
  last: number | null;
  firstAt: number | null;
  lastAt: number | null;
}

export interface TelemetryBucket {
  time: number;
  signals: Readonly<Partial<Record<string, SignalBucketSnapshot>>>;
}

export interface MetricsSnapshot {
  now: number;
  sessionStartedAt: number;
  totalFrames: number;
  totalLogs: number;
  evictedLogs: number;
  clearedLogs: number;
  totalEvents: number;
  totalAcks: number;
  invalidFrames: number;
  buckets: readonly TelemetryBucket[];
  priorSignalCounts: Readonly<Partial<Record<string, number>>>;
  priorSignalSums: Readonly<Partial<Record<string, number>>>;
  priorSignalLatest: Readonly<Partial<Record<string, SignalValueSnapshot>>>;
  signalTotals: Readonly<Partial<Record<string, number>>>;
  signalSums: Readonly<Partial<Record<string, number>>>;
  signalLatest: Readonly<Partial<Record<string, SignalValueSnapshot>>>;
  signalMaximums: Readonly<Partial<Record<string, number>>>;
  recentLogs: readonly RecentLog[];
  recentEvents: readonly RecentEvent[];
  lastLogAt: number | null;
  lastEventAt: number | null;
  poolSnapshot: NativeFeesSnapshot | null;
}

interface MutableSignalBucket {
  count: number;
  samples: number;
  sum: number;
  minimum: number | null;
  maximum: number | null;
  first: number | null;
  last: number | null;
  firstAt: number | null;
  lastAt: number | null;
}

interface MutableBucket {
  time: number;
  signals: Map<string, MutableSignalBucket>;
  snapshot: TelemetryBucket | null;
}

class RingBuffer<T> {
  readonly #items: Array<T | undefined>;
  #next = 0;
  #size = 0;
  #snapshot: readonly T[] | null = null;

  constructor(readonly capacity: number) {
    this.#items = Array.from({ length: capacity });
  }

  get size(): number {
    return this.#size;
  }

  push(value: T): boolean {
    const evicted = this.#size === this.capacity;
    this.#snapshot = null;
    this.#items[this.#next] = value;
    this.#next = (this.#next + 1) % this.capacity;
    this.#size = Math.min(this.#size + 1, this.capacity);
    return evicted;
  }

  clear(): void {
    this.#snapshot = null;
    // eslint-disable-next-line unicorn/no-useless-undefined -- Array.fill requires a value in TypeScript.
    this.#items.fill(undefined);
    this.#next = 0;
    this.#size = 0;
  }

  newestFirst(): readonly T[] {
    if (this.#snapshot) return this.#snapshot;
    const output: T[] = [];
    for (let offset = 0; offset < this.#size; offset += 1) {
      const index = (this.#next - 1 - offset + this.capacity) % this.capacity;
      const value = this.#items[index];
      if (value !== undefined) output.push(value);
    }
    this.#snapshot = output;
    return output;
  }
}

type SnapshotListener = () => void;

function addToRecord(record: Partial<Record<string, number>>, key: string, value: number): void {
  record[key] = (record[key] ?? 0) + value;
}

function cloneSignalValues(
  record: Readonly<Partial<Record<string, SignalValueSnapshot>>>
): Partial<Record<string, SignalValueSnapshot>> {
  const clone: Partial<Record<string, SignalValueSnapshot>> = {};
  for (const [signalId, latest] of Object.entries(record)) {
    if (latest) clone[signalId] = { ...latest };
  }
  return clone;
}

function createMutableSignalBucket(): MutableSignalBucket {
  return {
    count: 0,
    samples: 0,
    sum: 0,
    minimum: null,
    maximum: null,
    first: null,
    last: null,
    firstAt: null,
    lastAt: null,
  };
}

export class LiveMetricsStore {
  readonly #now: () => number;
  readonly #autoPublish: boolean;
  #listeners = new Set<SnapshotListener>();
  #logs = new RingBuffer<RecentLog>(RECENT_LOG_LIMIT);
  #events = new RingBuffer<RecentEvent>(RECENT_EVENT_LIMIT);
  #buckets = new Map<number, MutableBucket>();
  #priorSignalCounts: Partial<Record<string, number>> = {};
  #priorSignalSums: Partial<Record<string, number>> = {};
  #priorSignalLatest: Partial<Record<string, SignalValueSnapshot>> = {};
  #signalTotals: Partial<Record<string, number>> = {};
  #signalSums: Partial<Record<string, number>> = {};
  #signalLatest: Partial<Record<string, SignalValueSnapshot>> = {};
  #signalMaximums: Partial<Record<string, number>> = {};
  #sessionStartedAt: number;
  #lastCollectionAt: number;
  #totalFrames = 0;
  #totalLogs = 0;
  #evictedLogs = 0;
  #clearedLogs = 0;
  #totalEvents = 0;
  #totalAcks = 0;
  #invalidFrames = 0;
  #lastLogAt: number | null = null;
  #lastEventAt: number | null = null;
  #poolSnapshot: MetricsSnapshot['poolSnapshot'] = null;
  #sequence = 0;
  #publishTimer: ReturnType<typeof setTimeout> | null = null;
  #clockTimer: ReturnType<typeof setInterval> | null = null;
  #snapshot: MetricsSnapshot;

  constructor(now: () => number = Date.now, autoPublish = true) {
    this.#now = now;
    this.#autoPublish = autoPublish;
    this.#sessionStartedAt = now();
    this.#lastCollectionAt = this.#sessionStartedAt;
    this.#ensureBucket(this.#sessionStartedAt);
    this.#snapshot = this.#createSnapshot();
  }

  readonly getSnapshot = (): MetricsSnapshot => this.#snapshot;

  // Stream panes can read the current rings without materializing chart history.
  readonly getRecentLogs = (): readonly RecentLog[] => this.#logs.newestFirst();
  readonly getRecentEvents = (): readonly RecentEvent[] => this.#events.newestFirst();

  readonly subscribe = (listener: SnapshotListener): (() => void) => {
    this.#listeners.add(listener);
    return () => {
      this.#listeners.delete(listener);
    };
  };

  start(): void {
    if (this.#clockTimer) return;
    this.tick();
    this.#clockTimer = setInterval(() => {
      this.tick();
    }, 1_000);
  }

  stop(): void {
    if (this.#clockTimer) {
      clearInterval(this.#clockTimer);
      this.#clockTimer = null;
    }
    if (this.#publishTimer) {
      clearTimeout(this.#publishTimer);
      this.#publishTimer = null;
    }
  }

  ingest(frame: AdminFrame, receivedAt = this.#now()): void {
    const collectionTime = this.#collectionTime(receivedAt);
    this.#totalFrames += 1;
    if (isAdminLogFrame(frame)) {
      this.#ingestLog(frame, collectionTime);
    } else if (isAdminEventFrame(frame)) {
      this.#ingestEvent(frame, collectionTime);
    } else if (isAdminAckFrame(frame)) {
      this.#totalAcks += 1;
    }
    this.#schedulePublish();
  }

  recordInvalidFrame(): void {
    this.#invalidFrames += 1;
    this.#schedulePublish();
  }

  recordNativeTop(height: number, receivedAt = this.#now()): void {
    const time = this.#collectionTime(receivedAt);
    const latest = this.#signalLatest[NATIVE_TOP_SIGNAL_ID];
    if (latest?.value === height && time - latest.time < 15_000) return;
    this.#recordSignal(NATIVE_TOP_SIGNAL_ID, time, height);
    this.#prune(time);
    this.#schedulePublish();
  }

  recordNativeFees(data: NativeFees, receivedAt = this.#now()): void {
    // Cached REST responses or repeated renders must not refresh an old sample.
    if (
      !Number.isFinite(receivedAt) ||
      receivedAt < this.#sessionStartedAt ||
      this.#now() - receivedAt >= POOL_STALE_AFTER_MS ||
      (this.#poolSnapshot !== null && receivedAt <= this.#poolSnapshot.receivedAt)
    )
      return;
    const time = this.#collectionTime(receivedAt);
    this.#poolSnapshot = { data, receivedAt };
    for (const signal of POOL_SIGNALS) {
      this.#recordSignal(signal.id, time, data[signal.field]);
    }
    this.#prune(time);
    this.#schedulePublish();
  }

  clearLogs(): void {
    this.#clearedLogs += this.#logs.size;
    this.#logs.clear();
    this.flush();
  }

  clearEvents(): void {
    this.#events.clear();
    this.flush();
  }

  reset(): void {
    this.#logs.clear();
    this.#events.clear();
    this.#buckets.clear();
    this.#priorSignalCounts = {};
    this.#priorSignalSums = {};
    this.#priorSignalLatest = {};
    this.#signalTotals = {};
    this.#signalSums = {};
    this.#signalLatest = {};
    this.#signalMaximums = {};
    this.#sessionStartedAt = this.#collectionTime(this.#now());
    this.#totalFrames = 0;
    this.#totalLogs = 0;
    this.#evictedLogs = 0;
    this.#clearedLogs = 0;
    this.#totalEvents = 0;
    this.#totalAcks = 0;
    this.#invalidFrames = 0;
    this.#lastLogAt = null;
    this.#lastEventAt = null;
    this.#poolSnapshot = null;
    this.#sequence = 0;
    this.#ensureBucket(this.#sessionStartedAt);
    this.flush();
  }

  tick(): void {
    const now = this.#collectionTime(this.#now());
    this.#ensureBucket(now);
    this.#prune(now);
    this.#schedulePublish();
  }

  flush(): void {
    if (this.#publishTimer) {
      clearTimeout(this.#publishTimer);
      this.#publishTimer = null;
    }
    this.#snapshot = this.#createSnapshot();
    for (const listener of this.#listeners) listener();
  }

  #ingestLog(frame: AdminLogFrame, receivedAt: number): void {
    this.#totalLogs += 1;
    this.#lastLogAt = receivedAt;
    this.#sequence += 1;
    if (this.#logs.push({ ...frame, id: this.#sequence, receivedAt })) this.#evictedLogs += 1;
    this.#recordSignal(logSignalId(frame.level), receivedAt, null);
  }

  #ingestEvent(frame: AdminEventFrame, receivedAt: number): void {
    this.#totalEvents += 1;
    this.#lastEventAt = receivedAt;
    this.#sequence += 1;
    this.#events.push({ ...frame, id: this.#sequence, receivedAt });
    this.#recordSignal(
      eventSignalId(frame.event),
      receivedAt,
      normalizeEventPayload(frame.event, frame.value)
    );
  }

  #collectionTime(candidate: number): number {
    if (!Number.isFinite(candidate)) return this.#lastCollectionAt;
    this.#lastCollectionAt = Math.max(this.#lastCollectionAt, candidate);
    return this.#lastCollectionAt;
  }

  #recordSignal(signalId: string, time: number, value: number | null): void {
    const bucket = this.#ensureBucket(time);
    bucket.snapshot = null;
    const signal = bucket.signals.get(signalId) ?? createMutableSignalBucket();
    signal.count += 1;
    addToRecord(this.#signalTotals, signalId, 1);

    if (value !== null) {
      signal.samples += 1;
      signal.sum += value;
      signal.minimum = signal.minimum === null ? value : Math.min(signal.minimum, value);
      signal.maximum = signal.maximum === null ? value : Math.max(signal.maximum, value);
      if (signal.firstAt === null || time < signal.firstAt) {
        signal.first = value;
        signal.firstAt = time;
      }
      if (signal.lastAt === null || time >= signal.lastAt) {
        signal.last = value;
        signal.lastAt = time;
      }
      addToRecord(this.#signalSums, signalId, value);
      const latest = this.#signalLatest[signalId];
      if (!latest || time >= latest.time) this.#signalLatest[signalId] = { value, time };
      this.#signalMaximums[signalId] = Math.max(this.#signalMaximums[signalId] ?? value, value);
    }

    bucket.signals.set(signalId, signal);
  }

  #ensureBucket(time: number): MutableBucket {
    const bucketTime = Math.floor(time / 1_000) * 1_000;
    const existing = this.#buckets.get(bucketTime);
    if (existing) return existing;
    const bucket: MutableBucket = { time: bucketTime, signals: new Map(), snapshot: null };
    this.#buckets.set(bucketTime, bucket);
    return bucket;
  }

  #prune(now: number): void {
    const cutoff = now - RETENTION_MS;
    for (const [time, bucket] of this.#buckets) {
      if (time >= cutoff) break;
      for (const [signalId, signal] of bucket.signals) {
        addToRecord(this.#priorSignalCounts, signalId, signal.count);
        addToRecord(this.#priorSignalSums, signalId, signal.sum);
        if (signal.last !== null && signal.lastAt !== null) {
          const latest = this.#priorSignalLatest[signalId];
          if (!latest || signal.lastAt >= latest.time) {
            this.#priorSignalLatest[signalId] = { value: signal.last, time: signal.lastAt };
          }
        }
      }
      this.#buckets.delete(time);
    }
  }

  #schedulePublish(): void {
    if (!this.#autoPublish || this.#publishTimer) return;
    this.#publishTimer = setTimeout(() => {
      this.#publishTimer = null;
      this.flush();
    }, PUBLISH_INTERVAL_MS);
  }

  #createSnapshot(): MetricsSnapshot {
    // Collection time is monotonic, so insertion order is chronological. Reuse
    // immutable closed buckets instead of cloning an hour of history each update.
    const buckets = [...this.#buckets.values()].map((bucket) => {
      bucket.snapshot ??= {
        time: bucket.time,
        signals: Object.fromEntries(
          [...bucket.signals].map(([signalId, signal]) => [signalId, { ...signal }])
        ),
      };
      return bucket.snapshot;
    });
    return {
      now: this.#collectionTime(this.#now()),
      sessionStartedAt: this.#sessionStartedAt,
      totalFrames: this.#totalFrames,
      totalLogs: this.#totalLogs,
      evictedLogs: this.#evictedLogs,
      clearedLogs: this.#clearedLogs,
      totalEvents: this.#totalEvents,
      totalAcks: this.#totalAcks,
      invalidFrames: this.#invalidFrames,
      buckets,
      priorSignalCounts: { ...this.#priorSignalCounts },
      priorSignalSums: { ...this.#priorSignalSums },
      priorSignalLatest: cloneSignalValues(this.#priorSignalLatest),
      signalTotals: { ...this.#signalTotals },
      signalSums: { ...this.#signalSums },
      signalLatest: cloneSignalValues(this.#signalLatest),
      signalMaximums: { ...this.#signalMaximums },
      recentLogs: this.#logs.newestFirst(),
      recentEvents: this.#events.newestFirst(),
      lastLogAt: this.#lastLogAt,
      lastEventAt: this.#lastEventAt,
      poolSnapshot: this.#poolSnapshot,
    };
  }
}

export function signalRateOverWindow(
  buckets: readonly TelemetryBucket[],
  signalId: string,
  now: number,
  seconds: number,
  startedAt = buckets[0]?.time ?? now
): number {
  if (seconds <= 0) return 0;
  const requestedStart = now - seconds * 1_000;
  const startsWithSession = requestedStart <= startedAt;
  const firstBucket = startsWithSession
    ? Math.floor(startedAt / 1_000) * 1_000
    : Math.ceil(requestedStart / 1_000) * 1_000;
  const observedStart = startsWithSession ? startedAt : firstBucket;
  const lastBucket = Math.floor(now / 1_000) * 1_000;
  const visible = buckets.filter(
    (bucket) => bucket.time >= firstBucket && bucket.time <= lastBucket
  );
  const total = visible.reduce((sum, bucket) => sum + (bucket.signals[signalId]?.count ?? 0), 0);
  const elapsedSeconds = Math.max(0, (now - observedStart) / 1_000);
  return elapsedSeconds > 0 ? total / elapsedSeconds : 0;
}

export function detectNodeActivity(snapshot: MetricsSnapshot, seconds: number): NodeActivity {
  const rate = (event: number) =>
    signalRateOverWindow(
      snapshot.buckets,
      eventSignalId(event),
      snapshot.now,
      seconds,
      snapshot.sessionStartedAt
    );
  const reorganizationRate = rate(2) + rate(14);
  if (reorganizationRate > 0) return { label: 'Reorganizing', rate: reorganizationRate };

  const candidates: NodeActivity[] = [
    { label: 'Confirming blocks', rate: rate(6) },
    { label: 'Validating blocks', rate: rate(5) },
    { label: 'Archiving blocks', rate: rate(3) },
    { label: 'Synchronizing headers', rate: Math.max(rate(0), rate(1)) },
  ];
  const activity = candidates.reduce((highest, candidate) =>
    candidate.rate > highest.rate ? candidate : highest
  );
  return activity.rate > 0 ? activity : { label: 'Idle', rate: 0 };
}
