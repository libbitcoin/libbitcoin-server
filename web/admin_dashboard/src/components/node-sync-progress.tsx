import { formatInteger } from '@/lib/format';
import { type ChainSync, SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS } from '@/lib/sync-status';

interface NodeSyncProgressProps {
  sync: ChainSync | null;
}

export function NodeSyncProgress({ sync }: NodeSyncProgressProps) {
  if (
    sync === null ||
    (!sync.diverged && sync.blocksRemaining <= SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS)
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
          <p className="text-accentSecondary text-xs tracking-wide uppercase">
            Node synchronization
          </p>
          <h2 id="node-sync-progress-title" className="text-accent mt-1 text-lg font-semibold">
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
          aria-valuetext={`${formatInteger(sync.blocksRemaining)} blocks remaining`}
          className="bg-background border-accentSecondary/20 h-3 overflow-hidden rounded-full border"
        >
          <div
            className="bg-accent h-full rounded-full transition-[width] duration-700"
            style={{ width: `${String(progress)}%` }}
          />
        </div>
      </div>

      <dl className="bg-accentSecondary/10 mt-5 grid gap-px text-sm sm:grid-cols-3">
        <SyncValue
          label="Top confirmed block"
          value={formatInteger(sync.confirmed.height)}
          hash={sync.confirmed.hash}
        />
        <SyncValue
          label="Top candidate header"
          value={formatInteger(sync.candidate.height)}
          hash={sync.candidate.hash}
        />
        <SyncValue label="Blocks remaining" value={formatInteger(sync.blocksRemaining)} />
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

function SyncValue({ label, value, hash }: { label: string; value: string; hash?: string }) {
  return (
    <div className="bg-surface min-w-0 px-5 py-3">
      <dt className="text-accentSecondary text-xs tracking-wide uppercase">{label}</dt>
      <dd className="text-textPrimary mt-1 text-lg font-semibold tabular-nums">{value}</dd>
      {hash !== undefined && (
        <dd className="text-textMuted truncate font-mono text-[11px]" title={hash}>
          {hash}
        </dd>
      )}
    </div>
  );
}
