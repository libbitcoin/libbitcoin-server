import type uPlot from 'uplot';
import { POOL_STALE_AFTER_MS } from './native-contract';
import type { ChartDefinition, ChartSeriesDefinition } from './chart-config';
import type { MetricsSnapshot, SignalBucketSnapshot, TelemetryBucket } from './live-metrics';
import { getMeasureDefinition, getSignalDefinition, type SignalUnit } from './signal-catalog';

export interface SelectedChartData {
  data: uPlot.AlignedData;
  unit: SignalUnit;
  hasObservedData: boolean;
  unobservedLabels: readonly string[];
}

interface ObservedValue {
  value: number;
  time: number;
}

function statFor(bucket: TelemetryBucket, signalId: string): SignalBucketSnapshot | undefined {
  return bucket.signals[signalId];
}

function totalBeforeRange(
  snapshot: MetricsSnapshot,
  signalId: string,
  cutoff: number,
  kind: 'count' | 'sum'
): number {
  let total =
    kind === 'count'
      ? (snapshot.priorSignalCounts[signalId] ?? 0)
      : (snapshot.priorSignalSums[signalId] ?? 0);
  for (const bucket of snapshot.buckets) {
    if (bucket.time >= cutoff) break;
    const stat = statFor(bucket, signalId);
    total += kind === 'count' ? (stat?.count ?? 0) : (stat?.sum ?? 0);
  }
  return total;
}

function latestBeforeRange(
  snapshot: MetricsSnapshot,
  signalId: string,
  cutoff: number
): ObservedValue | null {
  let latest: ObservedValue | null = snapshot.priorSignalLatest[signalId] ?? null;
  for (const bucket of snapshot.buckets) {
    if (bucket.time >= cutoff) break;
    const stat = statFor(bucket, signalId);
    if (stat?.last !== null && stat?.last !== undefined && stat.lastAt !== null) {
      latest = { value: stat.last, time: stat.lastAt };
    }
  }
  return latest;
}

function materializeTimeline(snapshot: MetricsSnapshot, cutoff: number): TelemetryBucket[] {
  const firstBucket =
    cutoff <= snapshot.sessionStartedAt
      ? Math.floor(snapshot.sessionStartedAt / 1_000) * 1_000
      : Math.ceil(cutoff / 1_000) * 1_000;
  const lastBucket = Math.floor(snapshot.now / 1_000) * 1_000;
  const bucketsByTime = new Map(snapshot.buckets.map((bucket) => [bucket.time, bucket]));
  const timeline: TelemetryBucket[] = [];

  for (let time = firstBucket; time <= lastBucket; time += 1_000) {
    timeline.push(bucketsByTime.get(time) ?? { time, signals: {} });
  }
  return timeline;
}

function bucketDurationSeconds(
  bucket: TelemetryBucket,
  now: number,
  sessionStartedAt: number
): number {
  const observedFrom = Math.max(bucket.time, sessionStartedAt);
  const observedUntil = Math.min(now, bucket.time + 1_000);
  return Math.max(0, (observedUntil - observedFrom) / 1_000);
}

function selectSeriesValues(
  snapshot: MetricsSnapshot,
  visible: readonly TelemetryBucket[],
  rangeStart: number,
  definition: ChartSeriesDefinition
): Array<number | null> {
  const signal = getSignalDefinition(definition.signalId);
  let cumulative =
    definition.measure === 'cumulative-occurrences'
      ? totalBeforeRange(snapshot, definition.signalId, rangeStart, 'count')
      : definition.measure === 'payload-cumulative'
        ? totalBeforeRange(snapshot, definition.signalId, rangeStart, 'sum')
        : 0;
  let previous = latestBeforeRange(snapshot, definition.signalId, rangeStart);

  return visible.map((bucket) => {
    const stat = statFor(bucket, definition.signalId);
    switch (definition.measure) {
      case 'occurrence-count': {
        return stat?.count ?? 0;
      }
      case 'occurrence-rate': {
        const duration = bucketDurationSeconds(bucket, snapshot.now, snapshot.sessionStartedAt);
        return duration > 0 ? (stat?.count ?? 0) / duration : 0;
      }
      case 'cumulative-occurrences': {
        cumulative += stat?.count ?? 0;
        return cumulative;
      }
      case 'payload-sum': {
        return stat && stat.samples > 0 ? stat.sum : null;
      }
      case 'payload-cumulative': {
        cumulative += stat?.sum ?? 0;
        return cumulative;
      }
      case 'payload-average': {
        return stat && stat.samples > 0 ? stat.sum / stat.samples : null;
      }
      case 'payload-minimum': {
        return stat?.minimum ?? null;
      }
      case 'payload-maximum': {
        return stat?.maximum ?? null;
      }
      case 'payload-latest': {
        if (stat?.last !== null && stat?.last !== undefined && stat.lastAt !== null) {
          previous = { value: stat.last, time: stat.lastAt };
          return stat.last;
        }
        if (signal.source === 'pool') {
          const at = Math.min(bucket.time + 1_000, snapshot.now);
          return previous && at - previous.time < POOL_STALE_AFTER_MS ? previous.value : null;
        }
        return signal.payloadKind === 'height' ? (previous?.value ?? null) : null;
      }
      case 'payload-change-rate': {
        if (stat?.last === null || stat?.last === undefined || stat.lastAt === null) return null;
        const baseline =
          previous ??
          (stat.first !== null && stat.firstAt !== null
            ? { value: stat.first, time: stat.firstAt }
            : null);
        previous = { value: stat.last, time: stat.lastAt };
        if (!baseline) return null;
        const elapsedSeconds = (stat.lastAt - baseline.time) / 1_000;
        return elapsedSeconds > 0 ? (stat.last - baseline.value) / elapsedSeconds : null;
      }
    }
  });
}

export function selectChartData(
  snapshot: MetricsSnapshot,
  chart: ChartDefinition
): SelectedChartData {
  const cutoff = snapshot.now - chart.range * 1_000;
  const visible = materializeTimeline(snapshot, cutoff);
  const rangeStart = visible[0]?.time ?? Math.floor(snapshot.now / 1_000) * 1_000;
  const firstSeries = chart.series[0];
  const unit = getMeasureDefinition(
    getSignalDefinition(firstSeries.signalId),
    firstSeries.measure
  ).unit;
  const unobservedLabels = chart.series
    .filter((item) => (snapshot.signalTotals[item.signalId] ?? 0) === 0)
    .map((item) => item.label ?? getSignalDefinition(item.signalId).label);

  return {
    data: [
      visible.map((bucket) => bucket.time / 1_000),
      ...chart.series.map((item) => selectSeriesValues(snapshot, visible, rangeStart, item)),
    ] as uPlot.AlignedData,
    unit,
    hasObservedData: unobservedLabels.length < chart.series.length,
    unobservedLabels,
  };
}

export function unitLabel(unit: SignalUnit): string {
  switch (unit) {
    case 'occurrences': {
      return 'occurrences';
    }
    case 'occurrences-per-second': {
      return 'occurrences/s';
    }
    case 'height': {
      return 'height';
    }
    case 'height-per-second': {
      return 'height/s';
    }
    case 'milliseconds': {
      return 'ms';
    }
    case 'count': {
      return 'value';
    }
    case 'transactions': {
      return 'transactions';
    }
    case 'vbytes': {
      return 'vB';
    }
    case 'satoshis': {
      return 'sat';
    }
    case 'sat-per-vbyte': {
      return 'sat/vB';
    }
  }
}

export function unitSuffix(unit: SignalUnit): string {
  switch (unit) {
    case 'occurrences-per-second': {
      return '/s';
    }
    case 'height-per-second': {
      return ' height/s';
    }
    case 'milliseconds': {
      return ' ms';
    }
    case 'transactions': {
      return ' tx';
    }
    case 'vbytes': {
      return ' vB';
    }
    case 'satoshis': {
      return ' sat';
    }
    case 'sat-per-vbyte': {
      return ' sat/vB';
    }
    default: {
      return '';
    }
  }
}

export function zeroBasedUnit(unit: SignalUnit): boolean {
  return unit !== 'height' && unit !== 'height-per-second';
}
