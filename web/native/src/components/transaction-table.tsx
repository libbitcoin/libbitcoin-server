import { useState } from 'react';
import { Link } from 'react-router-dom';
import { HashText } from '@/components/hash-text';
import { SortHeader, useSortedRows } from '@/components/sort-header';
import { useWindowScrollToBottom } from '@/lib/hooks/useWindowScrollToBottom';
import { formatInteger } from '@/lib/utils/format';
import { transactionPath } from '@/lib/utils/routes';

export interface TransactionTableRow {
  position: number;
  hash?: string;
  virtual: number;
  fee: number;
  cluster: number;
}

const PAGE_SIZE = 25;

// The coinbase of a template, which has no tx yet.
const NULL_HASH = /^0{64}$/u;

const rate = (row: TransactionTableRow) => (row.virtual === 0 ? 0 : row.fee / row.virtual);

const values = {
  position: (row: TransactionTableRow) => row.position,
  txid: (row: TransactionTableRow) => row.hash,
  virtual: (row: TransactionTableRow) => row.virtual,
  fee: (row: TransactionTableRow) => row.fee,
  rate,
  cluster: (row: TransactionTableRow) => row.cluster,
};

// The txs of a block or template, sortable by any column, shown a page at a
// time as the page scrolls or more are asked for.
export function TransactionTable({
  rows,
  isLoading = false,
  isError = false,
  onPosition,
}: {
  rows?: TransactionTableRow[];
  isLoading?: boolean;
  isError?: boolean;
  // Shows the tx at a position in the details.
  onPosition?: (position: number) => void;
}) {
  const { sorted, sort, toggle } = useSortedRows(rows ?? [], values);
  const [shown, setShown] = useState(PAGE_SIZE);
  const more = shown < sorted.length;
  const showMore = () => {
    setShown((current) => current + PAGE_SIZE);
  };
  useWindowScrollToBottom({ enabled: more, onBottomReached: showMore });
  return (
    <section id="transactions" className="scroll-mt-24">
      <header className="mb-4 flex items-center justify-between">
        <h2 className="text-accent text-lg font-semibold">Transactions</h2>
        {rows && (
          <p className="text-textMuted text-sm">
            Showing {formatInteger(Math.min(shown, rows.length))} of {formatInteger(rows.length)}{' '}
            transactions
          </p>
        )}
      </header>
      <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
        <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
          <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
            <tr>
              <SortHeader label="Position" column="position" sort={sort} onSort={toggle} />
              <SortHeader label="TxID" column="txid" sort={sort} onSort={toggle} />
              <SortHeader label="Virtual size" column="virtual" sort={sort} onSort={toggle} align="right" />
              <SortHeader label="Fee" column="fee" sort={sort} onSort={toggle} align="right" />
              <SortHeader label="Rate" column="rate" sort={sort} onSort={toggle} align="right" />
              <SortHeader label="Cluster" column="cluster" sort={sort} onSort={toggle} align="right" />
            </tr>
          </thead>
          <tbody className="divide-accentSecondary/20 divide-y tabular-nums">
            {!rows && (
              <tr>
                <td colSpan={6} className="text-textMuted px-4 py-6 text-center">
                  {isError
                    ? 'Could not load transactions.'
                    : isLoading
                      ? 'Loading transactions…'
                      : 'No transactions available.'}
                </td>
              </tr>
            )}
            {sorted.slice(0, shown).map((row) => (
              <tr key={row.position} className="hover:bg-accentSecondary/10">
                <td className="px-4 py-2">
                  {onPosition ? (
                    <button
                      type="button"
                      title="Show in details"
                      onClick={() => {
                        onPosition(row.position);
                      }}
                      className="text-accent hover:text-accentSecondary hover:underline"
                    >
                      {formatInteger(row.position)}
                    </button>
                  ) : (
                    formatInteger(row.position)
                  )}
                </td>
                <td className="px-4 py-2">
                  {row.hash && !NULL_HASH.test(row.hash) ? (
                    <Link
                      to={transactionPath(row.hash)}
                      className="text-accent hover:text-accentSecondary font-mono"
                    >
                      <HashText value={row.hash} />
                    </Link>
                  ) : row.hash ? (
                    <HashText value={row.hash} className="text-textMuted font-mono" />
                  ) : (
                    '—'
                  )}
                </td>
                <td className="px-4 py-2 text-right">{formatInteger(row.virtual)} vB</td>
                <td className="px-4 py-2 text-right">{formatInteger(row.fee)} sat</td>
                <td className="px-4 py-2 text-right">{rate(row).toFixed(2)} sat/vB</td>
                <td className="px-4 py-2 text-right">{formatInteger(row.cluster)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      {more && (
        <div className="mt-4 flex justify-center">
          <button
            type="button"
            onClick={showMore}
            className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary rounded-md border px-4 py-2 text-sm font-medium"
          >
            Load more transactions
          </button>
        </div>
      )}
    </section>
  );
}
