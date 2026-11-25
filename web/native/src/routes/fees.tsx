import { errorStatus } from '@/lib/api/errors';
import { useFees } from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';

const integer = new Intl.NumberFormat();
const histogramRate = new Intl.NumberFormat(undefined, {
  minimumFractionDigits: 3,
  maximumFractionDigits: 3,
});

function unavailable(error: unknown): string {
  const status = errorStatus(error);
  if (status === 501) return 'Relay is disabled, so the node keeps no pool.';
  if (status === 503) return 'The pool is not open yet.';
  return error instanceof Error ? error.message : 'Unknown error';
}

export function FeesPage() {
  usePageTitle('Fees');
  const { data: fees, isLoading, isError, error } = useFees();

  return (
    <div className="space-y-8">
      <header className="space-y-2">
        <p className="text-accentSecondary text-sm tracking-wide uppercase">Pool</p>
        <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
          Fees
        </h1>
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
          <section className="grid gap-4 sm:grid-cols-3">
            <Card label="Transactions" value={integer.format(fees.count)} />
            <Card label="Virtual size" value={`${integer.format(fees.virtual)} vB`} />
            <Card label="Fees" value={`${integer.format(fees.fee)} sat`} />
          </section>

          <section className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
            <h2 className="text-accent mb-3 text-lg font-semibold">Rates by virtual size</h2>
            <table className="w-full text-sm tabular-nums">
              <thead className="text-textMuted text-left">
                <tr>
                  <th className="py-1 font-normal">Rate (sat/vB)</th>
                  <th className="py-1 text-right font-normal">Virtual size (vB)</th>
                </tr>
              </thead>
              <tbody>
                {fees.histogram.map(([rate, size], index) => (
                  <tr key={index} className="border-accent/10 border-t">
                    <td className="py-1">{histogramRate.format(rate)}</td>
                    <td className="py-1 text-right">{integer.format(size)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
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
