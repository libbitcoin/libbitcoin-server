import uPlot from 'uplot';
import { useEffect, useLayoutEffect, useMemo, useRef } from 'react';
import 'uplot/dist/uPlot.min.css';
import type { ChartType } from '@/lib/signal-catalog';
import type { ChartScale } from '@/lib/chart-config';
import { compressedAxisSplits } from '@/lib/chart-scale';

export interface ChartSeries {
  label: string;
  color: string;
  fill?: string;
  width?: number;
  points?: boolean;
}
interface TimeSeriesChartProps {
  data: uPlot.AlignedData;
  series: readonly ChartSeries[];
  height?: number;
  emptyMessage?: string;
  valueSuffix?: string;
  zeroBased?: boolean;
  valueFormatter?: (value: number) => string;
  visualization?: ChartType;
  scale?: ChartScale;
  forceEmpty?: boolean;
}

export function TimeSeriesChart({
  data,
  series,
  height = 280,
  emptyMessage = 'Waiting for live data…',
  valueSuffix = '',
  zeroBased = true,
  valueFormatter = formatAxisValue,
  visualization = 'line',
  scale = 'linear',
  forceEmpty = false,
}: TimeSeriesChartProps) {
  const containerReference = useRef<HTMLDivElement>(null);
  const chartReference = useRef<uPlot | null>(null);
  const isZoomedReference = useRef(false);
  const zoomReference = useRef<{ min: number; max: number } | null>(null);
  const normalizedData = useMemo(() => ensureTwoPoints(data), [data]);
  const latestDataReference = useRef(normalizedData);
  const hasData = !forceEmpty && data[0]?.length > 0;

  useLayoutEffect(() => {
    latestDataReference.current = normalizedData;
  }, [normalizedData]);

  useLayoutEffect(() => {
    const container = containerReference.current;
    if (!container) return;

    const theme = getComputedStyle(container);
    const axisFont = `12px ${theme.fontFamily}`;
    const options: uPlot.Options = {
      width: Math.max(240, container.clientWidth),
      height,
      padding: [12, 8, 0, 0],
      cursor: {
        drag: { x: true, y: false, setScale: true },
        focus: { prox: 24 },
      },
      hooks: {
        setSelect: [
          (chart) => {
            if (chart.select.width > 0) isZoomedReference.current = true;
          },
        ],
      },
      legend: {
        show: true,
        live: true,
      },
      scales: {
        x: { time: true },
        y: {
          auto: true,
          // Asinh is linear near zero and logarithmic for large magnitudes.
          // Keep raw data so legends, gaps, and bar baselines retain their meaning.
          distr: scale === 'compressed' ? 4 : 1,
          asinh: 1,
          ...(zeroBased
            ? {
                range: (_self: uPlot, minimum: number, maximum: number) => [
                  Math.min(0, minimum),
                  maximum === 0 ? 1 : maximum * 1.08,
                ],
              }
            : {
                range: (_self: uPlot, minimum: number, maximum: number) => {
                  const span = maximum - minimum;
                  const padding =
                    span === 0 ? Math.max(1, Math.abs(maximum) * 0.0005) : span * 0.08;
                  return [minimum - padding, maximum + padding];
                },
              }),
        },
      },
      axes: [
        {
          stroke: theme.color,
          grid: { stroke: '#c8563733', width: 1 },
          ticks: { stroke: '#ffa0224d', width: 1 },
          font: axisFont,
        },
        {
          size: 72,
          stroke: theme.color,
          grid: { stroke: '#c8563733', width: 1 },
          ticks: { stroke: '#ffa0224d', width: 1 },
          font: axisFont,
          values: (_self, values: Array<number | null>) =>
            values.map((value) =>
              value === null ? null : `${valueFormatter(value)}${valueSuffix}`
            ),
          ...(scale === 'compressed'
            ? {
                splits: compressedAxisSplits,
                filter: (_self: uPlot, splits: number[]) => splits,
              }
            : {}),
        },
      ],
      series: [
        {},
        ...series.map((item, seriesIndex) => {
          const paths =
            visualization === 'step'
              ? uPlot.paths.stepped?.({ align: 1 })
              : visualization === 'bar'
                ? groupedBarPaths(seriesIndex, series.length)
                : undefined;
          return {
            label: item.label,
            stroke: item.color,
            fill:
              visualization === 'area' || visualization === 'bar'
                ? (item.fill ?? `${item.color}24`)
                : item.fill,
            width: item.width ?? 2,
            paths,
            points: { show: visualization === 'line' ? (item.points ?? false) : false },
            value: (_self: uPlot, value: number | null) =>
              value === null ? '—' : `${valueFormatter(value)}${valueSuffix}`,
          };
        }),
      ],
    };

    const chart = new uPlot(options, latestDataReference.current, container);
    const zoom = zoomReference.current;
    if (zoom) chart.setScale('x', zoom);
    chartReference.current = chart;
    const observer = new ResizeObserver((entries) => {
      const width = entries[0]?.contentRect.width;
      if (width && width > 0) chart.setSize({ width: Math.round(width), height });
    });
    observer.observe(container);

    return () => {
      zoomReference.current =
        isZoomedReference.current && chart.scales.x.min != null && chart.scales.x.max != null
          ? { min: chart.scales.x.min, max: chart.scales.x.max }
          : null;
      observer.disconnect();
      chart.destroy();
      chartReference.current = null;
    };
  }, [height, series, valueFormatter, valueSuffix, visualization, zeroBased, scale]);

  useEffect(() => {
    const chart = chartReference.current;
    if (!chart) return;

    const shouldResetScales = !isZoomedReference.current;
    chart.setData(normalizedData, shouldResetScales);
    if (!shouldResetScales) chart.redraw();
  }, [normalizedData]);

  return (
    <div className="relative min-w-0">
      <div ref={containerReference} className="text-textMuted min-w-0 overflow-hidden" />
      {!hasData && (
        <div className="bg-surface text-textMuted pointer-events-none absolute inset-0 z-10 flex items-center justify-center text-sm">
          {emptyMessage}
        </div>
      )}
    </div>
  );
}

function groupedBarPaths(seriesIndex: number, seriesCount: number) {
  const count = Math.max(1, seriesCount);

  return uPlot.paths.bars?.({
    size: [1, 48 / count, 0],
    disp: {
      x0: {
        unit: 1,
        values: (chart) => {
          const layout = groupedBarLayout(chart, count);
          const offset = -layout.groupWidth / 2 + layout.barWidth * seriesIndex;
          return Array.from(chart.data[0], (value) => value + offset);
        },
      },
      size: {
        unit: 1,
        values: (chart) => [groupedBarLayout(chart, count).barWidth],
      },
    },
  });
}

function groupedBarLayout(chart: uPlot, seriesCount: number) {
  const columnWidth = minimumPositiveSpacing(chart.data[0]);
  const maximumGroupWidth = Math.abs(chart.posToVal(48, 'x') - chart.posToVal(0, 'x'));
  const groupWidth = Math.min(columnWidth * 0.72, maximumGroupWidth);

  return {
    groupWidth,
    barWidth: groupWidth / seriesCount,
  };
}

function minimumPositiveSpacing(values: Iterable<number>): number {
  let minimum = Number.POSITIVE_INFINITY;
  let previous: number | undefined;

  for (const value of values) {
    if (!Number.isFinite(value)) continue;
    if (previous !== undefined) {
      const spacing = Math.abs(value - previous);
      if (spacing > 0 && spacing < minimum) minimum = spacing;
    }
    previous = value;
  }

  return Number.isFinite(minimum) ? minimum : 1;
}

function ensureTwoPoints(data: uPlot.AlignedData): uPlot.AlignedData {
  const xValues = data[0];
  if (xValues.length >= 2) return data;

  const nowSeconds = Date.now() / 1_000;
  const first = xValues[0] ?? nowSeconds;
  const x = [first - 1, first];
  const values = data.slice(1).map((series) => {
    const value = series[0] ?? null;
    return [value, value];
  });
  return [x, ...values] as uPlot.AlignedData;
}

function formatAxisValue(value: number): string {
  if (Math.abs(value) >= 1_000_000) return `${(value / 1_000_000).toFixed(1)}m`;
  if (Math.abs(value) >= 1_000) return `${(value / 1_000).toFixed(1)}k`;
  if (Math.abs(value) >= 10) return value.toFixed(0);
  if (Math.abs(value) >= 1) return value.toFixed(1);
  return value.toFixed(2);
}
