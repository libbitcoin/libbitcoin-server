import { useEffect, useMemo, useRef, useState } from 'react';

export interface UsePaginatedListOptions<T> {
  /** First page of items (typically from React Query); the list renders these first. */
  initialItems?: T[];
  /** Values identifying the list (client, route param, …); the list resets when any change. */
  resetDeps: readonly unknown[];
  /** Stable identity per item, used to drop duplicates across pages. Must not change identity per render. */
  getItemKey: (item: T) => string;
  /** Fetches the page after `items`. Empty or fully-duplicate pages mark the end. */
  fetchNextPage: (items: T[]) => Promise<T[]>;
  /** Whether more pages may exist after `items` (e.g. compare against a known total). */
  canFetchMore?: (items: T[]) => boolean;
  /** When set, pages shorter than this mark the end of the list. */
  pageSize?: number;
}

interface FetchedPagesState<T> {
  /** The resetDeps snapshot these pages were fetched under; mismatching state is ignored. */
  deps: readonly unknown[];
  items: T[];
  reachedEnd: boolean;
  isFetchingMore: boolean;
  error: string | null;
}

function areDepsEqual(a: readonly unknown[], b: readonly unknown[]) {
  return a.length === b.length && a.every((value, index) => Object.is(value, b[index]));
}

function createEmptyState<T>(deps: readonly unknown[]): FetchedPagesState<T> {
  return { deps, items: [], reachedEnd: false, isFetchingMore: false, error: null };
}

/**
 * Accumulates a paged list: renders the initial page followed by deduplicated
 * pages fetched on demand. All fetched state is tagged with the resetDeps it
 * was loaded under, so when the list identity changes the old pages become
 * invisible immediately — no reset effect, and a stale list is never painted.
 */
export function usePaginatedList<T>({
  initialItems,
  resetDeps,
  getItemKey,
  fetchNextPage,
  canFetchMore,
  pageSize,
}: UsePaginatedListOptions<T>) {
  const requestToken = useRef(0);
  const fetchMoreLock = useRef(false);
  const [fetched, setFetched] = useState<FetchedPagesState<T>>(() => createEmptyState(resetDeps));

  // State fetched under a previous list identity is ignored here and lazily
  // replaced by the next write, rather than cleared with an effect.
  const live = areDepsEqual(fetched.deps, resetDeps) ? fetched : null;

  // Invalidate in-flight pages once the list identity changes. Their writes are
  // already invisible via the deps tag; the token stops a late arrival from
  // replacing state written for the new identity.
  useEffect(() => {
    requestToken.current += 1;
    fetchMoreLock.current = false;
    // The caller-supplied resetDeps array is the dependency list by design.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, resetDeps);

  const fetchedItems = live?.items;
  const items = useMemo(() => {
    const seeded = initialItems ?? [];
    if (!fetchedItems || fetchedItems.length === 0) return seeded;
    const known = new Set(seeded.map((entry) => getItemKey(entry)));
    return [...seeded, ...fetchedItems.filter((entry) => !known.has(getItemKey(entry)))];
  }, [fetchedItems, getItemKey, initialItems]);

  const isFetchingMore = live?.isFetchingMore ?? false;
  const loadMoreError = live?.error ?? null;
  // A first page shorter than a full page means there is nothing after it.
  const initialPageIsShort =
    pageSize !== undefined && initialItems !== undefined && initialItems.length < pageSize;
  const hasMore =
    !(live?.reachedEnd ?? false) &&
    !initialPageIsShort &&
    (canFetchMore ? canFetchMore(items) : true);

  const loadMore = async () => {
    if (!hasMore || isFetchingMore || fetchMoreLock.current) return;
    const token = requestToken.current;
    const depsAtCall = resetDeps;
    const itemsAtCall = items;
    fetchMoreLock.current = true;
    setFetched((prev) => ({
      ...(areDepsEqual(prev.deps, depsAtCall) ? prev : createEmptyState<T>(depsAtCall)),
      isFetchingMore: true,
      error: null,
    }));
    try {
      const next = await fetchNextPage(itemsAtCall);
      if (token !== requestToken.current) return;
      const known = new Set(itemsAtCall.map((entry) => getItemKey(entry)));
      const fresh = next.filter((entry) => !known.has(getItemKey(entry)));
      const ended =
        next.length === 0 ||
        fresh.length === 0 ||
        (pageSize !== undefined && next.length < pageSize);
      setFetched((prev) => {
        const base = areDepsEqual(prev.deps, depsAtCall) ? prev : createEmptyState<T>(depsAtCall);
        return {
          ...base,
          items: fresh.length > 0 ? [...base.items, ...fresh] : base.items,
          reachedEnd: base.reachedEnd || ended,
          isFetchingMore: false,
        };
      });
    } catch (error) {
      if (token !== requestToken.current) return;
      console.warn('Failed to load the next page', error);
      const message = error instanceof Error ? error.message : 'Failed to load more items';
      setFetched((prev) => ({
        ...(areDepsEqual(prev.deps, depsAtCall) ? prev : createEmptyState<T>(depsAtCall)),
        isFetchingMore: false,
        error: message,
      }));
    } finally {
      fetchMoreLock.current = false;
    }
  };

  return { items, isFetchingMore, hasMore, loadMore, loadMoreError };
}
