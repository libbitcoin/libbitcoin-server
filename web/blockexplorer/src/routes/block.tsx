import { type ReactNode, useEffect, useLayoutEffect, useRef } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import { useBlockHeader, useBlockStats, useBlockTransactions } from '@/lib/hooks/useBlockchainData';
import { useBlockchainClient, type TransactionSummary } from '@/lib/api/client';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { usePaginatedList } from '@/lib/hooks/usePaginatedList';
import { useWindowScrollToBottom } from '@/lib/hooks/useWindowScrollToBottom';
import { blockByHashPath, blockValidationPath, transactionPath } from '@/lib/utils/routes';
import {
  formatBtc,
  formatBytes,
  formatHex,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
  satsToBtc,
} from '@/lib/utils/format';
import { HashText } from '@/components/hash-text';
import { BlockSizeBar } from '@/components/block-size-bar';
import { BlockValidationInspector } from '@/components/block-validation-inspector';
import { SaveBookmarkButton } from '@/components/save-bookmark';

const TRANSACTION_PAGE_SIZE = 25;

const transactionKey = (tx: TransactionSummary) => tx.txid;

export function BlockPage() {
  const { hash, height } = useParams<{ hash?: string; height?: string }>();
  const identifier = hash ?? height ?? 'latest';
  const navigate = useNavigate();
  const location = useLocation();
  const inspectValidation = location.pathname.endsWith('/header/context');
  const client = useBlockchainClient();
  const now = useNow();
  const {
    data: block,
    isLoading: blockLoading,
    isError: blockIsError,
    error: blockError,
  } = useBlockHeader(identifier);
  usePageTitle(block ? `Block #${String(block.height)}` : 'Block');
  const isLibbitcoin = client.metadata.id === 'libbitcoin';
  const statsTarget = isLibbitcoin && block?.hash ? block.hash : '';
  const { data: stats, isLoading: statsLoading } = useBlockStats(statsTarget, {
    enabled: isLibbitcoin && Boolean(block?.hash),
  });
  const {
    data: initialTransactions,
    isLoading: initialTransactionsLoading,
    error: initialTransactionsError,
  } = useBlockTransactions(identifier, 0);
  const previousBlockHashRef = useRef<string | null>(null);
  const nextBlockHashRef = useRef<string | null>(null);
  const {
    items: transactions,
    isFetchingMore,
    hasMore,
    loadMore,
  } = usePaginatedList<TransactionSummary>({
    initialItems: initialTransactions,
    resetDeps: [client, identifier],
    getItemKey: transactionKey,
    fetchNextPage: (items) => client.getBlockTransactions(block?.hash ?? identifier, items.length),
    canFetchMore: (items) => {
      if (!block) return false;
      if (typeof block.txCount === 'number') return items.length < block.txCount;
      return true;
    },
    pageSize: TRANSACTION_PAGE_SIZE,
  });

  useLayoutEffect(() => {
    previousBlockHashRef.current = block?.previousBlockHash ?? null;
    nextBlockHashRef.current = block?.nextBlockHash ?? null;
  }, [block?.nextBlockHash, block?.previousBlockHash]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.defaultPrevented) return;
      if (event.repeat) return;
      if (event.metaKey || event.ctrlKey || event.altKey || event.shiftKey) return;

      const target = event.target;
      if (target instanceof HTMLElement) {
        const tag = target.tagName;
        if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
        if (target.isContentEditable) return;
      }

      if (event.key === 'ArrowLeft' && previousBlockHashRef.current) {
        event.preventDefault();
        void navigate(blockByHashPath(previousBlockHashRef.current));
        return;
      }

      if (event.key === 'ArrowRight' && nextBlockHashRef.current) {
        event.preventDefault();
        void navigate(blockByHashPath(nextBlockHashRef.current));
      }
    };

    globalThis.window.addEventListener('keydown', handleKeyDown);
    return () => {
      globalThis.window.removeEventListener('keydown', handleKeyDown);
    };
  }, [navigate]);

  useEffect(() => {
    if (!block?.hash) return;
    if (hash && hash !== block.hash) {
      void navigate(
        inspectValidation ? blockValidationPath(block.hash) : blockByHashPath(block.hash),
        { replace: true }
      );
    }
  }, [block?.hash, hash, inspectValidation, navigate]);

  useWindowScrollToBottom({
    enabled: hasMore && !isFetchingMore && transactions.length > 0,
    onBottomReached: () => {
      void loadMore();
    },
  });

  const timestamp = block?.timestamp;
  const feesSats = stats?.feesSats ?? block?.feesSats;
  const subsidySats = stats?.subsidySats ?? block?.subsidySats;
  const weight = stats?.weight ?? block?.weight;
  const size = stats?.size ?? block?.size;
  const showStatsPlaceholder = isLibbitcoin && Boolean(block) && statsLoading;

  return (
    <div className="space-y-8">
      {blockIsError ? (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Could not load block</p>
          <p className="text-textMuted mt-1 break-words">
            {blockError instanceof Error ? blockError.message : 'Unknown error'}
          </p>
        </div>
      ) : null}
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Block</p>
          <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
            {block ? `#${String(block.height)}` : '—'}
          </h1>
          <p className="text-textPrimary mt-2 font-mono text-xs break-all">{block?.hash ?? '—'}</p>
          <p className="text-textMuted text-sm">
            {formatRelativeTime(timestamp, now)} · {formatTimestamp(timestamp)}
          </p>
        </div>
        <div className="flex flex-wrap items-center gap-3">
          {block && (
            <SaveBookmarkButton
              target={{ kind: 'block', hash: block.hash, height: block.height }}
            />
          )}
          {block?.previousBlockHash ? (
            <Link
              to={blockByHashPath(block.previousBlockHash)}
              className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm"
            >
              ← Previous
            </Link>
          ) : null}
          {block?.nextBlockHash ? (
            <Link
              to={blockByHashPath(block.nextBlockHash)}
              className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm"
            >
              Next →
            </Link>
          ) : null}
        </div>
      </header>

      <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-3">
        <InfoItem
          label="Transactions"
          value={formatNumber(block?.txCount, { maximumFractionDigits: 0 })}
        />
        <InfoItem label="Size" value={formatBytes(size)} />
        <InfoItem
          label="Weight"
          value={
            typeof weight === 'number'
              ? formatNumber(weight, { maximumFractionDigits: 0 })
              : showStatsPlaceholder
                ? '…'
                : '—'
          }
        />
        <InfoItem
          label="Fees"
          value={
            typeof feesSats === 'number' ? formatSats(feesSats) : showStatsPlaceholder ? '…' : '—'
          }
        />
        <InfoItem
          label="Subsidy"
          value={
            typeof subsidySats === 'number'
              ? formatSats(subsidySats)
              : showStatsPlaceholder
                ? '…'
                : '—'
          }
        />
        <InfoItem label="Difficulty" value={formatNumber(block?.difficulty ?? undefined)} />
        <InfoItem label="Nonce" value={formatHex(block?.nonce)} />
        <InfoItem label="Bits" value={formatHex(block?.bits)} />
        <InfoItem label="Version" value={formatHex(block?.version)} />
        <InfoItem
          label="Merkle root"
          value={
            <span className="text-textPrimary font-mono text-xs break-all">
              {block?.merkleRoot ?? '—'}
            </span>
          }
        />
      </section>

      {(typeof size === 'number' ||
        (typeof stats?.nominal === 'number' && typeof weight === 'number')) && (
        <BlockSizeBar weight={weight} nominal={stats?.nominal} size={size} />
      )}

      {block && (
        <BlockValidationInspector
          key={JSON.stringify([
            client.metadata.id,
            client.metadata.endpoint,
            block.hash,
            inspectValidation,
          ])}
          blockHash={block.hash}
          initiallyExpanded={inspectValidation}
        />
      )}

      <section>
        <header className="mb-4 flex items-center justify-between">
          <h2 className="text-accent text-lg font-semibold">Transactions</h2>
          <p className="text-textMuted text-sm">Showing {transactions.length} transactions</p>
        </header>
        <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
          <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
            <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
              <tr>
                <th className="px-4 py-3 text-left">TxID</th>
                <th className="px-4 py-3 text-right">Outputs</th>
                <th className="px-4 py-3 text-right">Fee</th>
                <th className="px-4 py-3 text-right">Size</th>
              </tr>
            </thead>
            <tbody>
              {initialTransactionsLoading && transactions.length === 0 && (
                <tr>
                  <td colSpan={4} className="text-textMuted px-4 py-6 text-center">
                    Loading transactions…
                  </td>
                </tr>
              )}
              {initialTransactionsError && transactions.length === 0 && (
                <tr>
                  <td colSpan={4} className="text-accentSecondary px-4 py-6 text-center">
                    Could not load transactions.
                  </td>
                </tr>
              )}
              {!initialTransactionsLoading &&
                !initialTransactionsError &&
                transactions.length === 0 &&
                !blockLoading && (
                  <tr>
                    <td colSpan={4} className="text-textMuted px-4 py-6 text-center">
                      No transactions available.
                    </td>
                  </tr>
                )}
              {transactions.map((tx) => (
                <tr key={tx.txid} className="hover:bg-accentSecondary/10">
                  <td className="px-4 py-3">
                    <Link
                      to={transactionPath(tx.txid)}
                      className="text-accent hover:text-accentSecondary font-mono"
                    >
                      <HashText value={tx.txid} truncate={12} />
                    </Link>
                  </td>
                  <td className="text-textPrimary px-4 py-3 text-right">
                    {formatBtc(satsToBtc(tx.totalOutput))}
                  </td>
                  <td className="text-textPrimary px-4 py-3 text-right">
                    {formatSats(tx.feeSats)}
                  </td>
                  <td className="text-textPrimary px-4 py-3 text-right">
                    {typeof tx.size === 'number' && tx.size > 0
                      ? `${formatNumber(tx.size)} bytes`
                      : '—'}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        {hasMore && (
          <div className="mt-4 flex justify-center">
            <button
              type="button"
              onClick={() => {
                void loadMore();
              }}
              disabled={isFetchingMore}
              className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary rounded-md border px-4 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-60"
            >
              {isFetchingMore ? 'Loading more…' : 'Load more transactions'}
            </button>
          </div>
        )}
      </section>
    </div>
  );
}

function InfoItem({ label, value }: { label: string; value: ReactNode }) {
  return (
    <div className="space-y-1">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary text-sm">{value}</p>
    </div>
  );
}
