export type ChainStatus = 'confirmed' | 'unconfirmed';

const STYLES: Record<ChainStatus, string> = {
  confirmed: 'bg-emerald-500/20 text-emerald-300',
  unconfirmed: 'bg-amber-500/20 text-amber-300',
};

const UNKNOWN = 'bg-slate-500/20 text-slate-300';

// Where a block, template or tx stands relative to the strong chain.
export function StatusPill({ status }: { status: ChainStatus | undefined }) {
  return (
    <span
      className={`inline-flex w-fit items-center gap-2 rounded-full px-3 py-1 text-xs font-medium tracking-wide whitespace-nowrap uppercase ${status ? STYLES[status] : UNKNOWN}`}
    >
      <span className="h-2 w-2 rounded-full bg-current" aria-hidden />
      {status ?? '—'}
    </span>
  );
}
