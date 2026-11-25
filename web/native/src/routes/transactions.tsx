import { useMemo } from 'react';
import { Spinner } from '@/components/spinner';
import { TransactionTable } from '@/components/transaction-table';
import { errorStatus } from '@/lib/api/errors';
import {
  useUnconfirmedTransactionDetails,
  useUnconfirmedTransactionHashes,
} from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { formatInteger, formatSats } from '@/lib/utils/format';

function unavailable(error: unknown): string {
  const status = errorStatus(error);
  if (status === 501) return 'Relay is disabled, so the node keeps no pool.';
  if (status === 503) return 'The pool is not open yet.';
  return error instanceof Error ? error.message : 'Unknown error';
}

// The unconfirmed transactions of the pool, all of them, in pool order.
export function TransactionsPage() {
  usePageTitle('Transactions');
  const { data: txs, isLoading, isError, error } = useUnconfirmedTransactionDetails();
  const { data: hashes } = useUnconfirmedTransactionHashes();
  const clusters = new Set(txs?.map((tx) => tx.cluster)).size;
  const size = txs?.reduce((sum, tx) => sum + tx.virtual, 0) ?? 0;
  const fees = txs?.reduce((sum, tx) => sum + tx.fee, 0) ?? 0;
  const rows = useMemo(
    () =>
      txs?.map((tx, index) => ({
        position: index + 1,
        hash: hashes?.at(index),
        virtual: tx.virtual,
        fee: tx.fee,
        cluster: tx.cluster,
        received: tx.time,
      })),
    [txs, hashes]
  );

  return (
    <div className="space-y-8">
      <header>
        <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
          Transactions
        </h1>
        <p className="text-textMuted text-sm">The unconfirmed transactions of the pool.</p>
      </header>

      {isError && (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Transactions unavailable</p>
          <p className="text-textMuted mt-1 break-words">{unavailable(error)}</p>
        </div>
      )}

      {isLoading && <Spinner label="Loading the pool…" />}

      {txs && (
        <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-4">
          <Item label="Transactions" value={formatInteger(txs.length)} />
          <Item label="Clusters" value={formatInteger(clusters)} />
          <Item label="Virtual size" value={`${formatInteger(size)} vB`} />
          <Item label="Fees" value={formatSats(fees)} />
        </section>
      )}

      {!isError && <TransactionTable rows={rows} isLoading={isLoading} />}
    </div>
  );
}

function Item({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <p className="text-textMuted text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary mt-1 font-medium tabular-nums">{value}</p>
    </div>
  );
}
