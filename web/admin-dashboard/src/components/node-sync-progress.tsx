import { formatInteger } from '@/lib/format';
import { type SyncEstimate, SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS } from '@/lib/sync-status';

interface NodeSyncProgressProps {
  confirmed: number | null;
  estimate: SyncEstimate | null;
}

export function NodeSyncProgress({ confirmed, estimate }: NodeSyncProgressProps) {
  if (
    confirmed === null ||
    estimate === null ||
    estimate.blocksRemaining <= SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS
  )
    return null;

  const progress = Math.min(99.99, Math.max(0, estimate.progress));
  const progressLabel = `${progress.toFixed(2)}%`;

  return (
    <section
      className="border-accent/30 bg-surface/80 overflow-hidden rounded-2xl border"
      aria-labelledby="node-sync-progress-title"
    >
      <header className="flex flex-wrap items-end justify-between gap-3 px-5 pt-5">
        <div>
          <p className="text-accentSecondary text-xs tracking-wide uppercase">
            Node synchronization
          </p>
          <h2 id="node-sync-progress-title" className="text-accent mt-1 text-lg font-semibold">
            Catching up to estimated chain tip
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
          aria-valuetext={`Approximately ${formatInteger(estimate.blocksRemaining)} blocks remaining`}
          className="bg-background border-accentSecondary/20 h-3 overflow-hidden rounded-full border"
        >
          <div
            className="bg-accent h-full rounded-full transition-[width] duration-700"
            style={{ width: `${String(progress)}%` }}
          />
        </div>
      </div>

      <dl className="bg-accentSecondary/10 mt-5 grid gap-px text-sm sm:grid-cols-3">
        <SyncValue label="Confirmed top" value={formatInteger(confirmed)} />
        <SyncValue label="Estimated chain tip" value={formatInteger(estimate.estimatedTop)} />
        <SyncValue
          label="Estimated blocks remaining"
          value={formatInteger(estimate.blocksRemaining)}
        />
      </dl>
      <p className="bg-surface text-textMuted border-accentSecondary/20 border-t px-5 py-2 text-[11px]">
        Estimated from the confirmed block timestamp and your browser clock, assuming one block
        every 10 minutes. Actual block counts may differ.
      </p>
    </section>
  );
}

function SyncValue({ label, value }: { label: string; value: string }) {
  return (
    <div className="bg-surface px-5 py-3">
      <dt className="text-accentSecondary text-xs tracking-wide uppercase">{label}</dt>
      <dd className="text-textPrimary mt-1 text-lg font-semibold tabular-nums">{value}</dd>
    </div>
  );
}
