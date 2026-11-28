import { useEffect, useRef, useState, type PointerEvent } from 'react';
import { CHART_COLORS } from '@/lib/chart-config';
import { ago, axisRate, gridStep } from '@/lib/mining/axis';
import { FLEET, OTHER, type FleetSeries } from '@/lib/mining/fleet';
import { formatHashrate } from '@/lib/mining/format';

const HEIGHT = 260;
const LEFT = 72;
const RIGHT = 12;
const TOP = 12;
const BOTTOM = 24;

// The color of a slot, adapted by the theme.
export function slotColor(slot: number): string {
  return `var(--chart-color-${String(slot)}, ${CHART_COLORS[slot] ?? CHART_COLORS[0]})`;
}

// A series color: its slot, else muted for other.
export function bandColor(slot: number | undefined): string {
  return slot === undefined ? 'var(--theme-text-muted)' : slotColor(slot);
}

export function PinIcon({ size = 16 }: { size?: number }) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={2}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
    >
      <path d="M12 17v5" />
      <path d="M9 10.76a2 2 0 0 1-1.11 1.79l-1.78.9A2 2 0 0 0 5 15.24V16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-.76a2 2 0 0 0-1.11-1.79l-1.78-.9A2 2 0 0 1 15 10.76V7a1 1 0 0 1 1-1 2 2 0 0 0 0-4H8a2 2 0 0 0 0 4 1 1 0 0 1 1 1z" />
    </svg>
  );
}

// The hash rate of the fleet over the span (seconds) ending at its last
// sample: the colored channels and other stacked (or as lines), their sum the
// fleet. The channel list is its legend: hovering a band highlights it there.
export function FleetChart({
  series,
  names,
  hidden,
  focus,
  onFocus,
  span = 1800,
}: {
  series: FleetSeries;
  names: ReadonlyMap<string, string>;
  hidden: ReadonlySet<string>;
  focus: string | null;
  onFocus: (key: string | null) => void;
  span?: number;
}) {
  const frame = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);
  const [hover, setHover] = useState<number | null>(null);
  const [stacked, setStacked] = useState(true);

  useEffect(() => {
    const element = frame.current;
    if (!element) return;
    const observer = new ResizeObserver(([entry]) => {
      setWidth(entry.contentRect.width);
    });
    observer.observe(element);
    return () => {
      observer.disconnect();
    };
  }, []);

  const { times, bands, total, others } = series;
  const end = times.at(-1) ?? 0;
  const start = end - span * 1000;
  const first = Math.max(0, times.findIndex((time) => time >= start));
  const shown = times.slice(first);
  const showFleet = !hidden.has(FLEET);

  // Stacked bands bottom to top, each with its lower and upper edge.
  const layers: { key: string; slot?: number; lower: number[]; upper: number[] }[] = [];
  const below = shown.map(() => 0);
  for (const band of bands) {
    const rates = band.rates.slice(first);
    const lower = stacked ? [...below] : shown.map(() => 0);
    const upper = rates.map((rate, index) => lower[index] + rate);
    if (stacked) upper.forEach((value, index) => (below[index] = value));
    layers.push({ key: band.key, slot: band.slot, lower, upper });
  }

  const sums = total.slice(first);
  const peak = Math.max(
    0,
    ...(stacked || showFleet ? sums : []),
    ...layers.flatMap((layer) => layer.upper)
  );
  const unit = peak > 0 ? gridStep(peak) : 1;
  const top = Math.max(unit, Math.ceil((peak * 1.05) / unit) * unit);
  const plot = Math.max(0, width - LEFT - RIGHT);
  const x = (time: number) => LEFT + ((time - start) / (span * 1000)) * plot;
  const y = (rate: number) => TOP + (1 - rate / top) * (HEIGHT - TOP - BOTTOM);
  const base = y(0);
  const points = (values: readonly number[]) =>
    values.map((value, index) => `${x(shown[index]).toFixed(1)},${y(value).toFixed(1)}`);
  const line = (values: readonly number[]) => (values.length > 1 ? `M${points(values).join('L')}` : '');
  const area = (lower: readonly number[], upper: readonly number[]) =>
    upper.length > 1 ? `${line(upper)}L${points(lower).reverse().join('L')}Z` : '';

  const grid = Array.from({ length: Math.round(top / unit) + 1 }, (_, index) => index * unit);
  const every = span <= 300 ? 60 : 300;
  const ticks =
    plot < 420
      ? [span, span / 2, 0]
      : Array.from({ length: Math.floor(span / every) + 1 }, (_, index) => span - index * every);
  const faded = (key: string) => focus !== null && focus !== key;
  const name = (key: string) => (key === OTHER ? `Other (${String(others)})` : (names.get(key) ?? key));

  const onMove = (event: PointerEvent<SVGSVGElement>) => {
    if (shown.length === 0 || plot <= 0) return;
    const bounds = event.currentTarget.getBoundingClientRect();
    const time = start + ((event.clientX - bounds.left - LEFT) / plot) * span * 1000;
    let nearest = 0;
    for (let index = 1; index < shown.length; index += 1)
      if (Math.abs(shown[index] - time) < Math.abs(shown[nearest] - time)) nearest = index;
    setHover(nearest);
    if (!stacked) return;
    const value = top * (1 - (event.clientY - bounds.top - TOP) / (HEIGHT - TOP - BOTTOM));
    const hit = layers.find((layer) => value >= layer.lower[nearest] && value < layer.upper[nearest]);
    const key = hit?.key ?? null;
    if (key !== focus) onFocus(key);
  };

  const hovered = hover === null ? undefined : shown.at(hover);
  const latest = sums.at(-1);

  return (
    <div className="space-y-3">
      <div className="flex justify-end gap-1.5" role="group" aria-label="Chart form">
        {[
          { label: 'Stacked', on: stacked, value: true },
          { label: 'Lines', on: !stacked, value: false },
        ].map((option) => (
          <button
            key={option.label}
            type="button"
            aria-pressed={option.on}
            onClick={() => {
              setStacked(option.value);
              onFocus(null);
            }}
            className={`rounded-md border px-2.5 py-1 text-xs transition ${
              option.on
                ? 'border-accent/50 bg-accent/15 text-accent'
                : 'text-textPrimary hover:bg-accentSecondary/10 border-transparent'
            }`}
          >
            {option.label}
          </button>
        ))}
      </div>
      <div ref={frame} className="relative">
        {width > 0 && (
          <svg
            width={width}
            height={HEIGHT}
            role="img"
            aria-label={
              latest === undefined
                ? `Fleet hashrate over the last ${String(span / 60)} minutes, no samples yet.`
                : `Fleet hashrate over the last ${String(span / 60)} minutes, now ${formatHashrate(latest)}.`
            }
            className="block touch-none select-none"
            onPointerMove={onMove}
            onPointerDown={onMove}
            onPointerLeave={() => {
              setHover(null);
              onFocus(null);
            }}
          >
            {grid.map((rate) => (
              <g key={rate}>
                <line
                  x1={LEFT}
                  x2={width - RIGHT}
                  y1={y(rate)}
                  y2={y(rate)}
                  className="stroke-textMuted"
                  strokeOpacity={rate === 0 ? 0.35 : 0.12}
                  strokeWidth={1}
                />
                <text
                  x={LEFT - 8}
                  y={y(rate)}
                  textAnchor="end"
                  dominantBaseline="middle"
                  className="fill-textMuted text-[11px] tabular-nums"
                >
                  {shown.length > 0 ? axisRate(rate) : ''}
                </text>
              </g>
            ))}
            {ticks.map((seconds) => (
              <text
                key={seconds}
                x={x(end - seconds * 1000)}
                y={HEIGHT - 6}
                textAnchor={seconds === span ? 'start' : seconds === 0 ? 'end' : 'middle'}
                className="fill-textMuted text-[11px] tabular-nums"
              >
                {ago(seconds)}
              </text>
            ))}
            {stacked &&
              layers.map((layer) => (
                <path
                  key={`area-${layer.key}`}
                  d={area(layer.lower, layer.upper)}
                  fill={bandColor(layer.slot)}
                  fillOpacity={faded(layer.key) ? 0.06 : layer.slot === undefined ? 0.2 : 0.3}
                />
              ))}
            {layers.map((layer) => (
              <path
                key={`line-${layer.key}`}
                d={line(layer.upper)}
                fill="none"
                stroke={bandColor(layer.slot)}
                strokeOpacity={faded(layer.key) ? 0.2 : 1}
                strokeWidth={stacked ? 1.5 : 2}
                strokeLinejoin="round"
                strokeLinecap="round"
              />
            ))}
            {showFleet && (
              <path
                d={line(sums)}
                fill="none"
                className="stroke-textPrimary"
                strokeOpacity={focus === null ? 1 : 0.5}
                strokeWidth={2}
                strokeDasharray={stacked ? undefined : '6 4'}
                strokeLinejoin="round"
                strokeLinecap="round"
              />
            )}
            {hovered !== undefined && (
              <line
                pointerEvents="none"
                x1={x(hovered)}
                x2={x(hovered)}
                y1={TOP}
                y2={base}
                className="stroke-textMuted"
                strokeOpacity={0.5}
                strokeWidth={1}
              />
            )}
          </svg>
        )}
        {hovered !== undefined && hover !== null && (
          <div
            className="border-accent/40 bg-surface text-textPrimary pointer-events-none absolute top-2 min-w-56 rounded-md border px-2 py-1 text-xs tabular-nums shadow-lg"
            style={{
              left: Math.min(Math.max(x(hovered) + 10, LEFT), Math.max(LEFT, width - 240)),
            }}
          >
            <p className="text-textMuted mb-1">{new Date(hovered).toLocaleTimeString()}</p>
            {[...layers].reverse().map((layer) => {
              const rate = layer.upper[hover] - layer.lower[hover];
              return (
                <p
                  key={layer.key}
                  className={`flex items-center gap-1.5 ${focus === layer.key ? 'font-semibold' : focus !== null ? 'opacity-60' : ''}`}
                >
                  <span
                    className="inline-block h-2.5 w-2.5 shrink-0 rounded-sm"
                    style={{ background: bandColor(layer.slot) }}
                  />
                  <span className="min-w-0 flex-1 truncate">{name(layer.key)}</span>
                  <span>{rate > 0 ? formatHashrate(rate) : '—'}</span>
                </p>
              );
            })}
            <p className="border-accent/20 mt-1 flex gap-3 border-t pt-1 font-semibold">
              <span className="flex-1">Fleet</span>
              <span>{formatHashrate(sums[hover] ?? 0)}</span>
            </p>
          </div>
        )}
        {shown.length === 0 && (
          <p className="text-textMuted absolute inset-0 flex items-center justify-center text-sm">
            The fleet hashrate is drawn here while mining.
          </p>
        )}
      </div>
    </div>
  );
}
