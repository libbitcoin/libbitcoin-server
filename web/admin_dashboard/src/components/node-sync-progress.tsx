import { useEffect, useRef, useState } from 'react';
import { formatInteger } from '@/lib/format';
import { CHART_COLORS } from '@/lib/chart-config';
import { chartColor } from '@/lib/chart-theme';
import type { SyncFill } from '@/lib/sync-fill';
import { type ChainSync, SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS } from '@/lib/sync-status';
import { useTheme } from '@/lib/theme';
import { FitHash } from '@/components/fit-hash';

interface NodeSyncProgressProps {
  sync: ChainSync | null;
}

export function NodeSyncProgress({ sync }: NodeSyncProgressProps) {
  const [focus, setFocus] = useState<FillLayer | null>(null);
  if (
    sync === null ||
    (!sync.diverged &&
      sync.blocksRemaining !== null &&
      sync.blocksRemaining <= SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS)
  )
    return null;

  const progress = Math.min(99.99, Math.max(0, sync.progress));
  const progressLabel = `${progress.toFixed(2)}%`;

  return (
    <section
      className="border-accent/30 bg-surface/80 overflow-hidden rounded-2xl border"
      aria-labelledby="node-sync-progress-title"
    >
      <header className="flex flex-wrap items-end justify-between gap-3 px-5 pt-5">
        <div>
          <h2 id="node-sync-progress-title" className="text-accent text-lg font-semibold">
            {sync.diverged ? 'Candidate branch diverges' : 'Catching up to top candidate header'}
          </h2>
        </div>
        <p className="text-accent text-2xl font-semibold tabular-nums">{progressLabel}</p>
      </header>

      <div className="px-5 pt-4">
        <div
          role="progressbar"
          aria-label="Node synchronization progress"
          aria-valuemin={0}
          aria-valuemax={100}
          aria-valuenow={Number(progress.toFixed(2))}
          aria-valuetext={
            sync.blocksRemaining === null
              ? 'Headers not current'
              : `${formatInteger(sync.blocksRemaining)} blocks remaining`
          }
          className="bg-background border-accentSecondary/20 h-4 overflow-hidden rounded-full border"
        >
          <FillBar
            fill={sync.fill}
            confirmed={sync.confirmed.height}
            candidate={sync.candidate.height}
            estimatedTop={sync.estimatedTop}
            focus={focus}
          />
        </div>
        <div className="text-textMuted mt-2 flex flex-wrap gap-4 text-[11px]">
          {FILL_LAYERS.map((layer) => (
            <span
              key={layer.label}
              className="flex cursor-default items-center gap-1.5"
              onMouseEnter={() => {
                setFocus(layer.label);
              }}
              onMouseLeave={() => {
                setFocus(null);
              }}
            >
              <span
                className="inline-block h-2 w-3 rounded-sm"
                style={{
                  background: `var(--chart-color-${String(layer.color)}, ${CHART_COLORS[layer.color]})`,
                }}
              />
              {layer.label}
            </span>
          ))}
        </div>
      </div>

      <dl className="bg-accentSecondary/10 mt-5 grid gap-px text-sm sm:grid-cols-3">
        <SyncValue
          label="Top confirmed block"
          value={formatInteger(sync.confirmed.height)}
          hash={sync.confirmed.hash}
        />
        <SyncValue
          label="Blocks remaining"
          value={sync.blocksRemaining === null ? '—' : formatInteger(sync.blocksRemaining)}
        />
        <SyncValue
          label="Top candidate header"
          value={formatInteger(sync.candidate.height)}
          hash={sync.candidate.hash}
        />
      </dl>
      {sync.diverged && (
        <p className="bg-surface border-accentSecondary/20 border-t px-5 py-2 text-[11px] text-amber-300">
          The candidate chain does not extend the confirmed top, so the confirmed chain is
          expected to reorganize.
        </p>
      )}
    </section>
  );
}

const FILL_LAYERS = [
  { label: 'Confirmed', color: 2 },
  { label: 'Validated', color: 1 },
  { label: 'Archived', color: 0 },
  { label: 'Headers', color: 3 },
] as const;

type FillLayer = (typeof FILL_LAYERS)[number]['label'];

// Headers and confirmed are contiguous, so each is solid to its frontier.
// Archived and validated heights arrive out of order, so each bin with any
// reported height is filled. Layers are opaque, each drawn over those it
// implies. A focused layer is drawn alone.
function FillBar({
  fill,
  confirmed,
  candidate,
  estimatedTop,
  focus,
}: {
  fill: SyncFill | null;
  confirmed: number;
  candidate: number;
  estimatedTop: number;
  focus: FillLayer | null;
}) {
  const canvasReference = useRef<HTMLCanvasElement>(null);
  const activeTheme = useTheme();

  useEffect(() => {
    const canvas = canvasReference.current;
    if (!canvas) return;

    const draw = () => {
      const ratio = window.devicePixelRatio || 1;
      const width = Math.max(1, Math.round(canvas.clientWidth * ratio));
      const height = Math.max(1, Math.round(canvas.clientHeight * ratio));
      if (canvas.width !== width) canvas.width = width;
      if (canvas.height !== height) canvas.height = height;
      const context = canvas.getContext('2d');
      if (!context) return;

      const style = getComputedStyle(canvas);
      const color = (index: number) => chartColor(CHART_COLORS[index] ?? '', style);
      const headers = Math.max(candidate, fill?.headers ?? 0);
      const top = Math.max(headers, estimatedTop);
      const scale = top > 0 ? width / top : 0;
      context.clearRect(0, 0, width, height);

      const shade = (bins: Uint8Array, color: string) => {
        if (!fill || scale === 0) return;
        const span = fill.height + 1;
        context.fillStyle = color;
        for (let bin = 0; bin < bins.length; bin++) {
          const value = bins[bin] ?? 0;
          if (value === 0) continue;
          const first = Math.floor((bin * span) / bins.length);
          const last = Math.floor(((bin + 1) * span) / bins.length);
          const left = first * scale;
          const right = Math.max(left + 1, Math.min(width, last * scale));
          if (left >= width) break;
          context.fillRect(left, 0, right - left, height);
        }
      };

      const shown = (layer: FillLayer) => focus === null || focus === layer;
      if (shown('Headers')) {
        context.fillStyle = color(3);
        context.fillRect(0, 0, Math.min(width, headers * scale), height);
      }
      if (fill && shown('Archived')) shade(fill.archived, color(0));
      if (fill && shown('Validated')) shade(fill.validated, color(1));
      if (shown('Confirmed')) {
        context.fillStyle = color(2);
        context.fillRect(0, 0, Math.min(width, confirmed * scale), height);
      }
    };

    draw();
    const observer = new ResizeObserver(draw);
    observer.observe(canvas);
    return () => {
      observer.disconnect();
    };
  }, [fill, confirmed, candidate, estimatedTop, focus, activeTheme]);

  return <canvas ref={canvasReference} className="block h-full w-full" />;
}

function SyncValue({ label, value, hash }: { label: string; value: string; hash?: string }) {
  return (
    <div className="bg-surface min-w-0 px-5 py-3">
      <dt className="text-accentSecondary text-xs tracking-wide uppercase">{label}</dt>
      <dd className="text-textPrimary mt-1 text-lg font-semibold tabular-nums">{value}</dd>
      {hash !== undefined && (
        <dd data-fit className="text-textMuted overflow-hidden font-mono text-[11px] whitespace-nowrap">
          <FitHash value={hash} />
        </dd>
      )}
    </div>
  );
}
