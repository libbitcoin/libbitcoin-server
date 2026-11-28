import { z } from 'zod';
import {
  MEASURE_DEFINITIONS,
  NATIVE_TOP_SIGNAL_ID,
  eventSignalId,
  getMeasureDefinition,
  getSignalDefinition,
  isMeasureAllowed,
  type SignalMeasure,
  type SignalUnit,
} from './signal-catalog';

export const MAX_CHARTS = 32;
export const MAX_CHART_SERIES = 8;
export const MAX_CHART_TITLE_LENGTH = 80;
export const CHART_RANGES = [300, 900, 3600] as const;
export const CHART_TYPES = ['line', 'step', 'area', 'bar'] as const;
const CHART_SCALES = ['linear', 'compressed'] as const;
export const CHART_COLORS = [
  '#ffa022',
  '#38bdf8',
  '#34d399',
  '#c084fc',
  '#fb7185',
  '#e879f9',
  '#facc15',
  '#60a5fa',
] as const;

export type ChartScale = (typeof CHART_SCALES)[number];

const signalMeasureSchema = z.enum([
  'occurrence-count',
  'occurrence-rate',
  'cumulative-occurrences',
  'payload-latest',
  'payload-average',
  'payload-minimum',
  'payload-maximum',
  'payload-sum',
  'payload-cumulative',
  'payload-change-rate',
]);

const chartSeriesDefinitionSchema = z
  .object({
    id: z.string().min(1).max(100),
    signalId: z.string().min(1).max(100),
    measure: signalMeasureSchema,
    color: z.string().regex(/^#[\da-f]{6}$/iu),
    label: z.string().max(80).optional(),
  })
  .strict();

const chartDefinitionSchema = z
  .object({
    id: z.string().min(1).max(100),
    title: z.string().trim().min(1).max(MAX_CHART_TITLE_LENGTH),
    visualization: z.enum(CHART_TYPES),
    scale: z.enum(CHART_SCALES).default('linear'),
    range: z.union([z.literal(300), z.literal(900), z.literal(3600)]),
    series: z.array(chartSeriesDefinitionSchema).min(1).max(MAX_CHART_SERIES),
  })
  .strict();

const poolHistogramSchema = z
  .object({
    id: z.string().min(1).max(100),
    title: z.string().trim().min(1).max(MAX_CHART_TITLE_LENGTH),
    visualization: z.literal('histogram'),
    scale: z.enum(CHART_SCALES).default('linear'),
    color: z.string().regex(/^#[\da-f]{6}$/iu),
  })
  .strict();

const dashboardChartSchema = z.union([chartDefinitionSchema, poolHistogramSchema]);

const chartLayoutSchema = z
  .object({
    version: z.literal(1),
    charts: z.array(dashboardChartSchema).max(MAX_CHARTS),
  })
  .strict();

export type ChartSeriesDefinition = z.infer<typeof chartSeriesDefinitionSchema>;
export type ChartDefinition = z.infer<typeof chartDefinitionSchema>;
type PoolHistogramDefinition = z.infer<typeof poolHistogramSchema>;
export type DashboardChartDefinition = ChartDefinition | PoolHistogramDefinition;

export function isTimeSeriesChart(chart: DashboardChartDefinition): chart is ChartDefinition {
  return chart.visualization !== 'histogram';
}

export interface ChartTemplate {
  id: string;
  label: string;
  description: string;
  chart: DashboardChartDefinition;
}

const syncPipeline: ChartDefinition = {
  id: 'default-sync-pipeline',
  title: 'Sync pipeline',
  visualization: 'area',
  scale: 'compressed',
  range: 300,
  series: [
    series('sync-header', eventSignalId(0), 'occurrence-rate', CHART_COLORS[3]),
    series('sync-archive', eventSignalId(3), 'occurrence-rate', CHART_COLORS[0]),
    series('sync-validate', eventSignalId(5), 'occurrence-rate', CHART_COLORS[1]),
    series('sync-confirm', eventSignalId(6), 'occurrence-rate', CHART_COLORS[2]),
  ],
};

const chainProgress: ChartDefinition = {
  id: 'default-chain-progress',
  title: 'Chain progress',
  visualization: 'step',
  scale: 'linear',
  range: 900,
  series: [
    series('progress-top', NATIVE_TOP_SIGNAL_ID, 'payload-latest', CHART_COLORS[0]),
    series('progress-organized', eventSignalId(13), 'payload-latest', CHART_COLORS[2]),
    series('progress-confirmed', eventSignalId(6), 'payload-latest', CHART_COLORS[1]),
  ],
};

const poolTransactions: ChartDefinition = {
  id: 'default-pool-transactions',
  title: 'Pool transactions',
  visualization: 'step',
  scale: 'linear',
  range: 900,
  series: [series('pool-count', 'pool:count', 'payload-latest', CHART_COLORS[0])],
};

const poolTotalFees: ChartDefinition = {
  ...poolTransactions,
  id: 'default-pool-fees',
  title: 'Pool total fees',
  series: [series('pool-fees', 'pool:total_fee', 'payload-latest', CHART_COLORS[3])],
};

const poolVirtualSize: ChartDefinition = {
  ...poolTransactions,
  id: 'default-pool-virtual-size',
  title: 'Pool virtual size',
  series: [series('pool-size', 'pool:vsize', 'payload-latest', CHART_COLORS[1])],
};

const transactionArchiveRate: ChartDefinition = {
  id: 'default-transaction-archive-rate',
  title: 'Transactions archived / second',
  visualization: 'line',
  scale: 'linear',
  range: 900,
  series: [series('tx-archived', eventSignalId(10), 'occurrence-rate', CHART_COLORS[2])],
};

const blockLifecycle: ChartDefinition = {
  id: 'default-block-lifecycle',
  title: 'Block lifecycle detail',
  visualization: 'line',
  scale: 'compressed',
  range: 300,
  series: [
    series('lifecycle-header-organized', eventSignalId(1), 'occurrence-rate', CHART_COLORS[3]),
    series('lifecycle-buffered', eventSignalId(4), 'occurrence-rate', CHART_COLORS[0]),
    series('lifecycle-validate-bypassed', eventSignalId(8), 'occurrence-rate', CHART_COLORS[1]),
    series('lifecycle-confirm-bypassed', eventSignalId(9), 'occurrence-rate', CHART_COLORS[2]),
    series('lifecycle-organized', eventSignalId(13), 'occurrence-rate', CHART_COLORS[6]),
    series('lifecycle-tx-archived', eventSignalId(10), 'occurrence-rate', CHART_COLORS[7]),
    series('lifecycle-tx-validated', eventSignalId(11), 'occurrence-rate', CHART_COLORS[5]),
  ],
};

const chainEvents: ChartDefinition = {
  id: 'default-chain-events',
  title: 'Chain and peer events',
  visualization: 'bar',
  scale: 'linear',
  range: 900,
  series: [
    series('events-header-reorganized', eventSignalId(2), 'occurrence-count', CHART_COLORS[3]),
    series('events-block-reorganized', eventSignalId(14), 'occurrence-count', CHART_COLORS[2]),
    series('events-unconfirmable', eventSignalId(7), 'occurrence-count', CHART_COLORS[4]),
    series('events-tx-invalidated', eventSignalId(12), 'occurrence-count', CHART_COLORS[5]),
    series('events-tx-dropped', eventSignalId(41), 'occurrence-count', CHART_COLORS[0]),
    series('events-tx-refused', eventSignalId(42), 'occurrence-count', CHART_COLORS[1]),
    series('events-tx-unserved', eventSignalId(43), 'occurrence-count', CHART_COLORS[7]),
    series('events-template-issued', eventSignalId(15), 'occurrence-count', CHART_COLORS[6]),
  ],
};

const topLatency: ChartDefinition = {
  id: 'default-top-latency',
  title: 'Top latency',
  visualization: 'line',
  scale: 'compressed',
  range: 3600,
  series: [
    series('top-header', eventSignalId(28), 'payload-average', CHART_COLORS[0]),
    series('top-validate', eventSignalId(27), 'payload-average', CHART_COLORS[1]),
  ],
};

const poolAdmission: ChartDefinition = {
  id: 'default-pool-admission',
  title: 'Pool admission',
  visualization: 'line',
  scale: 'compressed',
  range: 900,
  series: [
    series('admission-submit', eventSignalId(30), 'payload-average', CHART_COLORS[0]),
    series('admission-coverage', eventSignalId(31), 'payload-average', CHART_COLORS[1]),
    series('admission-connect', eventSignalId(32), 'payload-average', CHART_COLORS[2]),
    series('admission-admit', eventSignalId(33), 'payload-average', CHART_COLORS[3]),
  ],
};

const poolBlocks: ChartDefinition = {
  id: 'default-pool-blocks',
  title: 'Pool block and template handling',
  visualization: 'line',
  scale: 'compressed',
  range: 900,
  series: [
    series('pool-block-in', eventSignalId(35), 'payload-maximum', CHART_COLORS[2]),
    series('pool-block-out', eventSignalId(36), 'payload-maximum', CHART_COLORS[4]),
    series('pool-compact', eventSignalId(34), 'payload-maximum', CHART_COLORS[1]),
    series('pool-context', eventSignalId(37), 'payload-maximum', CHART_COLORS[3]),
    series('pool-template', eventSignalId(38), 'payload-maximum', CHART_COLORS[6]),
    series('pool-project', eventSignalId(39), 'payload-maximum', CHART_COLORS[0]),
    series('pool-list', eventSignalId(40), 'payload-maximum', CHART_COLORS[5]),
  ],
};

const poolRows: ChartDefinition = {
  id: 'default-pool-rows',
  title: 'Pool rows',
  visualization: 'step',
  scale: 'linear',
  range: 900,
  series: [series('pool-rows', eventSignalId(29), 'payload-latest', CHART_COLORS[5])],
};

const servingLatency: ChartDefinition = {
  id: 'default-serving-latency',
  title: 'Serving latency',
  visualization: 'line',
  scale: 'compressed',
  range: 900,
  series: [
    series('serve-block', eventSignalId(19), 'payload-average', CHART_COLORS[0]),
    series('serve-ancestry', eventSignalId(20), 'payload-average', CHART_COLORS[1]),
    series('serve-filter', eventSignalId(21), 'payload-average', CHART_COLORS[2]),
    series('serve-filter-hashes', eventSignalId(22), 'payload-average', CHART_COLORS[3]),
    series('serve-filter-checkpoints', eventSignalId(23), 'payload-average', CHART_COLORS[4]),
  ],
};

const signatureBatches: ChartDefinition = {
  id: 'default-signature-batches',
  title: 'Signature batches',
  visualization: 'line',
  scale: 'compressed',
  range: 900,
  series: [
    series('batch-ecdsa', eventSignalId(24), 'payload-average', CHART_COLORS[0]),
    series('batch-schnorr', eventSignalId(25), 'payload-average', CHART_COLORS[1]),
    series('batch-silent', eventSignalId(26), 'payload-average', CHART_COLORS[2]),
  ],
};

const storeOperations: ChartDefinition = {
  id: 'default-store-operations',
  title: 'Store operations',
  visualization: 'step',
  scale: 'compressed',
  range: 3600,
  series: [
    series('store-snapshot', eventSignalId(16), 'payload-maximum', CHART_COLORS[0]),
    series('store-prune', eventSignalId(17), 'payload-maximum', CHART_COLORS[1]),
    series('store-reload', eventSignalId(18), 'payload-maximum', CHART_COLORS[2]),
  ],
};

const poolHistogram: PoolHistogramDefinition = {
  id: 'default-pool-fee-histogram',
  title: 'Pool fee distribution',
  visualization: 'histogram',
  scale: 'linear',
  color: CHART_COLORS[0],
};

export const CHART_TEMPLATES: readonly ChartTemplate[] = [
  {
    id: 'pool-fee-histogram',
    label: 'Pool fee distribution',
    description: 'Current pool virtual size grouped by fee rate.',
    chart: poolHistogram,
  },
  {
    id: 'sync-pipeline',
    label: 'Sync pipeline',
    description: 'Header and block lifecycle throughput.',
    chart: syncPipeline,
  },
  {
    id: 'chain-progress',
    label: 'Chain progress',
    description: 'Canonical, organized, and confirmed heights.',
    chart: chainProgress,
  },
  {
    id: 'pool-transactions',
    label: 'Pool transactions',
    description: 'Current pool transaction count, sampled every five seconds.',
    chart: poolTransactions,
  },
  {
    id: 'transaction-archive-rate',
    label: 'Transaction archive rate',
    description: 'Newly archived transactions per second, including compact-block recovery.',
    chart: transactionArchiveRate,
  },
  {
    id: 'pool-virtual-size',
    label: 'Pool virtual size',
    description: 'Current pool virtual size in vbytes.',
    chart: poolVirtualSize,
  },
  {
    id: 'pool-fees',
    label: 'Pool total fees',
    description: 'Total fees currently in the pool, in satoshis.',
    chart: poolTotalFees,
  },
  {
    id: 'block-lifecycle',
    label: 'Block lifecycle detail',
    description: 'Rates of the header, block and transaction events not in the sync pipeline.',
    chart: blockLifecycle,
  },
  {
    id: 'chain-events',
    label: 'Chain and peer events',
    description: 'Reorganizations, unconfirmable blocks, invalid transactions, peer drops, refused and unserved transactions, and issued templates.',
    chart: chainEvents,
  },
  {
    id: 'top-latency',
    label: 'Top latency',
    description: 'Candidate header to confirmed top, and block validation, in milliseconds.',
    chart: topLatency,
  },
  {
    id: 'pool-admission',
    label: 'Pool admission',
    description: 'Per transaction set: submit, coverage, script connect and index write.',
    chart: poolAdmission,
  },
  {
    id: 'pool-blocks',
    label: 'Pool block and template handling',
    description: 'Per block and template: organize, reorganize, compact fill, context sweep, template, projection and listing.',
    chart: poolBlocks,
  },
  {
    id: 'pool-rows',
    label: 'Pool rows',
    description: 'Pool table rows at each organized block.',
    chart: poolRows,
  },
  {
    id: 'serving-latency',
    label: 'Serving latency',
    description: 'Time to serve blocks, ancestry, filters, filter hashes and filter checkpoints.',
    chart: servingLatency,
  },
  {
    id: 'signature-batches',
    label: 'Signature batches',
    description: 'ECDSA and Schnorr batch verification and silent payment scan times.',
    chart: signatureBatches,
  },
  {
    id: 'store-operations',
    label: 'Store operations',
    description: 'Snapshot, prune and reload times.',
    chart: storeOperations,
  },
  {
    id: 'transaction-fee-rate',
    label: 'Archived transaction fee rate',
    description: 'Average fee rate of newly archived transactions, in sat/vB.',
    chart: {
      ...transactionArchiveRate,
      id: 'template-tx-fees',
      title: 'Archived transaction fee rate',
      scale: 'compressed',
      series: [series('tx-fee-rate', eventSignalId(10), 'payload-average', CHART_COLORS[0])],
    },
  },
];

const STORAGE_KEY = 'libbitcoinAdminDashboardCharts';
let idSequence = 0;

function series(
  id: string,
  signalId: string,
  measure: SignalMeasure,
  color: string
): ChartSeriesDefinition {
  return { id, signalId, measure, color };
}

function createId(prefix: string): string {
  idSequence += 1;
  return `${prefix}-${Date.now().toString(36)}-${String(idSequence)}`;
}

function cloneChart(chart: DashboardChartDefinition, replaceIds = false): DashboardChartDefinition {
  if (!isTimeSeriesChart(chart)) {
    return { ...chart, id: replaceIds ? createId('chart') : chart.id };
  }
  return {
    ...chart,
    id: replaceIds ? createId('chart') : chart.id,
    series: chart.series.map((item) => ({
      ...item,
      id: replaceIds ? createId('series') : item.id,
    })),
  };
}

export function defaultChartDefinitions(): DashboardChartDefinition[] {
  return [
    syncPipeline,
    chainProgress,
    poolTransactions,
    poolTotalFees,
    poolVirtualSize,
    poolHistogram,
    blockLifecycle,
    chainEvents,
    topLatency,
    poolAdmission,
    poolBlocks,
    poolRows,
    servingLatency,
    signatureBatches,
    storeOperations,
  ].map((chart) => cloneChart(chart));
}

export function createChartFromTemplate(templateId: string): DashboardChartDefinition {
  const template = CHART_TEMPLATES.find((item) => item.id === templateId);
  return cloneChart(template?.chart ?? syncPipeline, true);
}

export function createCustomChart(): ChartDefinition {
  return {
    id: createId('chart'),
    title: 'Custom chart',
    visualization: 'line',
    scale: 'linear',
    range: 900,
    series: [
      {
        id: createId('series'),
        signalId: eventSignalId(3),
        measure: 'occurrence-rate',
        color: CHART_COLORS[0],
      },
    ],
  };
}

export function duplicateChart(chart: DashboardChartDefinition): DashboardChartDefinition {
  const suffix = ' copy';
  const title = `${chart.title.slice(0, MAX_CHART_TITLE_LENGTH - suffix.length).trimEnd()}${suffix}`;
  return { ...cloneChart(chart, true), title };
}

export function chartAxisUnit(chart: DashboardChartDefinition): SignalUnit | null {
  if (!isTimeSeriesChart(chart)) return 'vbytes';
  const units = new Set(
    chart.series.map(
      (item) => getMeasureDefinition(getSignalDefinition(item.signalId), item.measure).unit
    )
  );
  return units.size === 1 ? ([...units][0] ?? null) : null;
}

export function chartValidationError(chart: DashboardChartDefinition): string | null {
  const parsed = dashboardChartSchema.safeParse(chart);
  if (!parsed.success) return parsed.error.issues[0]?.message ?? 'Invalid chart configuration';
  if (!isTimeSeriesChart(chart)) return null;

  for (const item of chart.series) {
    const signal = getSignalDefinition(item.signalId);
    if (!isMeasureAllowed(signal, item.measure)) {
      return `${MEASURE_DEFINITIONS[item.measure].label} is not valid for ${signal.label}.`;
    }
    const measure = getMeasureDefinition(signal, item.measure);
    if (!measure.chartTypes.includes(chart.visualization)) {
      return `${measure.label} cannot be displayed as a ${chart.visualization} chart.`;
    }
  }

  if (chartAxisUnit(chart) === null) return 'All series on a chart must use compatible units.';
  return null;
}

export function parseChartDefinitions(value: unknown): DashboardChartDefinition[] | null {
  const parsed = chartLayoutSchema.safeParse(value);
  if (!parsed.success) return null;
  if (new Set(parsed.data.charts.map((chart) => chart.id)).size !== parsed.data.charts.length) {
    return null;
  }
  if (
    parsed.data.charts.some(
      (chart) =>
        isTimeSeriesChart(chart) &&
        new Set(chart.series.map((item) => item.id)).size !== chart.series.length
    )
  ) {
    return null;
  }
  return parsed.data.charts.every((chart) => chartValidationError(chart) === null)
    ? parsed.data.charts
    : null;
}

function browserStorage(): Storage | null {
  try {
    const window_ = (globalThis as unknown as { window?: Window }).window;
    return window_?.localStorage ?? null;
  } catch {
    return null;
  }
}

export function loadChartDefinitions(
  storage: Storage | null = browserStorage()
): DashboardChartDefinition[] {
  try {
    const stored = storage?.getItem(STORAGE_KEY);
    if (!stored) return defaultChartDefinitions();
    return parseChartDefinitions(JSON.parse(stored) as unknown) ?? defaultChartDefinitions();
  } catch {
    return defaultChartDefinitions();
  }
}

export function saveChartDefinitions(
  charts: readonly DashboardChartDefinition[],
  storage: Storage | null = browserStorage()
): void {
  try {
    const layout = { version: 1, charts };
    if (!parseChartDefinitions(layout)) return;
    storage?.setItem(STORAGE_KEY, JSON.stringify(layout));
  } catch {
    // Storage may be disabled or full; the in-memory layout remains usable.
  }
}
