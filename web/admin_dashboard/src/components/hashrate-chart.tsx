import { useEffect, useRef, useState, type PointerEvent } from 'react';
import { formatHashrate } from '@/lib/mining/format';

// A hashrate (hashes per second) at a time, in milliseconds.
export interface HashrateSample {
  time: number;
  rate: number;
}

const HEIGHT = 200;
const LEFT = 72;
const RIGHT = 12;
const TOP = 12;
const BOTTOM = 24;


// A round step of about a third of the value, for the gridlines.
function step(maximum: number): number {
  const rough = maximum / 3;
  const power = 10 ** Math.floor(Math.log10(rough));
  const scaled = rough / power;
  return (scaled <= 1 ? 1 : scaled <= 2 ? 2 : scaled <= 5 ? 5 : 10) * power;
}

// An axis rate, without trailing zeros (6 MH/s, 2.5 MH/s).
function axisRate(rate: number): string {
  return formatHashrate(rate).replace(/(\.\d*?)0+ /, '$1 ').replace('. ', ' ');
}

function ago(seconds: number): string {
  if (seconds <= 0) return 'now';
  return seconds < 60 ? `−${String(seconds)} s` : `−${String(seconds / 60)} min`;
}

// The hashrate over the span (seconds) ending at the last sample.
export function HashrateChart({
  history,
  span = 300,
}: {
  history: readonly HashrateSample[];
  span?: number;
}) {
  const frame = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);
  const [hover, setHover] = useState<number | null>(null);

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

  const end = history.at(-1)?.time ?? 0;
  const start = end - span * 1000;
  const shown = history.filter((sample) => sample.time >= start);
  const peak = Math.max(0, ...shown.map((sample) => sample.rate));
  const unit = peak > 0 ? step(peak) : 1;
  const top = Math.max(unit, Math.ceil((peak * 1.05) / unit) * unit);
  const plot = Math.max(0, width - LEFT - RIGHT);
  const x = (time: number) => LEFT + ((time - start) / (span * 1000)) * plot;
  const y = (rate: number) => TOP + (1 - rate / top) * (HEIGHT - TOP - BOTTOM);
  const base = y(0);
  const points = shown.map((sample) => `${x(sample.time).toFixed(1)},${y(sample.rate).toFixed(1)}`);
  const line = points.length > 1 ? `M${points.join('L')}` : '';
  const area =
    points.length > 1
      ? `${line}L${x(shown[shown.length - 1].time).toFixed(1)},${base.toFixed(1)}L${x(shown[0].time).toFixed(1)},${base.toFixed(1)}Z`
      : '';
  const grid = Array.from({ length: Math.round(top / unit) + 1 }, (_, index) => index * unit);
  // Narrow charts label fewer times.
  const every = span <= 300 ? 60 : 300;
  const ticks =
    plot < 420
      ? [span, span / 2, 0]
      : Array.from({ length: Math.floor(span / every) + 1 }, (_, index) => span - index * every);
  const hovered = hover === null ? undefined : shown.at(hover);

  const onMove = (event: PointerEvent<SVGSVGElement>) => {
    if (shown.length === 0 || plot <= 0) return;
    const bounds = event.currentTarget.getBoundingClientRect();
    const time = start + ((event.clientX - bounds.left - LEFT) / plot) * span * 1000;
    let nearest = 0;
    for (let index = 1; index < shown.length; index += 1)
      if (Math.abs(shown[index].time - time) < Math.abs(shown[nearest].time - time)) nearest = index;
    setHover(nearest);
  };

  const latest = shown.at(-1);
  return (
    <div ref={frame} className="relative">
      {width > 0 && (
        <svg
          width={width}
          height={HEIGHT}
          role="img"
          aria-label={
            latest
              ? `Hashrate over the last ${String(span / 60)} minutes, now ${formatHashrate(latest.rate)}, peak ${formatHashrate(peak)}.`
              : `Hashrate over the last ${String(span / 60)} minutes, no samples yet.`
          }
          className="block touch-none select-none"
          onPointerMove={onMove}
          onPointerDown={onMove}
          onPointerLeave={() => {
            setHover(null);
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
          {area && <path d={area} className="fill-accent" fillOpacity={0.14} />}
          {line && (
            <path
              d={line}
              fill="none"
              className="stroke-accent"
              strokeWidth={2}
              strokeLinejoin="round"
              strokeLinecap="round"
            />
          )}
          {hovered && (
            <g pointerEvents="none">
              <line
                x1={x(hovered.time)}
                x2={x(hovered.time)}
                y1={TOP}
                y2={base}
                className="stroke-textMuted"
                strokeOpacity={0.5}
                strokeWidth={1}
              />
              <circle
                cx={x(hovered.time)}
                cy={y(hovered.rate)}
                r={4}
                className="fill-accent stroke-surface"
                strokeWidth={2}
              />
            </g>
          )}
        </svg>
      )}
      {hovered && (
        <div
          className="border-accent/40 bg-surface text-textPrimary pointer-events-none absolute top-2 rounded-md border px-2 py-1 text-xs tabular-nums shadow-lg"
          style={{
            left: Math.min(Math.max(x(hovered.time) + 10, LEFT), Math.max(LEFT, width - 140)),
          }}
        >
          <p className="font-medium">{formatHashrate(hovered.rate)}</p>
          <p className="text-textMuted">{new Date(hovered.time).toLocaleTimeString()}</p>
        </div>
      )}
      {shown.length === 0 && (
        <p className="text-textMuted absolute inset-0 flex items-center justify-center text-sm">
          The hashrate is drawn here while mining.
        </p>
      )}
    </div>
  );
}
