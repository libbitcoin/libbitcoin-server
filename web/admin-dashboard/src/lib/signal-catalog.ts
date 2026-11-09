import {
  EVENT_DEFINITIONS,
  LOG_LEVELS,
  type EventValueKind,
  eventValueInMilliseconds,
  getEventDefinition,
} from './admin-contract';

export type SignalSource = 'event' | 'log' | 'native' | 'pool';
export type SignalPayloadKind =
  | 'height'
  | 'duration'
  | 'count'
  | 'text'
  | 'unknown'
  | 'transactions'
  | 'vbytes'
  | 'satoshis'
  | 'fee-rate';
export type SignalUnit =
  | 'occurrences'
  | 'occurrences-per-second'
  | 'height'
  | 'height-per-second'
  | 'milliseconds'
  | 'count'
  | 'transactions'
  | 'vbytes'
  | 'satoshis'
  | 'sat-per-vbyte';
export type ChartType = 'line' | 'step' | 'area' | 'bar';
export type SignalMeasure =
  | 'occurrence-count'
  | 'occurrence-rate'
  | 'cumulative-occurrences'
  | 'payload-latest'
  | 'payload-average'
  | 'payload-minimum'
  | 'payload-maximum'
  | 'payload-sum'
  | 'payload-cumulative'
  | 'payload-change-rate';

export interface MeasureDefinition {
  id: SignalMeasure;
  label: string;
  description: string;
  unit: SignalUnit;
  chartTypes: readonly ChartType[];
}

export interface SignalDefinition {
  id: string;
  source: SignalSource;
  sourceId: number | 'top' | 'count' | 'virtual' | 'fee';
  name: string;
  label: string;
  category: string;
  payloadKind: SignalPayloadKind;
  allowedMeasures: readonly SignalMeasure[];
  defaultMeasure: SignalMeasure;
  description: string;
}

const occurrenceCharts = ['line', 'area', 'bar'] as const;
const cumulativeCharts = ['line', 'step', 'area'] as const;
const gaugeCharts = ['line', 'step', 'area'] as const;

export const MEASURE_DEFINITIONS: Readonly<Record<SignalMeasure, MeasureDefinition>> = {
  'occurrence-count': {
    id: 'occurrence-count',
    label: 'Occurrences per bucket',
    description: 'Number of frames observed in each one-second bucket.',
    unit: 'occurrences',
    chartTypes: occurrenceCharts,
  },
  'occurrence-rate': {
    id: 'occurrence-rate',
    label: 'Occurrence rate',
    description: 'Frames observed per second.',
    unit: 'occurrences-per-second',
    chartTypes: occurrenceCharts,
  },
  'cumulative-occurrences': {
    id: 'cumulative-occurrences',
    label: 'Cumulative occurrences',
    description: 'Running number of frames observed during this browser session.',
    unit: 'occurrences',
    chartTypes: cumulativeCharts,
  },
  'payload-latest': {
    id: 'payload-latest',
    label: 'Latest value',
    description: 'Most recently observed payload value.',
    unit: 'count',
    chartTypes: gaugeCharts,
  },
  'payload-average': {
    id: 'payload-average',
    label: 'Average value',
    description: 'Average payload value within each one-second bucket.',
    unit: 'count',
    chartTypes: ['line', 'area', 'bar'],
  },
  'payload-minimum': {
    id: 'payload-minimum',
    label: 'Minimum value',
    description: 'Minimum payload value within each one-second bucket.',
    unit: 'count',
    chartTypes: ['line', 'step'],
  },
  'payload-maximum': {
    id: 'payload-maximum',
    label: 'Maximum value',
    description: 'Maximum payload value within each one-second bucket.',
    unit: 'count',
    chartTypes: ['line', 'step'],
  },
  'payload-sum': {
    id: 'payload-sum',
    label: 'Value per bucket',
    description: 'Sum of count payloads within each one-second bucket.',
    unit: 'count',
    chartTypes: ['line', 'area', 'bar'],
  },
  'payload-cumulative': {
    id: 'payload-cumulative',
    label: 'Cumulative value',
    description: 'Running sum of count payloads during this browser session.',
    unit: 'count',
    chartTypes: cumulativeCharts,
  },
  'payload-change-rate': {
    id: 'payload-change-rate',
    label: 'Height change rate',
    description: 'Change in the latest observed height per second.',
    unit: 'height-per-second',
    chartTypes: occurrenceCharts,
  },
};

const occurrenceMeasures = [
  'occurrence-rate',
  'occurrence-count',
  'cumulative-occurrences',
] as const satisfies readonly SignalMeasure[];

const heightMeasures = [
  'payload-latest',
  'payload-maximum',
  'payload-change-rate',
  ...occurrenceMeasures,
] as const satisfies readonly SignalMeasure[];

const durationMeasures = [
  'payload-average',
  'payload-latest',
  'payload-minimum',
  'payload-maximum',
  ...occurrenceMeasures,
] as const satisfies readonly SignalMeasure[];

const countMeasures = [
  'payload-sum',
  'payload-cumulative',
  'payload-latest',
  'payload-average',
  ...occurrenceMeasures,
] as const satisfies readonly SignalMeasure[];

const gaugeMeasures = [
  'payload-latest',
  'payload-average',
  'payload-minimum',
  'payload-maximum',
] as const satisfies readonly SignalMeasure[];

export const POOL_SIGNALS = [
  {
    field: 'count',
    id: 'pool:count',
    label: 'Pool transactions',
    payloadKind: 'transactions',
    description: 'Current confirmable transaction count in the pool.',
  },
  {
    field: 'virtual',
    id: 'pool:vsize',
    label: 'Pool virtual size',
    payloadKind: 'vbytes',
    description: 'Total virtual size of the current pool, in vbytes.',
  },
  {
    field: 'fee',
    id: 'pool:total_fee',
    label: 'Pool total fees',
    payloadKind: 'satoshis',
    description: 'Total fees of the current pool, in satoshis.',
  },
] as const;

export function eventSignalId(event: number): string {
  return `event:${String(event)}`;
}

export function logSignalId(level: number): string {
  return `log:${String(level)}`;
}

export const NATIVE_TOP_SIGNAL_ID = 'native:top';

function eventPayloadKind(valueKind: EventValueKind): SignalPayloadKind {
  switch (valueKind) {
    case 'height': {
      return 'height';
    }
    case 'seconds':
    case 'milliseconds':
    case 'microseconds': {
      return 'duration';
    }
    case 'count': {
      return 'count';
    }
    case 'fee-rate': {
      return 'fee-rate';
    }
    default: {
      return 'unknown';
    }
  }
}

function measuresForPayload(payloadKind: SignalPayloadKind): readonly SignalMeasure[] {
  switch (payloadKind) {
    case 'height': {
      return heightMeasures;
    }
    case 'duration': {
      return durationMeasures;
    }
    case 'count': {
      return countMeasures;
    }
    case 'fee-rate': {
      return [...gaugeMeasures, ...occurrenceMeasures];
    }
    default: {
      return occurrenceMeasures;
    }
  }
}

function defaultMeasureForPayload(payloadKind: SignalPayloadKind): SignalMeasure {
  switch (payloadKind) {
    case 'height': {
      return 'payload-latest';
    }
    case 'duration': {
      return 'payload-average';
    }
    case 'count': {
      return 'payload-sum';
    }
    default: {
      return 'occurrence-rate';
    }
  }
}

function createEventSignal(event: number): SignalDefinition {
  const definition = getEventDefinition(event);
  const payloadKind = eventPayloadKind(definition.valueKind);
  return {
    id: eventSignalId(event),
    source: 'event',
    sourceId: event,
    name: definition.name,
    label: definition.label,
    category: definition.category,
    payloadKind,
    allowedMeasures: measuresForPayload(payloadKind),
    defaultMeasure: defaultMeasureForPayload(payloadKind),
    description:
      event === 10
        ? 'Newly archived transactions, including compact-block recovery. Occurrences measure archival, not all received transactions. Fee rate is normalized to sat/vB.'
        : payloadKind === 'unknown'
          ? 'Payload semantics are unknown; occurrence measurements are safe.'
          : `${definition.label} event with a ${payloadKind} payload.`,
  };
}

const declaredSignals: SignalDefinition[] = [
  ...POOL_SIGNALS.map((signal): SignalDefinition => ({
    id: signal.id,
    source: 'pool',
    sourceId: signal.field,
    name: signal.field,
    label: signal.label,
    category: 'pool',
    payloadKind: signal.payloadKind,
    allowedMeasures: gaugeMeasures,
    defaultMeasure: 'payload-latest',
    description: signal.description,
  })),
  ...EVENT_DEFINITIONS.map((definition) => createEventSignal(definition.id)),
  ...LOG_LEVELS.map((level) => ({
    id: logSignalId(level.id),
    source: 'log' as const,
    sourceId: level.id,
    name: level.name,
    label: `${level.label} logs`,
    category: 'logs',
    payloadKind: 'text' as const,
    allowedMeasures: occurrenceMeasures,
    defaultMeasure: 'occurrence-rate' as const,
    description: `${level.label} log message volume. Message text is not interpreted as numeric data.`,
  })),
  {
    id: NATIVE_TOP_SIGNAL_ID,
    source: 'native',
    sourceId: 'top',
    name: 'canonical_top',
    label: 'Canonical top',
    category: 'native',
    payloadKind: 'height',
    allowedMeasures: ['payload-latest', 'payload-maximum', 'payload-change-rate'],
    defaultMeasure: 'payload-latest',
    description: 'Canonical chain height reported by the native interface.',
  },
];

export const SIGNAL_DEFINITIONS: readonly SignalDefinition[] = declaredSignals;

const signalById = new Map(SIGNAL_DEFINITIONS.map((signal) => [signal.id, signal]));

export function getSignalDefinition(signalId: string): SignalDefinition {
  const declared = signalById.get(signalId);
  if (declared) return declared;

  const match = /^event:(\d+)$/u.exec(signalId);
  if (match) return createEventSignal(Number(match[1]));

  return {
    id: signalId,
    source: 'event',
    sourceId: -1,
    name: signalId,
    label: signalId,
    category: 'unknown',
    payloadKind: 'unknown',
    allowedMeasures: occurrenceMeasures,
    defaultMeasure: 'occurrence-rate',
    description: 'Unknown signal; only occurrence measurements are available.',
  };
}

export function getMeasureDefinition(
  signal: SignalDefinition,
  measure: SignalMeasure
): MeasureDefinition {
  const base = MEASURE_DEFINITIONS[measure];
  if (measure.startsWith('occurrence')) return base;
  if (measure === 'cumulative-occurrences') return base;

  const unit =
    measure === 'payload-change-rate'
      ? 'height-per-second'
      : signal.payloadKind === 'height'
        ? 'height'
        : signal.payloadKind === 'duration'
          ? 'milliseconds'
          : signal.payloadKind === 'fee-rate'
            ? 'sat-per-vbyte'
            : signal.payloadKind === 'transactions' ||
                signal.payloadKind === 'vbytes' ||
                signal.payloadKind === 'satoshis'
              ? signal.payloadKind
              : 'count';
  return { ...base, unit };
}

export function isMeasureAllowed(signal: SignalDefinition, measure: SignalMeasure): boolean {
  return signal.allowedMeasures.includes(measure);
}

export function normalizeEventPayload(event: number, value: number): number {
  if (getEventDefinition(event).valueKind === 'fee-rate') return value / 1_000;
  return eventValueInMilliseconds(event, value) ?? value;
}
