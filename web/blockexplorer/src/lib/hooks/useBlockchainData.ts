import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { type Query, type QueryClient, useQuery, useQueryClient } from '@tanstack/react-query';
import {
  AddressActivityItem,
  AddressDetails,
  AddressSummary,
  AddressUtxo,
  BlockchainClient,
  BlockchainClientConfiguration,
  BlockchainEventStatus,
  BlockDetail,
  BlockHeaderDetail,
  BlockValidation,
  BlockStats,
  BlockSummary,
  BlockchainEvent,
  TransactionSummary,
  TransactionOutputSpend,
  useBlockchainClient,
} from '@/lib/api/client';

type BlockchainQueryScope = {
  providerId: string;
  endpoint?: string;
};

const DEFAULT_LIVE_BLOCK_LIMIT = 10;
const RECENT_TRANSACTION_LIMIT = 20;

type BlockRefreshMode = 'always' | 'while-unconfirmed';

interface BlockchainQueryMeta extends Record<string, unknown> {
  blockRefresh?: BlockRefreshMode;
}

interface BlockchainQueryOptions<T> {
  queryKey: readonly unknown[];
  queryFn: (client: BlockchainClient) => Promise<T>;
  enabled?: boolean;
  retry?: boolean | number | ((failureCount: number, error: Error) => boolean);
  staleTime?: number;
  initialData?: T;
  blockRefresh?: BlockRefreshMode;
}

function getBlockchainQueryScope(client: BlockchainClient): BlockchainQueryScope {
  return client.metadata.endpoint
    ? { providerId: client.metadata.id, endpoint: client.metadata.endpoint }
    : { providerId: client.metadata.id };
}

function isMatchingQueryScope(value: unknown, scope: BlockchainQueryScope) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return false;
  const candidate = value as Partial<BlockchainQueryScope>;
  return candidate.providerId === scope.providerId && candidate.endpoint === scope.endpoint;
}

function getQueryScopeKey(scope: BlockchainQueryScope) {
  return JSON.stringify([scope.providerId, scope.endpoint ?? null]);
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

function updateRecentTransactionsCache(
  queryClient: QueryClient,
  queryScope: BlockchainQueryScope,
  tx: TransactionSummary
) {
  queryClient.setQueryData<TransactionSummary[]>([queryScope, 'transactions', 'recent'], (prev) => {
    const existing = prev ?? [];
    const next = [tx, ...existing.filter((item) => item.txid !== tx.txid)];
    return next.slice(0, RECENT_TRANSACTION_LIMIT);
  });
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

function applyBlockchainEventToCache(
  queryClient: QueryClient,
  queryScope: BlockchainQueryScope,
  event: BlockchainEvent
) {
  if (event.type === 'block.new') {
    updateLatestBlocksCache(queryClient, queryScope, event.payload);
  }
  if (event.type === 'tx.new') {
    updateRecentTransactionsCache(queryClient, queryScope, event.payload);
  }
}

function useBlockchainQuery<T>({
  queryKey,
  queryFn,
  enabled,
  retry,
  staleTime,
  initialData,
  blockRefresh,
}: BlockchainQueryOptions<T>) {
  const client = useBlockchainClient();

  // The provider client comes from context and includes methods; cache identity should track metadata.
  return useQuery<T>({
    queryKey: [getBlockchainQueryScope(client), ...queryKey],
    queryFn: () => queryFn(client),
    enabled,
    ...(retry === undefined ? {} : { retry }),
    staleTime,
    initialData,
    meta: blockRefresh ? ({ blockRefresh } satisfies BlockchainQueryMeta) : undefined,
  });
}

export function useLatestBlocks(limit = 10) {
  return useBlockchainQuery<BlockSummary[]>({
    queryKey: ['blocks', 'latest', { limit }],
    queryFn: (client) => client.getLatestBlocks(limit),
  });
}

export function useProviderConfiguration() {
  return useBlockchainQuery<BlockchainClientConfiguration>({
    queryKey: ['configuration'],
    queryFn: (client) => client.getConfiguration(),
    staleTime: 60_000,
  });
}

export function useBlock(hashOrHeight: string) {
  return useBlockchainQuery<BlockDetail>({
    queryKey: ['block', hashOrHeight],
    queryFn: (client) => client.getBlock(hashOrHeight),
  });
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

export function useAddress(address: string) {
  return useBlockchainQuery<AddressDetails>({
    queryKey: ['address', address],
    queryFn: (client) => client.getAddress(address),
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

export function useAddressTransactions(address: string, lastSeenTxid?: string) {
  return useBlockchainQuery<AddressActivityItem[]>({
    queryKey: ['address', address, 'transactions', { lastSeenTxid: lastSeenTxid ?? null }],
    queryFn: (client) => client.getAddressTransactions(address, lastSeenTxid),
    enabled: Boolean(address.trim()),
    blockRefresh: 'always',
  });
}

export function useBlockchainEvents(subscribe: (event: BlockchainEvent) => void) {
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
      transport: client.metadata.supportsWebsocket ? 'websocket' : 'polling',
      websocketAvailable: client.metadata.supportsWebsocket,
    }),
    [client.metadata.supportsWebsocket]
  );
  const [status, setStatus] = useState<BlockchainEventStatus>(() =>
    client.getEventStatus ? client.getEventStatus() : fallbackStatus
  );

  useEffect(() => {
    const initialStatus = client.getEventStatus ? client.getEventStatus() : fallbackStatus;
    setStatus(initialStatus);
    return client.subscribeToEventStatus?.(setStatus);
  }, [client, fallbackStatus]);

  return status;
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
      const scopeKey = getQueryScopeKey(scope);
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
      applyBlockchainEventToCache(queryClient, queryScope, event);
      if (event.type !== 'block.new') return;

      const scopeKey = getQueryScopeKey(queryScope);
      if (lastBlockHashByScope.current.get(scopeKey) === event.payload.hash) return;
      lastBlockHashByScope.current.set(scopeKey, event.payload.hash);
      scheduleBlockRefresh(queryScope);
    },
    [queryClient, queryScope, scheduleBlockRefresh]
  );
  useBlockchainEvents(handleEvent);

  return null;
}

export function useLiveLatestBlocks(limit = 10) {
  const { data, isLoading, isError, error } = useLatestBlocks(limit);

  const { data: recentTransactions = [] } = useBlockchainQuery<TransactionSummary[]>({
    queryKey: ['transactions', 'recent'],
    queryFn: () => Promise.resolve([]),
    enabled: false,
    initialData: [],
  });

  return { data, recentTransactions, isLoading, isError, error };
}
