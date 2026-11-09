import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { useBlockchainClient, type BlockSummary } from '@/lib/api/client';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { usePaginatedList } from '@/lib/hooks/usePaginatedList';
import { useWindowScrollToBottom } from '@/lib/hooks/useWindowScrollToBottom';
import { formatBytes, formatNumber, formatRelativeTime } from '@/lib/utils/format';
import { HashText } from '@/components/hash-text';
import { blockByHashPath, blockByHeightPath } from '@/lib/utils/routes';

const PAGE_SIZE = 10;

const blockKey = (block: BlockSummary) => block.hash;

export function BlocksPage() {
  usePageTitle('Blocks');
  const client = useBlockchainClient();
  const now = useNow();
  const [initialBlocks, setInitialBlocks] = useState<BlockSummary[] | undefined>();
  const [isLoading, setIsLoading] = useState(true);
  const [initialError, setInitialError] = useState<string | null>(null);

  useEffect(() => {
    let isMounted = true;
    setInitialBlocks(undefined);
    setInitialError(null);
    setIsLoading(true);
    const loadInitial = async () => {
      try {
        const latest = await client.getLatestBlocks(PAGE_SIZE);
        if (!isMounted) return;
        setInitialBlocks(latest);
      } catch (error_) {
        if (!isMounted) return;
        setInitialError(error_ instanceof Error ? error_.message : 'Failed to load blocks');
      } finally {
        if (isMounted) {
          setIsLoading(false);
        }
      }
    };
    void loadInitial();
    return () => {
      isMounted = false;
    };
  }, [client]);

  const {
    items: blocks,
    isFetchingMore,
    hasMore,
    loadMore,
    loadMoreError,
  } = usePaginatedList<BlockSummary>({
    initialItems: initialBlocks,
    resetDeps: [client],
    getItemKey: blockKey,
    fetchNextPage: (items) => {
      const last = items.at(-1);
      if (!last) return Promise.resolve([]);
      return client.getBlocksBefore(last.height - 1, PAGE_SIZE);
    },
    canFetchMore: (items) => items.length > 0,
    pageSize: PAGE_SIZE,
  });
  const error = initialError ?? loadMoreError;

  useWindowScrollToBottom({
    enabled: hasMore && !isLoading && error === null && !isFetchingMore,
    onBottomReached: () => {
      void loadMore();
    },
  });

  return (
    <div className="space-y-8">
      <header className="flex items-center justify-between">
        <div>
          <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
            Blocks
          </h1>
          <p className="text-textMuted text-sm">Browse recent blocks and load more as needed.</p>
        </div>
      </header>

      <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
        <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
          <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
            <tr>
              <th className="px-4 py-3 text-left">Height</th>
              <th className="px-4 py-3 text-left">Hash</th>
              <th className="px-4 py-3 text-left">Age</th>
              <th className="px-4 py-3 text-right">Tx</th>
              <th className="px-4 py-3 text-right">Size</th>
            </tr>
          </thead>
          <tbody>
            {isLoading && (
              <tr>
                <td colSpan={5} className="text-textMuted px-4 py-6 text-center">
                  Loading blocks…
                </td>
              </tr>
            )}
            {error && !isLoading && (
              <tr>
                <td colSpan={5} className="text-accentSecondary px-4 py-6 text-center">
                  {error}
                </td>
              </tr>
            )}
            {!isLoading && !error && blocks.length === 0 && (
              <tr>
                <td colSpan={5} className="text-textMuted px-4 py-6 text-center">
                  No blocks available.
                </td>
              </tr>
            )}
            {!isLoading &&
              !error &&
              blocks.map((block) => (
                <tr key={block.hash} className="hover:bg-accentSecondary/10">
                  <td className="px-4 py-3">
                    <Link
                      to={blockByHeightPath(block.height)}
                      className="text-accent hover:text-accentSecondary font-medium"
                    >
                      {formatNumber(block.height, { maximumFractionDigits: 0 })}
                    </Link>
                  </td>
                  <td className="text-textPrimary px-4 py-3">
                    <Link to={blockByHashPath(block.hash)} className="text-textPrimary">
                      <HashText value={block.hash} className="font-mono" />
                    </Link>
                  </td>
                  <td className="text-textMuted px-4 py-3">
                    {formatRelativeTime(block.timestamp, now)}
                  </td>
                  <td className="text-textPrimary px-4 py-3 text-right">
                    {formatNumber(block.txCount)}
                  </td>
                  <td className="text-textPrimary px-4 py-3 text-right">
                    {formatBytes(block.size)}
                  </td>
                </tr>
              ))}
          </tbody>
        </table>
      </div>

      {hasMore && !isLoading && !error && (
        <div className="flex justify-center">
          <button
            type="button"
            onClick={() => {
              void loadMore();
            }}
            disabled={isFetchingMore}
            className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary rounded-md border px-4 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-60"
          >
            {isFetchingMore ? 'Loading more…' : 'Load more blocks'}
          </button>
        </div>
      )}
    </div>
  );
}
