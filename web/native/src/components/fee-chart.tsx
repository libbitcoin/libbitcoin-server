import { useEffect, useRef, useState, type PointerEvent } from 'react';

const HEIGHT = 220;
const LEFT = 64;
const RIGHT = 12;
const TOP = 20;
const BOTTOM = 28;
const GAP = 2;

// The virtual size of a block, in vbytes.
const BLOCK = 1_000_000;

const integer = new Intl.NumberFormat();
const rate = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 });

// A round step of about a third of the value, for the gridlines.
function step(maximum: number): number {
  const rough = maximum / 3;
  const power = 10 ** Math.floor(Math.log10(rough));
  const scaled = rough / power;
  return (scaled <= 1 ? 1 : scaled <= 2 ? 2 : scaled <= 5 ? 5 : 10) * power;
}

function axisSize(size: number): string {
  if (size >= 1_000_000) return `${rate.format(size / 1_000_000)} MvB`;
  if (size >= 1_000) return `${rate.format(size / 1_000)} kvB`;
  return `${integer.format(size)} vB`;
}

// The pool by fee rate, highest first: a bar per rate bin of its virtual size,
// with a line where each block of space fills.
export function FeeChart({ histogram }: { histogram: ReadonlyArray<readonly [number, number]> }) {
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

  const count = histogram.length;
  const peak = Math.max(0, ...histogram.map(([, size]) => size));
  const unit = peak > 0 ? step(peak) : 1;
  const top = Math.max(unit, Math.ceil((peak * 1.05) / unit) * unit);
  const plot = Math.max(0, width - LEFT - RIGHT);
  const slot = count > 0 ? plot / count : 0;
  const bar = Math.max(1, slot - GAP);
  const y = (size: number) => TOP + (1 - size / top) * (HEIGHT - TOP - BOTTOM);
  const base = y(0);
  const grid = Array.from({ length: Math.round(top / unit) + 1 }, (_, index) => index * unit);

  const totals: number[] = [];
  histogram.forEach(([, size], index) => {
    totals.push((totals[index - 1] ?? 0) + size);
  });

  // Each block boundary falls within the bin whose running total crosses it.
  const blocks: Array<{ block: number; x: number }> = [];
  for (let block = 1; block * BLOCK < (totals.at(-1) ?? 0); block += 1) {
    const index = totals.findIndex((total) => total >= block * BLOCK);
    const before = totals[index - 1] ?? 0;
    const within = (block * BLOCK - before) / histogram[index][1];
    blocks.push({ block, x: LEFT + (index + within) * slot });
  }

  const labeled = Math.max(1, Math.ceil(count / Math.max(1, Math.floor(plot / 72))));
  const hovered = hover === null ? undefined : histogram.at(hover);

  const onMove = (event: PointerEvent<SVGSVGElement>) => {
    if (count === 0 || slot <= 0) return;
    const bounds = event.currentTarget.getBoundingClientRect();
    const index = Math.floor((event.clientX - bounds.left - LEFT) / slot);
    setHover(index >= 0 && index < count ? index : null);
  };

  return (
    <div ref={frame} className="relative">
      {width > 0 && count > 0 && (
        <svg
          width={width}
          height={HEIGHT}
          role="img"
          aria-label={`Pool virtual size by fee rate, ${integer.format(count)} bins, ${axisSize(totals.at(-1) ?? 0)} in all.`}
          className="block touch-none select-none"
          onPointerMove={onMove}
          onPointerDown={onMove}
          onPointerLeave={() => {
            setHover(null);
          }}
        >
          {grid.map((size) => (
            <g key={size}>
              <line
                x1={LEFT}
                x2={width - RIGHT}
                y1={y(size)}
                y2={y(size)}
                className="stroke-textMuted"
                strokeOpacity={size === 0 ? 0.35 : 0.12}
                strokeWidth={1}
              />
              <text
                x={LEFT - 8}
                y={y(size)}
                textAnchor="end"
                dominantBaseline="middle"
                className="fill-textMuted text-[11px] tabular-nums"
              >
                {axisSize(size)}
              </text>
            </g>
          ))}
          {histogram.map(([binRate, size], index) => (
            <rect
              key={index}
              x={LEFT + index * slot + GAP / 2}
              y={y(size)}
              width={bar}
              height={Math.max(0, base - y(size))}
              rx={Math.min(2, bar / 2)}
              className="fill-accent"
              fillOpacity={hover === index ? 1 : 0.75}
            >
              <title>{`${rate.format(binRate)} sat/vB · ${integer.format(size)} vB`}</title>
            </rect>
          ))}
          {histogram.map(([binRate], index) =>
            index % labeled === 0 ? (
              <text
                key={index}
                x={LEFT + index * slot + slot / 2}
                y={HEIGHT - 8}
                textAnchor="middle"
                className="fill-textMuted text-[11px] tabular-nums"
              >
                {rate.format(binRate)}
              </text>
            ) : null
          )}
          {blocks.map(({ block, x }) => (
            <g key={block} pointerEvents="none">
              <line x1={x} x2={x} y1={TOP - 6} y2={base} className="stroke-accentSecondary" strokeDasharray="3 3" />
              <text x={x + 4} y={TOP - 8} className="fill-accentSecondary text-[11px]">
                {`Block ${String(block)}`}
              </text>
            </g>
          ))}
        </svg>
      )}
      {hovered && hover !== null && (
        <div
          className="border-accent/40 bg-surface text-textPrimary pointer-events-none absolute top-2 rounded-md border px-2 py-1 text-xs tabular-nums shadow-lg"
          style={{ left: Math.min(Math.max(LEFT + hover * slot + slot, LEFT), Math.max(LEFT, width - 170)) }}
        >
          <p className="font-medium">{`${rate.format(hovered[0])} sat/vB`}</p>
          <p className="text-textMuted">{`${integer.format(hovered[1])} vB`}</p>
          <p className="text-textMuted">{`${axisSize(totals[hover])} at or above`}</p>
        </div>
      )}
      {count === 0 && <p className="text-textMuted py-10 text-center text-sm">The pool is empty.</p>}
    </div>
  );
}
