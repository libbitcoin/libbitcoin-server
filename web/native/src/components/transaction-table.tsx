import { useState } from 'react';
import { Link } from 'react-router-dom';
import { FitHash } from '@/components/fit-hash';
import { SortHeader, useSortedRows } from '@/components/sort-header';
import { Spinner } from '@/components/spinner';
import { useWindowScrollToBottom } from '@/lib/hooks/useWindowScrollToBottom';
import { useNow } from '@/lib/hooks/useNow';
import { formatInteger, formatRate, formatShortRelativeTime } from '@/lib/utils/format';
import { transactionPath } from '@/lib/utils/routes';

export interface TransactionTableRow {
  position: number;
  hash?: string;
  virtual: number;
  fee: number;
  cluster: number;
  // When the tx was received, in seconds, where known.
  received?: number;
}

const PAGE_SIZE = 25;

// The coinbase of a template, which has no tx yet.
const NULL_HASH = /^0{64}$/u;

const rate = (row: TransactionTableRow) => (row.virtual === 0 ? 0 : row.fee / row.virtual);

const values = {
  position: (row: TransactionTableRow) => row.position,
  txid: (row: TransactionTableRow) => row.hash,
  received: (row: TransactionTableRow) => row.received,
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
  const now = useNow();
  const timed = rows?.some((row) => row.received !== undefined) ?? false;
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
        <table className="divide-accentSecondary/30 w-full min-w-[48rem] table-fixed divide-y text-sm">
          <colgroup>
            <col className="w-24" />
            <col />
            {timed && <col className="w-28" />}
            <col className="w-28" />
            <col className="w-24" />
            <col className="w-28" />
            <col className="w-24" />
          </colgroup>
          <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
            <tr>
              <SortHeader label="Position" column="position" sort={sort} onSort={toggle} />
              <SortHeader label="TxID" column="txid" sort={sort} onSort={toggle} />
              {timed && (
                <SortHeader label="Received" column="received" sort={sort} onSort={toggle} />
              )}
              <SortHeader label="Fee" column="fee" sort={sort} onSort={toggle} align="right" />
              <SortHeader label="Size" column="virtual" sort={sort} onSort={toggle} align="right" />
              <SortHeader label="Rate" column="rate" sort={sort} onSort={toggle} align="right" />
              <SortHeader label="Cluster" column="cluster" sort={sort} onSort={toggle} align="right" />
            </tr>
          </thead>
          <tbody className="divide-accentSecondary/20 divide-y tabular-nums">
            {!rows && (
              <tr>
                <td colSpan={7} className="text-textMuted px-4 py-6 text-center">
                  {isError ? (
                    'Could not load transactions.'
                  ) : isLoading ? (
                    <Spinner label="Loading transactions…" />
                  ) : (
                    'No transactions available.'
                  )}
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
                <td data-fit className="overflow-hidden px-4 py-2 whitespace-nowrap">
                  {row.hash && !NULL_HASH.test(row.hash) ? (
                    <Link
                      to={transactionPath(row.hash)}
                      className="text-accent hover:text-accentSecondary font-mono"
                    >
                      <FitHash value={row.hash} />
                    </Link>
                  ) : row.hash ? (
                    <FitHash value={row.hash} className="text-textMuted font-mono" />
                  ) : (
                    '—'
                  )}
                </td>
                {timed && (
                  <td className="text-textMuted px-4 py-2 whitespace-nowrap">
                    {formatShortRelativeTime(row.received, now)}
                  </td>
                )}
                <td className="px-4 py-2 text-right">{formatInteger(row.fee)} sat</td>
                <td className="px-4 py-2 text-right">{formatInteger(row.virtual)} vB</td>
                <td className="px-4 py-2 text-right">{formatRate(rate(row))}</td>
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
