import { useLayoutEffect, useMemo, useRef } from 'react';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import type { ChartScale } from '@/lib/chart-config';
import { compressedAxisSplits } from '@/lib/chart-scale';
import {
  feeHistogramBins,
  formatFeeRate,
  formatVirtualSize,
  type FeeHistogramBin,
} from '@/lib/fee-histogram';
import { formatInteger, formatTimestamp } from '@/lib/format';
import { POOL_STALE_AFTER_MS, type NativeFeesSnapshot } from '@/lib/native-contract';

interface PoolFeeHistogramProps {
  snapshot: NativeFeesSnapshot | null;
  now: number;
  color: string;
  scale: ChartScale;
}

export function PoolFeeHistogram({ snapshot, now, color, scale }: PoolFeeHistogramProps) {
  const bins = useMemo(() => feeHistogramBins(snapshot?.data.histogram ?? []), [snapshot]);
  const stale = snapshot !== null && now - snapshot.receivedAt >= POOL_STALE_AFTER_MS;
  const message =
    snapshot === null
      ? 'Waiting for transaction-pool fee data…'
      : stale
        ? 'Fee distribution is stale. Waiting for a fresh pool snapshot…'
        : snapshot.data.count === 0
          ? 'The transaction pool is empty.'
          : bins.length === 0
            ? 'No fee distribution is available for this pool snapshot.'
            : null;

  return (
    <div className="space-y-3">
      {message ? (
        <p
          role="status"
          className="text-textMuted flex min-h-72 items-center justify-center px-4 text-center text-sm"
        >
          {message}
        </p>
      ) : (
        <>
          <HistogramPlot bins={bins} color={color} scale={scale} />
          <p className="text-textMuted text-xs">
            Each bar is a fee-rate group, labeled by its minimum rate. Bar height is virtual size,
            not transaction count. Groups are spaced evenly.
          </p>
          <details className="border-accentSecondary/20 rounded-md border text-xs">
            <summary className="text-textMuted hover:text-accent cursor-pointer px-3 py-2">
              View {formatInteger(bins.length)} fee-rate {bins.length === 1 ? 'group' : 'groups'}
            </summary>
            <div className="border-accentSecondary/20 max-h-56 overflow-auto border-t">
              <table className="w-full text-left tabular-nums">
                <caption className="sr-only">Transaction-pool fee-rate groups</caption>
                <thead className="bg-surface text-textMuted sticky top-0">
                  <tr>
                    <th scope="col" className="px-3 py-2">
                      Minimum rate (sat/vB)
                    </th>
                    <th scope="col" className="px-3 py-2 text-right">
                      Virtual size (vB)
                    </th>
                  </tr>
                </thead>
                <tbody className="divide-accentSecondary/20 divide-y">
                  {bins.map((bin, index) => (
                    <tr key={index}>
                      <td className="px-3 py-2">{formatFeeRate(bin.minimumRate)}</td>
                      <td className="px-3 py-2 text-right">{formatInteger(bin.virtualSize)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </details>
        </>
      )}
      {snapshot && (
        <p className="text-textMuted text-xs">
          {stale ? 'Last snapshot' : 'Snapshot'} {formatTimestamp(snapshot.receivedAt)}
          {!stale &&
            ` · ${formatInteger(snapshot.data.count)} transactions · ${formatVirtualSize(snapshot.data.virtual)}`}
        </p>
      )}
    </div>
  );
}

function HistogramPlot({
  bins,
  color,
  scale,
}: {
  bins: FeeHistogramBin[];
  color: string;
  scale: ChartScale;
}) {
  const containerReference = useRef<HTMLDivElement>(null);
  const chartReference = useRef<uPlot | null>(null);
  const binsReference = useRef(bins);

  useLayoutEffect(() => {
    binsReference.current = bins;
    const chart = chartReference.current;
    if (!chart) return;
    chart.setData([bins.map((_, index) => index), bins.map((bin) => bin.virtualSize)]);
    // Rates can change while group counts and axis bounds stay the same.
    chart.redraw(false, true);
  }, [bins]);

  useLayoutEffect(() => {
    const container = containerReference.current;
    if (!container) return;
    const theme = getComputedStyle(container);
    const font = `12px ${theme.fontFamily}`;
    const data: uPlot.AlignedData = [
      binsReference.current.map((_, index) => index),
      binsReference.current.map((bin) => bin.virtualSize),
    ];
    const options: uPlot.Options = {
      width: Math.max(240, container.clientWidth),
      height: 280,
      padding: [12, 12, 0, 0],
      cursor: { drag: { x: false, y: false } },
      legend: { show: true, live: true },
      scales: {
        x: { time: false, range: () => [-0.5, binsReference.current.length - 0.5] },
        y: {
          distr: scale === 'compressed' ? 4 : 1,
          asinh: 1,
          range: (_self, _min, max) => [0, Math.max(1, max * 1.08)],
        },
      },
      axes: [
        {
          stroke: theme.color,
          font,
          grid: { show: false },
          ticks: { stroke: '#ffa0224d', width: 1 },
          // X values are group indexes, not continuous fee-rate intervals.
          splits: (chart) => {
            const groups = binsReference.current;
            const step = Math.max(
              1,
              Math.ceil(groups.length / Math.max(1, Math.floor(chart.width / 80)))
            );
            return groups.map((_, index) => index).filter((index) => index % step === 0);
          },
          values: (_self, values: number[]) =>
            values.map((index) => formatFeeRate(binsReference.current[index].minimumRate)),
        },
        {
          size: 76,
          stroke: theme.color,
          font,
          grid: { stroke: '#c8563733', width: 1 },
          ticks: { stroke: '#ffa0224d', width: 1 },
          values: (_self, values: number[]) => values.map((value) => formatInteger(value)),
          ...(scale === 'compressed'
            ? { splits: compressedAxisSplits, filter: (_self: uPlot, values: number[]) => values }
            : {}),
        },
      ],
      series: [
        {
          label: 'Min rate',
          value: (_self, value: number | null) => {
            const bin = value === null ? undefined : binsReference.current[value];
            return bin ? `${formatFeeRate(bin.minimumRate)} sat/vB` : '—';
          },
        },
        {
          label: 'Virtual size',
          stroke: color,
          fill: `${color}90`,
          width: 1,
          points: { show: false },
          paths: uPlot.paths.bars?.({ size: [0.85, 48] }),
          value: (_self, value: number | null) => (value === null ? '—' : formatVirtualSize(value)),
        },
      ],
    };
    const chart = new uPlot(options, data, container);
    chartReference.current = chart;
    const observer = new ResizeObserver((entries) => {
      const width = entries[0]?.contentRect.width;
      if (width && width > 0) chart.setSize({ width: Math.round(width), height: 280 });
    });
    observer.observe(container);
    return () => {
      observer.disconnect();
      chart.destroy();
      chartReference.current = null;
    };
  }, [color, scale]);

  return (
    <div
      role="img"
      aria-label="Transaction-pool fee distribution: virtual size in vbytes by minimum fee rate in satoshis per vbyte"
    >
      <p className="text-textMuted mb-2 text-xs">Virtual size (vB)</p>
      <div ref={containerReference} className="text-textMuted min-w-0 overflow-hidden" />
      <p className="text-textMuted text-center text-xs">Minimum fee rate per group (sat/vB)</p>
    </div>
  );
}
