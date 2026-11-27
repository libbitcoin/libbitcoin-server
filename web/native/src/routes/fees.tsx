import { useState } from 'react';
import { FeeChart } from '@/components/fee-chart';
import { Toggle } from '@/components/toggle';
import type { EstimateMode } from '@/lib/api/client';
import { errorStatus } from '@/lib/api/errors';
import { useFees } from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';

const integer = new Intl.NumberFormat();
const rate = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 });

const STORAGE_KEY = 'libbitcoin.fees.mode';

const MODES: ReadonlyArray<{ id: EstimateMode; label: string }> = [
  { id: 'economical', label: 'Economical' },
  { id: 'conservative', label: 'Conservative' },
  { id: 'geometric', label: 'Geometric' },
  { id: 'basic', label: 'Basic' },
];

// Confirmation targets (blocks) of the node's estimates.
const TARGETS: ReadonlyArray<readonly [string, string]> = [
  ['1', 'Next block'],
  ['3', '~30 min'],
  ['6', '~1 hour'],
  ['12', '~2 hours'],
  ['144', '~1 day'],
];

function loadMode(): EstimateMode {
  try {
    const saved = globalThis.localStorage.getItem(STORAGE_KEY);
    return MODES.find((mode) => mode.id === saved)?.id ?? 'economical';
  } catch {
    return 'economical';
  }
}

function saveMode(mode: EstimateMode) {
  try {
    globalThis.localStorage.setItem(STORAGE_KEY, mode);
  } catch {
    // The mode is a convenience, the page works without it.
  }
}

function unavailable(error: unknown): string {
  const status = errorStatus(error);
  if (status === 501) return 'Relay is disabled, so the node keeps no pool.';
  if (status === 503) return 'The pool is not open yet.';
  return error instanceof Error ? error.message : 'Unknown error';
}

export function FeesPage() {
  usePageTitle('Fees');
  const [mode, setMode] = useState<EstimateMode>(loadMode);
  const { data: fees, isLoading, isError, error } = useFees();

  const onMode = (next: EstimateMode) => {
    setMode(next);
    saveMode(next);
  };

  return (
    <div className="space-y-8">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
            Fees
          </h1>
          <p className="text-textMuted text-sm">The fee rates of the pool, and the rates it projects to confirm.</p>
        </div>
        <div className="text-sm">
          <Toggle label="Estimate mode" options={MODES} value={mode} onChange={onMode} />
        </div>
      </header>

      {isError && (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Fees unavailable</p>
          <p className="text-textMuted mt-1 break-words">{unavailable(error)}</p>
        </div>
      )}
      {isLoading && <div className="text-textMuted py-10 text-sm">Loading fees...</div>}

      {fees && (
        <>
          <section className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
            {TARGETS.map(([target, label]) => {
              const value = fees.estimates?.[mode]?.[target];
              return (
                <Card
                  key={target}
                  label={label}
                  value={typeof value === 'number' ? `${rate.format(value)} sat/vB` : '—'}
                />
              );
            })}
          </section>
          {fees.estimates === null && (
            <p className="text-textMuted text-sm">The node has no estimates until its pool opens.</p>
          )}

          <section className="grid gap-4 sm:grid-cols-3">
            <Card label="Transactions" value={integer.format(fees.count)} />
            <Card label="Virtual size" value={`${integer.format(fees.virtual)} vB`} />
            <Card label="Fees" value={`${integer.format(fees.fee)} sat`} />
          </section>

          <section className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
            <h2 className="text-accent mb-3 text-lg font-semibold">Pool by fee rate (sat/vB)</h2>
            <FeeChart histogram={fees.histogram} />
          </section>
        </>
      )}
    </div>
  );
}

function Card({ label, value }: { label: string; value: string }) {
  return (
    <div className="border-accent/30 bg-surface/80 rounded-2xl border p-4">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary mt-1 text-2xl font-semibold tabular-nums">{value}</p>
    </div>
  );
}
