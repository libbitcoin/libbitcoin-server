import { useCallback, useEffect, useMemo, useRef, useState, useSyncExternalStore } from 'react';
import {
  type Query,
  type QueryClient,
  type QueryFunctionContext,
  type UseQueryOptions,
  useQuery,
  useQueryClient,
} from '@tanstack/react-query';
import {
  AddressActivityItem,
  AddressSummary,
  AddressUtxo,
  BlockchainClient,
  BlockchainClientConfiguration,
  BlockchainEventStatus,
  BlockHeaderDetail,
  BlockValidation,
  BlockStats,
  BlockSummary,
  BlockchainEvent,
  ChainBranch,
  HeaderTop,
  PoolFees,
  RecentMiners,
  TemplateTransaction,
  TransactionInput,
  TransactionSizes,
  TransactionSummary,
  TransactionOutputSpend,
  useBlockchainClient,
} from '@/lib/api/client';
import type { Miner } from '@/lib/utils/miner';

type BlockchainQueryScope = {
  endpoint: string;
};

const DEFAULT_LIVE_BLOCK_LIMIT = 10;

type BlockRefreshMode = 'always' | 'while-unconfirmed';

interface BlockchainQueryMeta extends Record<string, unknown> {
  blockRefresh?: BlockRefreshMode;
}

interface BlockchainQueryOptions<T> {
  queryKey: readonly unknown[];
  queryFn: (client: BlockchainClient, context: QueryFunctionContext) => Promise<T>;
  enabled?: boolean;
  retry?: boolean | number | ((failureCount: number, error: Error) => boolean);
  staleTime?: number;
  refetchInterval?: UseQueryOptions<T>['refetchInterval'];
  blockRefresh?: BlockRefreshMode;
}

function getBlockchainQueryScope(client: BlockchainClient): BlockchainQueryScope {
  return { endpoint: client.metadata.endpoint ?? '' };
}

function isMatchingQueryScope(value: unknown, scope: BlockchainQueryScope) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return false;
  const candidate = value as Partial<BlockchainQueryScope>;
  return candidate.endpoint === scope.endpoint;
}

function getQueryLimit(value: unknown, fallback: number) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return fallback;
  const limit = (value as { limit?: unknown }).limit;
  return typeof limit === 'number' && Number.isFinite(limit) && limit > 0 ? limit : fallback;
}

function updateLatestBlocksCache(
  queryClient: QueryClient,
  queryScope: BlockchainQueryScope,
  block: BlockSummary
) {
  const queries = queryClient.getQueryCache().findAll({
    predicate: (query) => {
      const key = query.queryKey;
      return (
        key.length >= 4 &&
        isMatchingQueryScope(key[0], queryScope) &&
        key[1] === 'blocks' &&
        key[2] === 'latest'
      );
    },
  });

  for (const query of queries) {
    const limit = getQueryLimit(query.queryKey[3], DEFAULT_LIVE_BLOCK_LIMIT);
    queryClient.setQueryData<BlockSummary[]>(query.queryKey, (prev) => {
      const existing = prev ?? [];
      const next = [block, ...existing.filter((item) => item.hash !== block.hash)];
      return next.slice(0, limit);
    });
  }

  queryClient.setQueryData<BlockSummary[]>(
    [queryScope, 'blocks', 'latest', { limit: DEFAULT_LIVE_BLOCK_LIMIT }],
    (prev) => {
      const existing = prev ?? [];
      const next = [block, ...existing.filter((item) => item.hash !== block.hash)];
      return next.slice(0, DEFAULT_LIVE_BLOCK_LIMIT);
    }
  );
}

// The strong chain's latest blocks, with those it displaced kept as weak.
function mergeLatestBlocks(strong: BlockSummary[], previous: BlockSummary[]): BlockSummary[] {
  const hashes = new Set(strong.map((block) => block.hash));
  const lowest = strong.at(-1)?.height ?? 0;
  const weak = previous
    .filter((block) => !hashes.has(block.hash) && block.height >= lowest)
    .map((block) => ({ ...block, weak: true }));
  return [...strong, ...weak].sort(
    (left, right) => right.height - left.height || Number(left.weak ?? false) - Number(right.weak ?? false)
  );
}

async function reconcileLatestBlocks(
  queryClient: QueryClient,
  queryScope: BlockchainQueryScope,
  client: BlockchainClient
) {
  const queries = queryClient.getQueryCache().findAll({
    predicate: (query) => {
      const key = query.queryKey;
      return (
        key.length >= 4 &&
        isMatchingQueryScope(key[0], queryScope) &&
        key[1] === 'blocks' &&
        key[2] === 'latest'
      );
    },
  });

  for (const query of queries) {
    const limit = getQueryLimit(query.queryKey[3], DEFAULT_LIVE_BLOCK_LIMIT);
    const strong = await client.getLatestBlocks(limit);
    queryClient.setQueryData<BlockSummary[]>(query.queryKey, (previous) =>
      mergeLatestBlocks(strong, previous ?? [])
    );
  }
}

function shouldRefreshQueryAfterBlock(query: Query, queryScope: BlockchainQueryScope) {
  if (!isMatchingQueryScope(query.queryKey[0], queryScope)) return false;

  const meta = query.meta;
  if (meta?.blockRefresh === 'always') return true;
  if (meta?.blockRefresh !== 'while-unconfirmed') return false;

  const data = query.state.data;
  if (!data || typeof data !== 'object' || Array.isArray(data)) return true;
  return (data as Partial<TransactionSummary>).status !== 'confirmed';
}

function invalidateBlockDependentQueries(
  queryClient: QueryClient,
  queryScope: BlockchainQueryScope
) {
  return queryClient.invalidateQueries({
    predicate: (query) => shouldRefreshQueryAfterBlock(query, queryScope),
    refetchType: 'active',
  });
}

function useBlockchainQuery<T>({
  queryKey,
  queryFn,
  enabled,
  retry,
  staleTime,
  refetchInterval,
  blockRefresh,
}: BlockchainQueryOptions<T>) {
  const client = useBlockchainClient();

  // Keep data isolated by node endpoint without tying it to the client instance.
  return useQuery<T>({
    queryKey: [getBlockchainQueryScope(client), ...queryKey],
    queryFn: (context) => queryFn(client, context),
    enabled,
    ...(retry === undefined ? {} : { retry }),
    staleTime,
    refetchInterval,
    meta: blockRefresh ? ({ blockRefresh } satisfies BlockchainQueryMeta) : undefined,
  });
}

export function useLatestBlocks(limit = 10) {
  return useBlockchainQuery<BlockSummary[]>({
    queryKey: ['blocks', 'latest', { limit }],
    queryFn: (client) => client.getLatestBlocks(limit),
  });
}

export function useServerConfiguration() {
  return useBlockchainQuery<BlockchainClientConfiguration>({
    queryKey: ['configuration'],
    queryFn: (client) => client.getConfiguration(),
    staleTime: 60_000,
    refetchInterval: (query) => (query.state.status === 'error' ? 15_000 : false),
  });
}

export function useFees() {
  return useBlockchainQuery<PoolFees>({
    queryKey: ['fees'],
    queryFn: (client) => client.getFees(),
    staleTime: 10_000,
    refetchInterval: 15_000,
  });
}

// Templates are refreshed by template notifications, kept once mined.
export function useHeaderTops() {
  return useBlockchainQuery<HeaderTop[]>({
    queryKey: ['headers', 'top'],
    queryFn: (client) => client.getHeaderTops(),
    blockRefresh: 'always',
  });
}

export function useBranches(options?: { enabled?: boolean }) {
  return useBlockchainQuery<ChainBranch[]>({
    queryKey: ['headers', 'branches'],
    queryFn: (client) => client.getBranches(),
    enabled: options?.enabled,
    blockRefresh: 'always',
  });
}

// The pool is large, so it is read again with each block, not each change.
export function useUnconfirmedTransactionHashes() {
  return useBlockchainQuery<string[]>({
    queryKey: ['txs', 'unconfirmed'],
    queryFn: (client) => client.getUnconfirmedTransactionHashes(),
    blockRefresh: 'always',
    retry: false,
  });
}

export function useUnconfirmedTransactionDetails() {
  return useBlockchainQuery<TemplateTransaction[]>({
    queryKey: ['txs', 'unconfirmed', 'details'],
    queryFn: (client) => client.getUnconfirmedTransactionDetails(),
    blockRefresh: 'always',
    retry: false,
  });
}

export function useTemplate(height: number) {
  return useBlockchainQuery<TemplateTransaction[]>({
    queryKey: ['template', { height }],
    queryFn: (client) => client.getTemplate(height),
    staleTime: Infinity,
    retry: false,
  });
}

// Template notifications arrive while this is mounted.
export function useTemplateNotifications() {
  const client = useBlockchainClient();
  useEffect(() => client.watchTemplates?.(), [client]);
}

// The height of the latest template notified, if any.
export function useLatestTemplateHeight(): number | undefined {
  const client = useBlockchainClient();
  const { data } = useQuery<number | null>({
    queryKey: [getBlockchainQueryScope(client), 'template', 'latest'],
    queryFn: () => null,
    enabled: false,
    staleTime: Infinity,
  });
  return data ?? undefined;
}

export function useBlockTransactionDetails(hash: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<TemplateTransaction[]>({
    queryKey: ['block', hash, 'txs', 'details'],
    queryFn: (client) => client.getBlockTransactionDetails({ chain: 'hash', id: hash }),
    enabled: options?.enabled,
    staleTime: Infinity,
  });
}

export function useBlockTransactionHashes(hash: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<string[]>({
    queryKey: ['block', hash, 'txs'],
    queryFn: (client) => client.getBlockTransactionHashes({ chain: 'hash', id: hash }),
    enabled: options?.enabled,
    staleTime: Infinity,
  });
}

export function useTemplateTransactionHashes(height: number) {
  return useBlockchainQuery<string[]>({
    queryKey: ['template', { height }, 'txs'],
    queryFn: (client) => client.getTemplateTransactionHashes(height),
    staleTime: Infinity,
    retry: false,
  });
}

// Loads the template at a height (and its tx hashes) into the cache ahead of
// its page, settling whether or not the template is found.
export function usePrefetchTemplate() {
  const client = useBlockchainClient();
  const queryClient = useQueryClient();
  return useCallback(
    async (height: number) => {
      const scope = getBlockchainQueryScope(client);
      await Promise.all([
        queryClient.prefetchQuery({
          queryKey: [scope, 'template', { height }],
          queryFn: () => client.getTemplate(height),
          staleTime: Infinity,
        }),
        queryClient.prefetchQuery({
          queryKey: [scope, 'template', { height }, 'txs'],
          queryFn: () => client.getTemplateTransactionHashes(height),
          staleTime: Infinity,
        }),
      ]);
    },
    [client, queryClient]
  );
}

export function useNetwork() {
  const { data } = useServerConfiguration();
  return { network: data?.network, params: data?.wallet };
}

export function useBlockHeader(
  hashOrHeight: string,
  options?: {
    enabled?: boolean;
    retry?: boolean | number | ((failureCount: number, error: Error) => boolean);
  }
) {
  return useBlockchainQuery<BlockHeaderDetail>({
    queryKey: ['block', hashOrHeight, 'header'],
    queryFn: (client) => client.getBlockHeader(hashOrHeight),
    enabled: (options?.enabled ?? true) && Boolean(hashOrHeight.trim()),
    retry: options?.retry,
    blockRefresh: 'always',
  });
}

export function useBlockStats(hashOrHeight: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<BlockStats>({
    queryKey: ['block', hashOrHeight, 'stats'],
    queryFn: (client) => client.getBlockStats(hashOrHeight),
    enabled: (options?.enabled ?? true) && Boolean(hashOrHeight.trim()),
    staleTime: Infinity,
  });
}

export function useBlockMiner(hash: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<Miner | null>({
    queryKey: ['block', hash, 'miner'],
    queryFn: (client) => client.getBlockMiner(hash),
    enabled: (options?.enabled ?? true) && Boolean(hash.trim()),
    staleTime: Infinity,
  });
}

export function useRecentMiners(count: number) {
  return useBlockchainQuery<RecentMiners>({
    queryKey: ['blocks', 'miners', { count }],
    queryFn: (client, { signal }) => client.getRecentMiners(count, signal),
    blockRefresh: 'always',
  });
}

export function useBlockValidation(hash: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<BlockValidation>({
    queryKey: ['block', hash, 'validation'],
    queryFn: (client) => client.getBlockValidation(hash),
    enabled: (options?.enabled ?? true) && Boolean(hash.trim()),
    blockRefresh: 'always',
  });
}

export function useBlockTransactions(hashOrHeight: string, startIndex = 0) {
  return useBlockchainQuery<TransactionSummary[]>({
    queryKey: ['block', hashOrHeight, 'transactions', { startIndex }],
    queryFn: (client) => client.getBlockTransactions(hashOrHeight, startIndex),
  });
}

export function useTransactionHeader(
  txid: string,
  options?: {
    enabled?: boolean;
    retry?: boolean | number | ((failureCount: number, error: Error) => boolean);
  }
) {
  return useBlockchainQuery<TransactionSummary>({
    queryKey: ['transaction', txid, 'header'],
    queryFn: (client) => client.getTransactionHeader(txid),
    enabled: (options?.enabled ?? true) && Boolean(txid.trim()),
    retry: options?.retry,
    blockRefresh: 'while-unconfirmed',
  });
}

export function useTransaction(txid: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<TransactionSummary>({
    queryKey: ['transaction', txid],
    queryFn: (client) => client.getTransaction(txid),
    enabled: options?.enabled ?? Boolean(txid.trim()),
    blockRefresh: 'while-unconfirmed',
  });
}

export function useTransactionPrunedInputs(txid: string, options?: { enabled?: boolean }) {
  return useBlockchainQuery<TransactionInput[]>({
    queryKey: ['transaction', txid, 'inputs', 'pruned'],
    queryFn: (client) => client.getPrunedInputs(txid),
    enabled: (options?.enabled ?? true) && Boolean(txid.trim()),
    staleTime: Infinity,
  });
}

export function useTransactionOutputScriptHex(
  txid: string,
  outputIndex: number,
  options?: { enabled?: boolean }
) {
  return useBlockchainQuery<string>({
    queryKey: ['transaction', txid, 'output', outputIndex, 'scriptHex'],
    queryFn: (client) => client.getTransactionOutputScriptHex(txid, outputIndex),
    enabled: (options?.enabled ?? true) && Boolean(txid.trim()),
    staleTime: Infinity,
  });
}

export function useTransactionInputScriptHex(
  txid: string,
  inputIndex: number,
  options?: { enabled?: boolean }
) {
  return useBlockchainQuery<string>({
    queryKey: ['transaction', txid, 'input', inputIndex, 'scriptHex'],
    queryFn: (client) => client.getTransactionInputScriptHex(txid, inputIndex),
    enabled: (options?.enabled ?? true) && Boolean(txid.trim()),
    staleTime: Infinity,
  });
}

export function useAddressSummary(address: string) {
  return useBlockchainQuery<AddressSummary>({
    queryKey: ['address', address, 'summary'],
    queryFn: (client) => client.getAddressSummary(address),
    enabled: Boolean(address.trim()),
    blockRefresh: 'always',
  });
}

export function useTransactionOutputSpends(txid: string, outputIndices: number[]) {
  return useBlockchainQuery<TransactionOutputSpend[]>({
    queryKey: ['transaction', txid, 'outputSpends', outputIndices],
    queryFn: (client) => client.getTransactionOutputSpends(txid, outputIndices),
    enabled: Boolean(txid.trim()) && outputIndices.length > 0,
    blockRefresh: 'always',
  });
}

export function useAddressUtxos(address: string) {
  return useBlockchainQuery<AddressUtxo[]>({
    queryKey: ['address', address, 'utxos'],
    queryFn: (client) => client.getAddressUtxos(address),
    enabled: Boolean(address.trim()),
    blockRefresh: 'always',
  });
}

export function useAddressTransactions(address: string, cursor?: string) {
  return useBlockchainQuery<AddressActivityItem[]>({
    queryKey: ['address', address, 'transactions', { cursor: cursor ?? null }],
    queryFn: (client) => client.getAddressTransactions(address, cursor),
    enabled: Boolean(address.trim()),
    blockRefresh: 'always',
  });
}

export function useTransactionSizes(txid: string) {
  return useBlockchainQuery<TransactionSizes>({
    queryKey: ['transaction', txid, 'sizes'],
    queryFn: (client) => client.getTransactionSizes(txid),
    staleTime: Infinity,
    retry: false,
  });
}

export interface RecentTransaction {
  txid: string;
  /** When its notification arrived, in seconds. */
  received: number;
}

const RECENT_TRANSACTIONS_INTERVAL_MS = 2000;

// The latest pooled txs, newest first, from their notifications. Arrivals are
// gathered and shown at intervals, as they can be many each second.
export function useRecentTransactions(limit: number): RecentTransaction[] {
  const client = useBlockchainClient();
  const [recent, setRecent] = useState<RecentTransaction[]>([]);

  useEffect(() => {
    setRecent([]);
    let arrived: RecentTransaction[] = [];
    let unsubscribe: (() => void) | undefined;

    // The stream is carried only while the page is showing.
    const sync = () => {
      if (document.hidden) {
        unsubscribe?.();
        unsubscribe = undefined;
      } else {
        unsubscribe ??= client.subscribeToTransactions?.((txid) => {
          arrived.unshift({ txid, received: Math.floor(Date.now() / 1000) });
          if (arrived.length > limit) arrived.length = limit;
        });
      }
    };

    sync();
    document.addEventListener('visibilitychange', sync);
    const timer = setInterval(() => {
      if (arrived.length === 0) return;
      const batch = arrived;
      arrived = [];
      setRecent((current) => [...batch, ...current].slice(0, limit));
    }, RECENT_TRANSACTIONS_INTERVAL_MS);
    return () => {
      clearInterval(timer);
      document.removeEventListener('visibilitychange', sync);
      unsubscribe?.();
    };
  }, [client, limit]);

  return recent;
}

function useBlockchainEvents(subscribe: (event: BlockchainEvent) => void) {
  const client = useBlockchainClient();
  const subscribeRef = useRef(subscribe);

  useEffect(() => {
    subscribeRef.current = subscribe;
  }, [subscribe]);

  useEffect(() => {
    const unsubscribe = client.subscribeToEvents((event) => {
      subscribeRef.current(event);
    });
    return () => {
      unsubscribe();
    };
  }, [client]);
}

export function useBlockchainEventStatus(): BlockchainEventStatus {
  const client = useBlockchainClient();
  const fallbackStatus = useMemo<BlockchainEventStatus>(
    () => ({
      transport: 'idle',
      websocketAvailable: client.metadata.supportsWebsocket,
      connectionState: client.metadata.connectionState,
    }),
    [client.metadata.supportsWebsocket, client.metadata.connectionState]
  );
  const subscribe = useCallback(
    (onChange: () => void) => {
      return client.subscribeToEventStatus?.(onChange) ?? (() => {});
    },
    [client]
  );
  const getSnapshot = useCallback(() => {
    return client.getEventStatus?.() ?? fallbackStatus;
  }, [client, fallbackStatus]);
  return useSyncExternalStore(subscribe, getSnapshot, getSnapshot);
}

// A query of the template at a height: [scope, 'template', { height }, ...].
function isTemplateQuery(key: readonly unknown[], scope: BlockchainQueryScope, height: number) {
  const [candidate, kind, target] = key;
  if (!isMatchingQueryScope(candidate, scope) || kind !== 'template') return false;
  if (!target || typeof target !== 'object') return false;
  return (target as { height?: unknown }).height === height;
}

export function BlockchainEventCacheUpdater() {
  const client = useBlockchainClient();
  const queryScope = useMemo(() => getBlockchainQueryScope(client), [client]);
  const queryClient = useQueryClient();
  const lastBlockHashByScope = useRef(new Map<string, string>());
  const blockRefreshByScope = useRef(
    new Map<string, { dirty: boolean; running: Promise<void> | null }>()
  );

  const scheduleBlockRefresh = useCallback(
    (scope: BlockchainQueryScope) => {
      const scopeKey = scope.endpoint;
      let refresh = blockRefreshByScope.current.get(scopeKey);
      if (!refresh) {
        refresh = { dirty: false, running: null };
        blockRefreshByScope.current.set(scopeKey, refresh);
      }

      refresh.dirty = true;
      if (refresh.running) return;

      const run = async () => {
        while (refresh.dirty) {
          refresh.dirty = false;
          await invalidateBlockDependentQueries(queryClient, scope);
        }
      };

      refresh.running = run().finally(() => {
        refresh.running = null;
        if (!refresh.dirty) {
          blockRefreshByScope.current.delete(scopeKey);
        }
      });
    },
    [queryClient]
  );

  const handleEvent = useCallback(
    (event: BlockchainEvent) => {
      if (event.type === 'template.new') {
        const { height } = event.payload;
        queryClient.setQueryData([queryScope, 'template', 'latest'], height);
        void queryClient.invalidateQueries({
          predicate: (query) => isTemplateQuery(query.queryKey, queryScope, height),
        });
        return;
      }

      if (event.contiguous) updateLatestBlocksCache(queryClient, queryScope, event.payload);
      else void reconcileLatestBlocks(queryClient, queryScope, client);

      const scopeKey = queryScope.endpoint;
      if (lastBlockHashByScope.current.get(scopeKey) === event.payload.hash) return;
      lastBlockHashByScope.current.set(scopeKey, event.payload.hash);
      scheduleBlockRefresh(queryScope);
    },
    [client, queryClient, queryScope, scheduleBlockRefresh]
  );
  useBlockchainEvents(handleEvent);

  return null;
}
