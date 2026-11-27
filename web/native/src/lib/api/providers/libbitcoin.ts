import { z } from 'zod';
import { AddressQueriesDisabledError } from '@/lib/api/errors';
import {
  AddressActivityItem,
  AddressSummary,
  AddressUtxo,
  BlockchainClientConfiguration,
  BlockHeaderDetail,
  BlockValidation,
  BlockStats,
  BlockSummary,
  BlockchainClient,
  BlockchainEventHandler,
  BlockchainEventStatus,
  BlockchainEventStatusHandler,
  BlockchainEventTransport,
  BlockReference,
  BranchHeader,
  ChainBranch,
  HeaderTop,
  EstimateMode,
  PoolFees,
  RecentMiners,
  TemplateTransaction,
  TransactionInput,
  TransactionSizes,
  TransactionSummary,
  TransactionOutputSpend,
} from '../client';
import {
  BlockHeaderResponse,
  PrunedInputResponse,
  TxOutputResponse,
  TxPoint,
  TxResponse,
  scriptHashFromAddress,
  toBlockSummary,
  mapOutput,
  mapPrunedInput,
  mapTransaction,
  outputAddresses,
} from './libbitcoin-mapper';
import { identifyMiner, type Miner } from '@/lib/utils/miner';
import { isAddressPath } from '@/lib/utils/routes';

interface LibbitcoinClientOptions {
  baseUrl: string;
  pollIntervalMs?: number;
  turbo?: boolean;
}

const TRANSACTION_PAGE_SIZE = 25;
// Bounds parallel node requests for multi-item fetches (block lists, tx pages,
// prevout resolution) so bursts don't overwhelm the embedded node.
const REQUEST_CONCURRENCY = 8;
// Address queries are bounded by the server's history limit, not this deadline.
const HTTP_REQUEST_TIMEOUT_MS = 30_000;
const TOP_QUERY_PATH = '/v1/top?format=json';
const BRANCH_WALK_LIMIT = 64;
// Miners are cached by height only at this depth, below which reorganization is not expected.
const MINER_CACHE_DEPTH = 6;
const TOP_SUBSCRIBE_PATH = '/v1/top/subscribe?format=json';
const TOP_UNSUBSCRIBE_PATH = '/v1/top/subscribe?format=json&stop=true';
const TRANSACTION_SUBSCRIBE_PATH = '/v1/tx/subscribe?format=json';
const TRANSACTION_UNSUBSCRIBE_PATH = '/v1/tx/subscribe?format=json&stop=true';
const TEMPLATE_SUBSCRIBE_PATH = '/v1/template/subscribe?format=json';
const TEMPLATE_UNSUBSCRIBE_PATH = '/v1/template/subscribe?format=json&stop=true';
const WEBSOCKET_CONNECT_TIMEOUT_MS = 5_000;
const WEBSOCKET_HEARTBEAT_INTERVAL_MS = 15_000;
const WEBSOCKET_RESPONSE_TIMEOUT_MS = 45_000;
const EVENT_STREAM_STOP_DELAY_MS = 2_500;
// After a websocket failure the client falls back to polling and retries the
// websocket with exponential backoff between these bounds.
const WEBSOCKET_RETRY_MIN_DELAY_MS = 5_000;
const WEBSOCKET_RETRY_MAX_DELAY_MS = 300_000;

interface BlockHeaderContextResponse {
  hash: string;
  height: number;
  mtp: number;
  state: {
    size: number;
    weight: number;
    count: number;
    validated: boolean;
    confirmed: boolean;
    candidate: boolean;
    confirmable: boolean;
    unconfirmable: boolean;
  };
  forks: Record<string, boolean>;
}

const blockValidationSchema = z.object({
  hash: z.string().regex(/^[0-9a-f]{64}$/iu),
  mtp: z.number().int().nonnegative().max(0xffffffff).optional(),
  state: z
    .object({
      validated: z.boolean().optional(),
      confirmed: z.boolean().optional(),
      candidate: z.boolean().optional(),
      confirmable: z.boolean().optional(),
      unconfirmable: z.boolean().optional(),
    })
    .optional(),
  forks: z.record(z.string(), z.boolean()).optional(),
});

interface BlockDetailsResponse {
  hash: string;
  height: number;
  count: number;
  sigops?: number;
  segregated: boolean;
  nominal: number;
  maximal: number;
  weight: number;
  virtual: number;
  value: number;
  fees: number;
  subsidy: number;
  reward: number;
  claim: number;
}

interface TxDetailsConfirmedResponse {
  height: number;
  position: number;
  sigops?: number;
}

interface TxDetailsResponse {
  wtxid?: string;
  segregated: boolean;
  coinbase: boolean;
  nominal: number;
  maximal: number;
  weight: number;
  virtual: number;
  value?: number;
  spend?: number;
  fee?: number;
  /** When the tx was pooled, in seconds, if unconfirmed. */
  time?: number;
  confirmed?: TxDetailsConfirmedResponse;
  confirmable?: boolean;
}

const TOP_CACHE_TTL_MS = 5_000;
const topCache = new Map<
  string,
  {
    promise: Promise<number>;
    value: number | null;
    expiresAt: number;
  }
>();

const inFlightRequests = new Map<string, Promise<unknown>>();

class LibbitcoinApiError extends Error {
  constructor(
    message: string,
    readonly status: number
  ) {
    super(message);
    this.name = 'LibbitcoinApiError';
  }
}

const feesSchema = z.object({
  count: z.number(),
  virtual: z.number(),
  fee: z.number(),
  histogram: z.array(z.tuple([z.number(), z.number()])),
  estimates: z
    .object({ mode: z.enum(['basic', 'geometric', 'economical', 'conservative']) })
    .catchall(z.number().nullable())
    .nullable()
    .optional()
    .transform((value) => value ?? null),
});

const templateSchema = z.array(
  z.object({
    segregated: z.boolean(),
    nominal: z.number(),
    maximal: z.number(),
    weight: z.number(),
    virtual: z.number(),
    value: z.number(),
    spend: z.number(),
    fee: z.number(),
    cluster: z.number(),
  })
);

const hashesSchema = z.array(z.string());

const headerTopsSchema = z.array(
  z.object({
    hash: z.string(),
    height: z.number().int().nonnegative(),
    header: z.object({ previous: z.string(), timestamp: z.number() }),
    chain: z.enum(['confirmed', 'candidate']).optional(),
  })
);

const configurationSchema = z.object({
  store: z.object({
    address: z.boolean(),
    filter: z.boolean(),
    turbo: z.boolean(),
    silent: z.number().int().nonnegative().nullable(),
    pruned: z.number().int().nonnegative().nullable().optional(),
  }),
  node: z.object({ network: z.string().trim().min(1) }).catchall(z.unknown()),
  network: z.record(z.string(), z.unknown()),
  wallet: z.object({
    p2kh: z.number().int().min(0).max(255),
    p2sh: z.number().int().min(0).max(255),
    witness: z.string().trim().min(1),
  }),
  forks: z.record(z.string(), z.boolean()),
});

function normalizeConfiguration(raw: unknown): BlockchainClientConfiguration {
  const parsed = configurationSchema.safeParse(raw);
  if (!parsed.success) {
    throw new Error('Invalid server configuration');
  }
  const { store, node, wallet, forks, network } = parsed.data;
  const flags = (source: Record<string, unknown>) =>
    Object.fromEntries(
      Object.entries(source).filter(
        (entry): entry is [string, boolean] => typeof entry[1] === 'boolean'
      )
    );
  const enabledForks = Object.entries(forks)
    .filter(([, enabled]) => enabled)
    .map(([fork]) => fork);
  return {
    address: store.address,
    filter: store.filter,
    turbo: store.turbo,
    silent: store.silent !== null,
    services: flags(node),
    protocol: flags(network),
    coin: enabledForks.includes('ltc_scrypt_proof_of_work') ? 'litecoin' : 'bitcoin',
    network: node.network,
    forks: enabledForks,
    wallet: { p2pkh: wallet.p2kh, p2sh: wallet.p2sh, hrp: wallet.witness },
    ...(typeof store.pruned === 'number' ? { pruned: store.pruned } : {}),
  };
}

type BlockHeaderContextState = Partial<
  Pick<BlockHeaderContextResponse['state'], 'count' | 'size' | 'weight'>
>;

function extractHeaderContextState(context: unknown): BlockHeaderContextState | null {
  if (!context || typeof context !== 'object' || Array.isArray(context)) return null;

  const record = context as Record<string, unknown>;
  const stateValue = record.state;
  if (!stateValue || typeof stateValue !== 'object' || Array.isArray(stateValue)) return null;

  const stateRecord = stateValue as Record<string, unknown>;
  const count = typeof stateRecord.count === 'number' ? stateRecord.count : undefined;
  const size = typeof stateRecord.size === 'number' ? stateRecord.size : undefined;
  const weight = typeof stateRecord.weight === 'number' ? stateRecord.weight : undefined;
  if (count === undefined && size === undefined && weight === undefined) return null;

  return { count, size, weight };
}

function getHeaderContextMaximalSize(state: BlockHeaderContextState | null) {
  // In header context, size is nominal bytes and weight is (3 * nominal + maximal).
  if (state?.size === undefined || state.weight === undefined) return;
  return state.weight - 3 * state.size;
}

function computeFeeRate(feeSats: number, weight: number) {
  if (!Number.isFinite(feeSats) || !Number.isFinite(weight) || weight <= 0) return null;
  return (feeSats / weight) * 4;
}

function applyTxDetails(
  summary: TransactionSummary,
  details: TxDetailsResponse | null
): TransactionSummary {
  if (!details) return summary;

  const feeSats = typeof details.fee === 'number' ? details.fee : undefined;
  const weight = typeof details.weight === 'number' ? details.weight : undefined;
  const feeRate =
    typeof feeSats === 'number' && typeof weight === 'number'
      ? computeFeeRate(feeSats, weight)
      : summary.feeRate;

  return {
    ...summary,
    wtxid: details.wtxid || summary.wtxid,
    confirmable: details.confirmable === true,
    coinbase: details.coinbase === true,
    feeSats: typeof details.fee === 'number' ? details.fee : summary.feeSats,
    feeRate,
    size: typeof details.maximal === 'number' ? details.maximal : summary.size,
    weight: typeof details.weight === 'number' ? details.weight : summary.weight,
    sigops:
      typeof details.confirmed?.sigops === 'number' ? details.confirmed.sigops : summary.sigops,
    totalInput: typeof details.value === 'number' ? details.value : summary.totalInput,
    totalOutput: typeof details.spend === 'number' ? details.spend : summary.totalOutput,
  };
}

async function fetchJson<T>(baseUrl: string, path: string, turbo?: boolean): Promise<T> {
  const normalized = path.startsWith('/') ? path : `/${path}`;
  let url = normalized.includes('?')
    ? `${baseUrl}${normalized}`
    : `${baseUrl}${normalized}?format=json`;
  if (turbo) {
    url += url.includes('?') ? '&turbo=true' : '?turbo=true';
  }

  const existing = inFlightRequests.get(url);
  if (existing) {
    return existing as Promise<T>;
  }

  const controller = new AbortController();
  let timeout: ReturnType<typeof setTimeout> | undefined;
  const deadline = isAddressPath(normalized)
    ? undefined
    : new Promise<never>((_resolve, reject) => {
        timeout = setTimeout(() => {
          const seconds = String(HTTP_REQUEST_TIMEOUT_MS / 1_000);
          const error = new Error(`Request timed out after ${seconds} seconds`);
          reject(error);
          controller.abort(error);
        }, HTTP_REQUEST_TIMEOUT_MS);
      });
  const request = (async () => {
    const response = await fetch(url, {
      signal: controller.signal,
      headers: {
        Accept: 'application/json',
      },
    });
    if (!response.ok) {
      const text = await response.text().catch(() => '');
      throw new LibbitcoinApiError(
        `Request failed ${String(response.status)}: ${text || response.statusText}`,
        response.status
      );
    }
    const text = await response.text();
    // Script hex can also be a JSON number (e.g. "51"). Preserve text endpoint bytes.
    if (/[?&]format=text(?:&|$)/u.test(normalized)) return text as T;
    try {
      return JSON.parse(text) as T;
    } catch {
      // Some endpoints return primitives (e.g. /v1/top).
      return text as unknown as T;
    }
  })();
  // Bound the whole response, including its body, and release shared requests
  // on timeout even if the transport is slow to react to cancellation.
  const promise = deadline ? Promise.race([request, deadline]) : request;

  inFlightRequests.set(url, promise);

  try {
    return await promise;
  } finally {
    clearTimeout(timeout);
    inFlightRequests.delete(url);
  }
}

async function tryFetchJson<T>(baseUrl: string, path: string, turbo?: boolean): Promise<T | null> {
  try {
    return await fetchJson<T>(baseUrl, path, turbo);
  } catch {
    return null;
  }
}

async function fetchBytes(baseUrl: string, path: string): Promise<Uint8Array> {
  const response = await fetch(`${baseUrl}${path}`, {
    headers: { Accept: 'application/octet-stream' },
  });
  if (!response.ok) {
    const text = await response.text().catch(() => '');
    throw new LibbitcoinApiError(
      `Request failed ${String(response.status)}: ${text || response.statusText}`,
      response.status
    );
  }
  return new Uint8Array(await response.arrayBuffer());
}

const HASH_SIZE = 32;
const HEX = Array.from({ length: 256 }, (_, byte) => byte.toString(16).padStart(2, '0'));

// Hashes are served in their byte order and displayed reversed.
function toHashes(bytes: Uint8Array): string[] {
  const hashes: string[] = [];
  for (let start = 0; start + HASH_SIZE <= bytes.length; start += HASH_SIZE) {
    let hash = '';
    for (let index = start + HASH_SIZE - 1; index >= start; index--) hash += HEX[bytes[index]];
    hashes.push(hash);
  }
  return hashes;
}

const DETAIL_SIZE = 36;
const TIME_SIZE = 4;
const WORD = 2 ** 32;

// The details of each tx as a fixed width record: nominal and maximal sizes
// (4 bytes each), value, spend and fee (8 each) and cluster (4), little endian.
// Segregation, weight and virtual size follow from the sizes. Where timed, a
// time (4 bytes) ends each record.
function toDetails(bytes: Uint8Array, timed = false): TemplateTransaction[] {
  const size = timed ? DETAIL_SIZE + TIME_SIZE : DETAIL_SIZE;
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const wide = (offset: number) =>
    view.getUint32(offset, true) + view.getUint32(offset + 4, true) * WORD;
  const details: TemplateTransaction[] = [];
  for (let start = 0; start + size <= bytes.length; start += size) {
    const nominal = view.getUint32(start, true);
    const maximal = view.getUint32(start + 4, true);
    const weight = 3 * nominal + maximal;
    details.push({
      segregated: maximal !== nominal,
      nominal,
      maximal,
      weight,
      virtual: Math.ceil(weight / 4),
      value: wide(start + 8),
      spend: wide(start + 16),
      fee: wide(start + 24),
      cluster: view.getUint32(start + 32, true),
      ...(timed ? { time: view.getUint32(start + DETAIL_SIZE, true) } : {}),
    });
  }
  return details;
}

async function fetchJsonOrNullOnNotFound<T>(
  baseUrl: string,
  path: string,
  turbo?: boolean
): Promise<T | null> {
  try {
    return await fetchJson<T>(baseUrl, path, turbo);
  } catch (error) {
    if (error instanceof LibbitcoinApiError && error.status === 404) {
      return null;
    }
    throw error;
  }
}

async function fetchTop(baseUrl: string, turbo?: boolean) {
  const raw = await fetchJson<unknown>(baseUrl, '/v1/top?format=json', turbo);
  const top = parseConfirmedTop(raw);
  if (!top) throw new Error('Invalid top');
  return top;
}

async function fetchTopHeight(baseUrl: string, turbo?: boolean) {
  return (await fetchTop(baseUrl, turbo)).height;
}

function isHexHash(value: string) {
  return /^[0-9a-f]{64}$/iu.test(value);
}

interface ChainTop {
  height: number;
  hash: string;
}

// Top responses and notifications are the strong tops, the confirmed first.
function parseConfirmedTop(value: unknown): ChainTop | null {
  const top: unknown = Array.isArray(value) ? value[0] : undefined;
  if (!isRecord(top)) return null;
  const { height, hash } = top;
  if (typeof height !== 'number' || !Number.isInteger(height) || height < 0) return null;
  if (typeof hash !== 'string' || !isHexHash(hash)) return null;
  return { height, hash };
}

function normalizeTextMessage(data: MessageEvent['data']): Promise<string | null> {
  if (typeof data === 'string') return Promise.resolve(data);
  if (data instanceof ArrayBuffer) {
    return Promise.resolve(new TextDecoder().decode(data));
  }
  if (data instanceof Blob) {
    return data.text();
  }
  return Promise.resolve(null);
}

// A template notification is the bare height of the replaced template.
function parseTemplateSubscriptionMessage(text: string): number | null {
  const trimmed = text.trim();
  if (!/^\d+$/u.test(trimmed)) return null;
  return Number(trimmed);
}

// A tx notification is the id of the pooled tx, as a json string.
function parseTransactionSubscriptionMessage(text: string): string | null {
  const match = /^"([0-9a-f]{64})"$/u.exec(text.trim());
  return match ? match[1] : null;
}

function parseTopSubscriptionMessage(text: string): string | null {
  const trimmed = text.trim();
  if (!trimmed) return null;

  try {
    return parseConfirmedTop(JSON.parse(trimmed))?.hash ?? null;
  } catch {
    return null;
  }
}

function resolveWebSocketUrl(baseUrl: string): string | null {
  const trimmed = baseUrl.trim();
  if (!trimmed) return null;

  const hasScheme = /^[a-z][a-z\d+\-.]*:\/\//iu.test(trimmed);
  const normalized = hasScheme ? trimmed : `http://${trimmed}`;
  const window_ = (globalThis as unknown as { window?: Window }).window;

  try {
    const url = new URL(normalized, window_?.location.href ?? 'http://localhost');
    if (url.protocol === 'http:') {
      url.protocol = 'ws:';
    } else if (url.protocol === 'https:') {
      url.protocol = 'wss:';
    } else if (url.protocol !== 'ws:' && url.protocol !== 'wss:') {
      return null;
    }
    url.hash = '';
    url.search = '';
    return url.toString();
  } catch {
    return null;
  }
}

async function fetchTopHeightCached(baseUrl: string, turbo?: boolean) {
  const now = Date.now();
  const key = `${baseUrl}|${turbo ? '1' : '0'}`;
  const cached = topCache.get(key);
  if (cached && cached.expiresAt > now) {
    return cached.promise;
  }
  const promise = fetchTopHeight(baseUrl, turbo).then(
    (value) => {
      topCache.set(key, {
        promise,
        value,
        expiresAt: Date.now() + TOP_CACHE_TTL_MS,
      });
      return value;
    },
    (error: unknown) => {
      // Evict the failed entry (unless a newer attempt already replaced it) so
      // the next caller retries the network instead of replaying this rejection
      // for the rest of the TTL.
      if (topCache.get(key)?.promise === promise) {
        topCache.delete(key);
      }
      throw error;
    }
  );
  topCache.set(key, {
    promise,
    value: cached?.value ?? null,
    expiresAt: now + TOP_CACHE_TTL_MS,
  });
  return promise;
}

async function resolveBlockPath(baseUrl: string, hashOrHeight: string, turbo?: boolean) {
  if (hashOrHeight === 'latest' || hashOrHeight === 'tip') {
    const tip = await fetchTopHeightCached(baseUrl, turbo);
    return { path: `/v1/block/confirmed/${String(tip)}`, height: tip };
  }
  if (/^\d+$/u.test(hashOrHeight)) {
    return { path: `/v1/block/confirmed/${hashOrHeight}` };
  }
  return { path: `/v1/block/hash/${hashOrHeight}` };
}

function createPrevoutResolver(baseUrl: string, turbo?: boolean) {
  const cache = new Map<string, Promise<TxOutputResponse | null>>();
  return (hash: string, index: number) => {
    const key = `${hash}:${String(index)}`;
    const cached = cache.get(key);
    if (cached) return cached;
    const promise = fetchJson<TxOutputResponse>(
      baseUrl,
      `/v1/output/${hash}/${String(index)}?format=json`,
      turbo
    ).catch(() => null);
    cache.set(key, promise);
    return promise;
  };
}

async function fetchBlockTx(baseUrl: string, path: string, index: number, turbo?: boolean) {
  return fetchJson<TxResponse>(baseUrl, `${path}/tx/${String(index)}?format=json`, turbo);
}

function feeRate(details: TxDetailsResponse): number | null {
  if (typeof details.fee !== 'number' || details.virtual === 0) return null;
  return details.fee / details.virtual;
}

interface PrunedBlockTransactions {
  hashes: string[];
  details: TemplateTransaction[];
}

// A pruned tx is not provided whole, so its row is of its block's txs and their
// details, all but its input scripts and witnesses being provided.
function toPrunedSummary(
  hash: string,
  detail: TemplateTransaction,
  block: { height: number; hash: string; tip: number }
): TransactionSummary {
  return {
    txid: hash,
    hash,
    status: 'confirmed',
    confirmations: block.tip - block.height + 1,
    blockHeight: block.height,
    blockHash: block.hash,
    feeSats: detail.fee,
    feeRate: detail.virtual === 0 ? null : detail.fee / detail.virtual,
    size: detail.maximal,
    weight: detail.weight,
    totalInput: detail.value,
    totalOutput: detail.spend,
    inputs: [],
    outputs: [],
  };
}

async function fetchBlockTransactionPositions(
  baseUrl: string,
  path: string,
  positions: number[],
  allowTrailingNotFound: boolean
): Promise<TxResponse[]> {
  const responses = await mapWithConcurrency(positions, REQUEST_CONCURRENCY, async (position) => {
    try {
      return await fetchBlockTx(baseUrl, path, position);
    } catch (error) {
      if (allowTrailingNotFound && error instanceof LibbitcoinApiError && error.status === 404) {
        return null;
      }
      throw error;
    }
  });

  const firstMissingIndex = responses.indexOf(null);
  if (firstMissingIndex === -1) {
    return responses.filter((response): response is TxResponse => response !== null);
  }

  if (responses.slice(firstMissingIndex + 1).some((response) => response !== null)) {
    throw new Error('Libbitcoin returned a non-contiguous block transaction page');
  }

  return responses
    .slice(0, firstMissingIndex)
    .filter((response): response is TxResponse => response !== null);
}

// Fetches header + context for each height with bounded parallelism and maps
// them to block summaries, preserving the input height order.
async function fetchBlockSummaries(baseUrl: string, heights: number[]): Promise<BlockSummary[]> {
  return mapWithConcurrency(heights, REQUEST_CONCURRENCY, async (h) => {
    const [header, context] = await Promise.all([
      fetchJson<BlockHeaderResponse>(
        baseUrl,
        `/v1/block/confirmed/${String(h)}/header?format=json`
      ),
      tryFetchJson<unknown>(baseUrl, `/v1/block/confirmed/${String(h)}/header/context?format=json`),
    ]);
    const state = extractHeaderContextState(context);
    return {
      ...toBlockSummary(header, state?.count),
      size: getHeaderContextMaximalSize(state),
    };
  });
}

type AddressOutpoint = { point: TxPoint; value: number };

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function isTxPoint(value: unknown): value is TxPoint {
  if (!isRecord(value)) return false;
  return (
    typeof value.hash === 'string' &&
    value.hash.length > 0 &&
    typeof value.index === 'number' &&
    Number.isInteger(value.index) &&
    value.index >= 0
  );
}

function parseAddressOutpoints(value: unknown, endpoint: string): AddressOutpoint[] {
  if (
    !Array.isArray(value) ||
    !value.every(
      (entry): entry is AddressOutpoint =>
        isRecord(entry) &&
        isTxPoint(entry.point) &&
        typeof entry.value === 'number' &&
        Number.isFinite(entry.value) &&
        entry.value >= 0
    )
  ) {
    throw new TypeError(`Invalid response from ${endpoint}`);
  }
  return value;
}

// Confirmed UTXOs only (current holdings). The node returns 404 when the script
// hash has no confirmed UTXOs (e.g. a fully-spent address), which maps to an
// empty list. Other failures must propagate so an outage is never shown as a
// zero balance. We must NOT fall back to `/v1/address/{hash}` here: that endpoint
// returns every historical output (spent + unspent), so a spent address would
// report its spent outputs as if they were unspent — inflating utxoCount and the
// balance fallback.
async function fetchConfirmedUtxos(
  baseUrl: string,
  hash: string,
  turbo?: boolean
): Promise<AddressOutpoint[]> {
  const endpoint = `/v1/address/${hash}/confirmed?format=json`;
  const confirmed = await fetchJsonOrNullOnNotFound<unknown>(baseUrl, endpoint, turbo);
  return confirmed === null ? [] : parseAddressOutpoints(confirmed, endpoint);
}

// All received outputs for the script hash (spent + unspent) — the address's
// full receiving history. A 404 means an unused script hash; all other request
// and validation failures propagate.
async function fetchAddressOutputs(
  baseUrl: string,
  hash: string,
  turbo?: boolean
): Promise<AddressOutpoint[]> {
  const endpoint = `/v1/address/${hash}?format=json`;
  const outputs = await fetchJsonOrNullOnNotFound<unknown>(baseUrl, endpoint, turbo);
  return outputs === null ? [] : parseAddressOutpoints(outputs, endpoint);
}

// The activity list is paginated by received output: each page resolves spenders
// for only the outputs on that page, so the per-output `/spender` lookup cost is
// bounded by the page size rather than the address's total history.
const ADDRESS_PAGE_SIZE = 25;
const SPENDER_LOOKUP_CONCURRENCY = 8;
// Full received-output list is cached briefly so paging through it does not
// re-fetch `/v1/address/{hash}` (up to ~MBs for busy scripts) on every page.
const ADDRESS_OUTPUTS_TTL_MS = 30_000;

function parseOffsetCursor(cursor?: string): number {
  if (cursor === undefined) return 0;
  const offset = Number(cursor);
  return Number.isInteger(offset) && offset > 0 ? offset : 0;
}

// Returns the spending outpoint for an output, or null when unspent (404).
// Not an address endpoint, so the turbo flag does not apply.
async function fetchOutputSpender(
  baseUrl: string,
  txid: string,
  index: number
): Promise<TxPoint | null> {
  const endpoint = `/v1/output/${txid}/${String(index)}/spender?format=json`;
  let spender: unknown;
  try {
    spender = await fetchJson<unknown>(baseUrl, endpoint);
  } catch (error) {
    if (error instanceof LibbitcoinApiError && error.status === 404) return null;
    throw error;
  }
  if (!isTxPoint(spender)) {
    throw new TypeError(`Invalid response from ${endpoint}`);
  }
  return spender;
}

// Runs `fn` over `items` with a bounded number of concurrent promises,
// preserving input order in the result. No item starts after a failure or
// after `signal` aborts.
async function mapWithConcurrency<T, R>(
  items: T[],
  limit: number,
  fn: (item: T, index: number) => Promise<R>,
  signal?: AbortSignal
): Promise<R[]> {
  const results = Array.from<R>({ length: items.length });
  let next = 0;
  let failed = false;
  async function worker() {
    while (next < items.length && !failed) {
      signal?.throwIfAborted();
      const current = next;
      next += 1;
      try {
        results[current] = await fn(items[current], current);
      } catch (error) {
        failed = true;
        throw error;
      }
    }
  }
  const workerCount = Math.min(limit, items.length);
  await Promise.all(Array.from({ length: workerCount }, () => worker()));
  return results;
}

// Builds a received/spent ledger for one page of received outputs: each output
// becomes an incoming (+value) row, and every spent output additionally yields an
// outgoing (-value) row linking the spending tx, placed right after its receive.
// `nextCursor` (when set) is attached to every row so the UI can request the next
// page after the last accumulated row.
async function buildAddressLedgerPage(
  baseUrl: string,
  pageOutputs: AddressOutpoint[],
  nextCursor: string | null
): Promise<AddressActivityItem[]> {
  const spenders = await mapWithConcurrency(pageOutputs, SPENDER_LOOKUP_CONCURRENCY, (output) =>
    fetchOutputSpender(baseUrl, output.point.hash, output.point.index)
  );

  const items: AddressActivityItem[] = [];
  for (const [i, output] of pageOutputs.entries()) {
    const { hash, index } = output.point;
    const spender = spenders[i];
    items.push({
      id: `in:${hash}:${String(index)}`,
      txid: hash,
      outputIndex: index,
      valueChange: output.value,
      direction: 'received',
      spent: spender !== null,
      ...(nextCursor === null ? {} : { cursor: nextCursor }),
    });
    if (spender) {
      items.push({
        id: `out:${hash}:${String(index)}`,
        txid: spender.hash,
        valueChange: -output.value,
        direction: 'sent',
        ...(nextCursor === null ? {} : { cursor: nextCursor }),
      });
    }
  }
  return items;
}

function resolveAddressBalance(
  balanceRaw: unknown,
  utxos: Array<{ point: TxPoint; value: number }>
) {
  if (typeof balanceRaw === 'number' && Number.isFinite(balanceRaw)) {
    return balanceRaw;
  }
  if (typeof balanceRaw === 'string' && balanceRaw.trim()) {
    const parsed = Number(balanceRaw);
    if (Number.isFinite(parsed)) return parsed;
  }

  let computedBalance = 0;
  for (const utxo of utxos) {
    computedBalance += utxo.value;
  }
  return computedBalance;
}

export function createLibbitcoinClient({
  baseUrl,
  pollIntervalMs = 15000,
  turbo: turboAddress = false,
}: LibbitcoinClientOptions): BlockchainClient {
  const listeners = new Set<BlockchainEventHandler>();
  const transactionListeners = new Set<(txid: string) => void>();
  let templateWatchers = 0;
  const statusListeners = new Set<BlockchainEventStatusHandler>();
  let poller: ReturnType<typeof setInterval> | null = null;
  let socket: WebSocket | null = null;
  let socketConnectTimer: ReturnType<typeof setTimeout> | null = null;
  let socketHeartbeatTimer: ReturnType<typeof setInterval> | null = null;
  let stopEventStreamTimer: ReturnType<typeof setTimeout> | null = null;
  let websocketRetryTimer: ReturnType<typeof setTimeout> | null = null;
  let websocketRetryDelayMs = WEBSOCKET_RETRY_MIN_DELAY_MS;
  let websocketUnavailable = false;
  let closingSocket = false;
  let eventTransport: BlockchainEventTransport = 'idle';
  let lastHeight: number | null = null;
  let lastBlockHash: string | null = null;
  let configuration: BlockchainClientConfiguration | null = null;
  let configurationPromise: Promise<BlockchainClientConfiguration> | null = null;
  const websocketUrl = resolveWebSocketUrl(baseUrl);
  const supportsWebsocket = Boolean(websocketUrl && globalThis.WebSocket);
  let httpAvailable: boolean | null = null;
  let eventStatus: BlockchainEventStatus = {
    transport: 'idle',
    websocketAvailable: supportsWebsocket,
    connectionState: 'unknown',
  };
  const minersByHeight = new Map<number, Miner | null>();
  const addressOutputsCache = new Map<string, { outputs: AddressOutpoint[]; expiresAt: number }>();
  const confirmedUtxosCache = new Map<
    string,
    { request: Promise<AddressOutpoint[]>; expiresAt: number }
  >();

  // Fetches the full received-output list for a script hash, caching it briefly so
  // paging does not re-download it on every page request.
  async function getAddressOutputsCached(hash: string, turbo?: boolean) {
    const now = Date.now();
    const cached = addressOutputsCache.get(hash);
    if (cached && cached.expiresAt > now) return cached.outputs;
    const outputs = await fetchAddressOutputs(baseUrl, hash, turbo);
    addressOutputsCache.set(hash, { outputs, expiresAt: now + ADDRESS_OUTPUTS_TTL_MS });
    return outputs;
  }

  async function getConfirmedUtxosCached(hash: string, turbo?: boolean) {
    const now = Date.now();
    const cached = confirmedUtxosCache.get(hash);
    if (cached && cached.expiresAt > now) return cached.request;

    const request = fetchConfirmedUtxos(baseUrl, hash, turbo);
    confirmedUtxosCache.set(hash, {
      request,
      expiresAt: now + ADDRESS_OUTPUTS_TTL_MS,
    });
    try {
      return await request;
    } catch (error) {
      if (confirmedUtxosCache.get(hash)?.request === request) {
        confirmedUtxosCache.delete(hash);
      }
      throw error;
    }
  }

  // Resolves one ADDRESS_PAGE_SIZE-sized page of the received/spent ledger,
  // starting at the offset encoded in `cursor` (undefined = first page).
  async function buildAddressActivityPage(hash: string, turbo: boolean, cursor?: string) {
    const outputs = await getAddressOutputsCached(hash, turbo);
    const offset = parseOffsetCursor(cursor);
    const pageOutputs = outputs.slice(offset, offset + ADDRESS_PAGE_SIZE);
    const nextOffset = offset + ADDRESS_PAGE_SIZE;
    const nextCursor = nextOffset < outputs.length ? String(nextOffset) : null;
    return buildAddressLedgerPage(baseUrl, pageOutputs, nextCursor);
  }

  function ensureConfiguration(): Promise<BlockchainClientConfiguration> {
    if (configurationPromise) return configurationPromise;
    configurationPromise = observeHttp(fetchJson<unknown>(baseUrl, '/v1/configuration?format=json'))
      .then((raw) => {
        const cfg = normalizeConfiguration(raw);
        configuration = cfg;
        return cfg;
      })
      .catch((error: unknown) => {
        configurationPromise = null;
        throw error;
      });
    return configurationPromise;
  }

  async function fetchMiner(path: string): Promise<Miner | null> {
    const params = await resolveNetworkParams();
    const [coinbase, script] = await Promise.all([
      fetchJson<TxResponse>(baseUrl, `${path}/tx/0?format=json`),
      fetchJson<string>(baseUrl, `${path}/tx/0/input/0/script?format=text`),
    ]);
    return identifyMiner(script.trim(), outputAddresses(coinbase, params)) ?? null;
  }

  async function resolveNetworkParams() {
    const config = configuration ?? (await ensureConfiguration());
    return config.wallet;
  }

  function isAddressEnabled(cfg: BlockchainClientConfiguration) {
    return cfg.address;
  }

  function isTurboEnabled(cfg: BlockchainClientConfiguration) {
    return cfg.turbo;
  }

  function getEventStatus(): BlockchainEventStatus {
    return eventStatus;
  }

  function getConnectionState(): BlockchainEventStatus['connectionState'] {
    if (eventTransport === 'websocket') return httpAvailable === false ? 'degraded' : 'connected';
    if (httpAvailable === false) return 'offline';
    if (httpAvailable === null) return 'unknown';
    return supportsWebsocket && poller !== null ? 'degraded' : 'connected';
  }

  function publishEventStatus() {
    const next: BlockchainEventStatus = {
      transport: eventTransport,
      websocketAvailable: supportsWebsocket && !websocketUnavailable,
      connectionState: getConnectionState(),
    };
    if (
      next.transport === eventStatus.transport &&
      next.websocketAvailable === eventStatus.websocketAvailable &&
      next.connectionState === eventStatus.connectionState
    )
      return;
    eventStatus = next;
    for (const listener of statusListeners) listener(eventStatus);
  }

  async function observeHttp<T>(request: Promise<T>): Promise<T> {
    try {
      const value = await request;
      httpAvailable = true;
      publishEventStatus();
      return value;
    } catch (error) {
      // A missing block or unsupported request still proves HTTP is reachable.
      httpAvailable = error instanceof LibbitcoinApiError && error.status < 500;
      publishEventStatus();
      throw error;
    }
  }

  function setEventTransport(transport: BlockchainEventTransport) {
    eventTransport = transport;
    publishEventStatus();
  }

  async function emitBlockEvent(identifier: string | number) {
    const path =
      typeof identifier === 'number'
        ? `/v1/block/confirmed/${String(identifier)}`
        : `/v1/block/hash/${identifier}`;
    const [header, context] = await Promise.all([
      observeHttp(fetchJson<BlockHeaderResponse>(baseUrl, `${path}/header?format=json`)),
      tryFetchJson<unknown>(baseUrl, `${path}/header/context?format=json`),
    ]);

    if (lastBlockHash === header.hash) return;

    const contiguous = lastBlockHash !== null && header.previous === lastBlockHash;
    const state = extractHeaderContextState(context);
    const summary = toBlockSummary(header, state?.count);
    const enriched: BlockSummary = {
      ...summary,
      size: getHeaderContextMaximalSize(state),
    };
    lastHeight = header.height;
    lastBlockHash = header.hash;

    for (const listener of listeners) {
      listener({ type: 'block.new', payload: enriched, contiguous });
    }
  }

  async function checkForNewTop() {
    try {
      const top = await observeHttp(fetchTop(baseUrl));
      if (lastBlockHash === null) {
        lastHeight = top.height;
        lastBlockHash = top.hash;
        return;
      }
      if (top.hash !== lastBlockHash) {
        await emitBlockEvent(top.hash);
      }
    } catch (error) {
      console.warn('Libbitcoin poller failed', error);
    }
  }

  function startPolling() {
    if (listeners.size === 0) return;
    if (!poller) {
      poller = setInterval(() => {
        void checkForNewTop();
      }, pollIntervalMs);
      void checkForNewTop();
    }
    setEventTransport('polling');
  }

  function stopPolling() {
    if (poller) {
      clearInterval(poller);
      poller = null;
    }
  }

  function clearSocketConnectTimer() {
    if (socketConnectTimer) {
      clearTimeout(socketConnectTimer);
      socketConnectTimer = null;
    }
  }

  function clearSocketHeartbeatTimer() {
    if (socketHeartbeatTimer !== null) {
      clearInterval(socketHeartbeatTimer);
      socketHeartbeatTimer = null;
    }
  }

  function clearStopEventStreamTimer() {
    if (stopEventStreamTimer) {
      clearTimeout(stopEventStreamTimer);
      stopEventStreamTimer = null;
    }
  }

  function clearWebSocketRetryTimer() {
    if (websocketRetryTimer) {
      clearTimeout(websocketRetryTimer);
      websocketRetryTimer = null;
    }
  }

  function scheduleWebSocketRetry() {
    if (websocketRetryTimer || !supportsWebsocket || listeners.size === 0) return;
    const delay = websocketRetryDelayMs;
    websocketRetryDelayMs = Math.min(websocketRetryDelayMs * 2, WEBSOCKET_RETRY_MAX_DELAY_MS);
    websocketRetryTimer = setTimeout(() => {
      websocketRetryTimer = null;
      if (socket || listeners.size === 0) return;
      websocketUnavailable = false;
      startWebSocket();
    }, delay);
  }

  // Falls back to polling and schedules a websocket reconnect with backoff, so a
  // transient drop does not permanently downgrade the session to polling.
  function handleWebSocketFailure() {
    websocketUnavailable = true;
    closeSocket(false);
    startPolling();
    scheduleWebSocketRetry();
  }

  function closeSocket(sendStop: boolean) {
    clearSocketConnectTimer();
    clearSocketHeartbeatTimer();
    if (!socket) return;
    closingSocket = true;
    try {
      if (sendStop && socket.readyState === 1) {
        socket.send(TOP_UNSUBSCRIBE_PATH);
        if (templateWatchers > 0) socket.send(TEMPLATE_UNSUBSCRIBE_PATH);
        if (transactionListeners.size > 0) socket.send(TRANSACTION_UNSUBSCRIBE_PATH);
      }
      socket.close();
    } catch {
      // The socket is already closing or unavailable.
    } finally {
      socket = null;
    }
  }

  function startWebSocket(): boolean {
    clearStopEventStreamTimer();
    if (socket) {
      return true;
    }

    if (!supportsWebsocket || !websocketUrl || websocketUnavailable || listeners.size === 0) {
      return false;
    }

    try {
      closingSocket = false;
      socket = new WebSocket(websocketUrl);
      setEventTransport('connecting');
    } catch (error) {
      console.warn('Libbitcoin websocket unavailable, falling back to polling', error);
      handleWebSocketFailure();
      return false;
    }

    const activeSocket = socket;
    let probeSentAt: number | null = null;
    socketConnectTimer = setTimeout(() => {
      if (socket !== activeSocket) return;
      console.warn('Libbitcoin websocket connect timed out, falling back to polling');
      handleWebSocketFailure();
    }, WEBSOCKET_CONNECT_TIMEOUT_MS);

    activeSocket.addEventListener('open', () => {
      if (socket !== activeSocket) return;
      clearSocketConnectTimer();
      try {
        activeSocket.send(TOP_SUBSCRIBE_PATH);
        if (templateWatchers > 0) activeSocket.send(TEMPLATE_SUBSCRIBE_PATH);
        if (transactionListeners.size > 0) activeSocket.send(TRANSACTION_SUBSCRIBE_PATH);
        stopPolling();
        websocketUnavailable = false;
        socketHeartbeatTimer = setInterval(() => {
          if (socket !== activeSocket) return;
          if (probeSentAt !== null) {
            if (Date.now() - probeSentAt >= WEBSOCKET_RESPONSE_TIMEOUT_MS) {
              console.warn('Libbitcoin websocket response timed out, falling back to polling');
              handleWebSocketFailure();
            }
            return;
          }
          try {
            // Native top queries reply even when no new block has arrived.
            activeSocket.send(TOP_QUERY_PATH);
            probeSentAt = Date.now();
          } catch (error) {
            console.warn('Libbitcoin websocket heartbeat failed, falling back to polling', error);
            handleWebSocketFailure();
          }
        }, WEBSOCKET_HEARTBEAT_INTERVAL_MS);
        setEventTransport('websocket');
        // Verify HTTP has also recovered before clearing a degraded state.
        if (httpAvailable === false) void checkForNewTop();
      } catch (error) {
        console.warn('Libbitcoin websocket subscribe failed, falling back to polling', error);
        handleWebSocketFailure();
      }
    });

    activeSocket.addEventListener('message', (event) => {
      if (socket !== activeSocket) return;
      void (async () => {
        try {
          const text = await normalizeTextMessage(event.data);
          if (text === null || socket !== activeSocket) return;
          const height = parseTemplateSubscriptionMessage(text);
          if (height !== null) {
            for (const listener of listeners) {
              listener({ type: 'template.new', payload: { height } });
            }
            return;
          }
          const txid = parseTransactionSubscriptionMessage(text);
          if (txid !== null) {
            for (const listener of transactionListeners) listener(txid);
            return;
          }
          const identifier = parseTopSubscriptionMessage(text);
          if (identifier === null) return;
          probeSentAt = null;
          // An open but unresponsive socket must not reset reconnect backoff.
          websocketRetryDelayMs = WEBSOCKET_RETRY_MIN_DELAY_MS;
          if (identifier === lastBlockHash) {
            // Heartbeats do not need to reload an unchanged block, but can
            // check whether a previously unavailable HTTP endpoint recovered.
            if (httpAvailable === false) void checkForNewTop();
            return;
          }
          await emitBlockEvent(identifier);
        } catch (error) {
          console.warn('Libbitcoin websocket event handling failed', error);
        }
      })();
    });

    activeSocket.addEventListener('error', (error) => {
      if (closingSocket || socket !== activeSocket) return;
      console.warn('Libbitcoin websocket failed, falling back to polling', error);
      handleWebSocketFailure();
    });

    activeSocket.addEventListener('close', () => {
      // Ignore close events from a superseded socket (a reconnect is active).
      if (socket !== null && socket !== activeSocket) return;
      clearSocketConnectTimer();
      clearSocketHeartbeatTimer();
      if (socket === activeSocket) {
        socket = null;
      }
      if (closingSocket) {
        closingSocket = false;
        return;
      }
      if (listeners.size === 0) return;
      handleWebSocketFailure();
    });

    return true;
  }

  function startEventStream() {
    if (startWebSocket()) return;
    startPolling();
    // If the websocket is in failure backoff, make sure a retry is pending.
    scheduleWebSocketRetry();
  }

  function stopEventStream() {
    clearStopEventStreamTimer();
    clearWebSocketRetryTimer();
    websocketRetryDelayMs = WEBSOCKET_RETRY_MIN_DELAY_MS;
    websocketUnavailable = false;
    stopPolling();
    closeSocket(true);
    setEventTransport('idle');
  }

  function stopEventStreamIfIdle() {
    if (listeners.size > 0 || stopEventStreamTimer) return;
    stopEventStreamTimer = setTimeout(() => {
      stopEventStreamTimer = null;
      if (listeners.size === 0) {
        stopEventStream();
      }
    }, EVENT_STREAM_STOP_DELAY_MS);
  }


  // The txs and details of the last pruned block listed, for its pages.
  const prunedBlocks = new Map<string, Promise<PrunedBlockTransactions>>();
  function fetchPrunedBlock(hash: string): Promise<PrunedBlockTransactions> {
    const cached = prunedBlocks.get(hash);
    if (cached) return cached;

    const path = `/v1/block/hash/${hash}`;
    const promise = Promise.all([
      fetchJson<unknown>(baseUrl, `${path}/txs?format=json`),
      fetchJson<unknown>(baseUrl, `${path}/txs/details?format=json`),
    ]).then(([hashes, details]) => ({
      hashes: hashesSchema.parse(hashes),
      details: templateSchema.parse(details),
    }));

    prunedBlocks.clear();
    prunedBlocks.set(hash, promise);
    promise.catch(() => {
      prunedBlocks.delete(hash);
    });

    return promise;
  }
  return {
    metadata: {
      label: 'Server',
      get connectionState() {
        return eventStatus.connectionState;
      },
      supportsWebsocket,
      endpoint: baseUrl,
      description: 'Adapter for the libbitcoin embedded REST API.',
    },

    getConfiguration() {
      return ensureConfiguration();
    },

    async getLatestBlocks(limit = 10): Promise<BlockSummary[]> {
      const tip = await observeHttp(fetchTopHeightCached(baseUrl));
      lastHeight = tip;
      const heights = Array.from({ length: limit }, (_, idx) => tip - idx).filter((h) => h >= 0);
      const summaries = await fetchBlockSummaries(baseUrl, heights);
      if (summaries.length > 0) {
        lastBlockHash = summaries[0].hash;
      }
      return summaries;
    },

    async getTransactionSizes(txid: string): Promise<TransactionSizes> {
      const path = `/v1/tx/${txid}/details?format=json`;
      const details = await fetchJson<TxDetailsResponse>(baseUrl, path);
      return { virtual: details.virtual, weight: details.weight, fee: details.fee };
    },

    async getHeaderTops(): Promise<HeaderTop[]> {
      const raw = await fetchJson<unknown>(baseUrl, '/v1/headers/top?format=json');
      return headerTopsSchema.parse(raw).map((top) => ({
        hash: top.hash,
        height: top.height,
        previous: top.header.previous,
        timestamp: top.header.timestamp,
        chain: top.chain,
      }));
    },

    // Each top not of the confirmed chain is walked back to the confirmed
    // block it builds on, to a limit of headers.
    async getBranches(): Promise<ChainBranch[]> {
      const tops = await this.getHeaderTops();
      const confirmed = tops.find((top) => top.chain === 'confirmed');
      const others = tops.filter((top) => top.chain !== 'confirmed');
      return mapWithConcurrency(others, REQUEST_CONCURRENCY, async (top) => {
        const headers: BranchHeader[] = [];
        let next: { hash: string; height: number; previous: string; timestamp: number } = top;
        for (let count = 0; count < BRANCH_WALK_LIMIT; count++) {
          headers.push({ hash: next.hash, height: next.height, timestamp: next.timestamp });
          if (next.height === 0) return { top, headers };

          const below = next.height - 1;
          const strong =
            confirmed !== undefined && below > confirmed.height
              ? null
              : await tryFetchJson<BlockHeaderResponse>(
                  baseUrl,
                  `/v1/block/confirmed/${String(below)}/header?format=json`
                );
          if (strong?.hash === next.previous) return { top, headers, fork: below };

          const parent = await tryFetchJson<BlockHeaderResponse>(
            baseUrl,
            `/v1/block/hash/${next.previous}/header?format=json`
          );
          if (!parent) return { top, headers };
          next = {
            hash: parent.hash,
            height: below,
            previous: parent.previous,
            timestamp: parent.timestamp,
          };
        }

        return { top, headers };
      });
    },

    async getUnconfirmedTransactionHashes(): Promise<string[]> {
      return toHashes(await fetchBytes(baseUrl, '/v1/txs/unconfirmed?format=data'));
    },

    async getUnconfirmedTransactionDetails(): Promise<TemplateTransaction[]> {
      const path = '/v1/txs/unconfirmed/details?format=data';
      return toDetails(await fetchBytes(baseUrl, path), true);
    },

    async getBlocksBefore(height: number, limit = 10): Promise<BlockSummary[]> {
      const heights = Array.from({ length: limit }, (_, idx) => height - idx).filter((h) => h >= 0);
      return fetchBlockSummaries(baseUrl, heights);
    },

    async getBlockHeader(hashOrHeight: string): Promise<BlockHeaderDetail> {
      const { path, height } = await resolveBlockPath(baseUrl, hashOrHeight);
      const tipPromise =
        height === undefined ? fetchTopHeightCached(baseUrl) : Promise.resolve(height);
      const [header, tip, context] = await Promise.all([
        fetchJson<BlockHeaderResponse>(baseUrl, `${path}/header?format=json`),
        tipPromise,
        tryFetchJson<unknown>(baseUrl, `${path}/header/context?format=json`),
      ]);
      lastHeight = Math.max(lastHeight ?? 0, tip);

      let nextBlockHash: string | null = null;
      if (header.height < tip) {
        const nextHeader = await tryFetchJson<BlockHeaderResponse>(
          baseUrl,
          `/v1/block/confirmed/${String(header.height + 1)}/header?format=json`
        );
        // The confirmed block above is next only where it builds on this one.
        nextBlockHash = nextHeader?.previous === header.hash ? nextHeader.hash : null;
      }

      const state = extractHeaderContextState(context);
      const summary = toBlockSummary(header, state?.count);
      return {
        ...summary,
        size: getHeaderContextMaximalSize(state),
        merkleRoot: header.merkle_root,
        nonce: header.nonce,
        bits: header.bits,
        version: header.version,
        nextBlockHash,
      };
    },

    async getBlockValidation(hash: string): Promise<BlockValidation> {
      const raw = await fetchJson<unknown>(
        baseUrl,
        `/v1/block/hash/${hash}/header/context?format=json`
      );
      const result = blockValidationSchema.safeParse(raw);
      if (!result.success) throw new TypeError('Invalid block validation response from Libbitcoin');
      const context = result.data;
      if (context.hash.toLowerCase() !== hash.toLowerCase()) {
        throw new Error('Block validation response does not match the requested block');
      }
      return {
        medianTimePast: context.mtp,
        state: context.state,
        rules: context.forks,
      };
    },

    async getBlockStats(hashOrHeight: string): Promise<BlockStats> {
      const { path } = await resolveBlockPath(baseUrl, hashOrHeight);
      const details = await tryFetchJson<BlockDetailsResponse>(
        baseUrl,
        `${path}/details?format=json`
      );
      return {
        weight: details?.weight,
        size: details?.maximal,
        nominal: details?.nominal,
        feesSats: details?.fees,
        subsidySats: details?.subsidy,
        sigops: details?.sigops,
      };
    },

    async getBlockTransactions(
      hashOrHeight: string,
      startIndex = 0
    ): Promise<TransactionSummary[]> {
      const networkParams = await resolveNetworkParams();
      const { path } = await resolveBlockPath(baseUrl, hashOrHeight);
      const [header, tip, context] = await Promise.all([
        fetchJson<BlockHeaderResponse>(baseUrl, `${path}/header?format=json`),
        fetchTopHeightCached(baseUrl),
        tryFetchJson<unknown>(baseUrl, `${path}/header/context?format=json`),
      ]);
      lastHeight = Math.max(lastHeight ?? 0, tip);

      const count = extractHeaderContextState(context)?.count;
      if (typeof count === 'number' && startIndex >= count) return [];

      const toRow = (tx: TxResponse, detail: TxDetailsResponse | null) => {
        const mapped = mapTransaction(
          tx,
          [], // No prevouts
          networkParams,
          {
            blockHeight: header.height,
            blockHash: header.hash,
            tipHeight: tip,
            timestamp: header.timestamp,
          },
          typeof detail?.fee === 'number' ? detail.fee : undefined
        );
        return applyTxDetails(mapped, detail);
      };

      // The txs of a pruned block are not provided whole, other than its coinbase.
      const { pruned } = await ensureConfiguration();
      if (pruned !== undefined && header.height <= pruned) {
        const block = { height: header.height, hash: header.hash, tip };
        const { hashes, details } = await fetchPrunedBlock(header.hash);
        const end = Math.min(startIndex + TRANSACTION_PAGE_SIZE, hashes.length);
        const rows = hashes
          .slice(startIndex, end)
          .map((hash, idx) => toPrunedSummary(hash, details[startIndex + idx], block));
        if (startIndex > 0 || rows.length === 0) return rows;

        const [coinbase] = await fetchBlockTransactionPositions(baseUrl, path, [0], true);
        if (coinbase === undefined) return rows;
        const detail = await tryFetchJson<TxDetailsResponse>(
          baseUrl,
          `/v1/tx/${coinbase.hash}/details?format=json`
        );
        return [toRow(coinbase, detail), ...rows.slice(1)];
      }

      const endExclusive =
        typeof count === 'number'
          ? Math.min(startIndex + TRANSACTION_PAGE_SIZE, count)
          : startIndex + TRANSACTION_PAGE_SIZE;
      const positions = Array.from(
        { length: Math.max(0, endExclusive - startIndex) },
        (_, idx) => startIndex + idx
      );

      const rawTxs = await fetchBlockTransactionPositions(
        baseUrl,
        path,
        positions,
        typeof count !== 'number'
      );
      if (rawTxs.length === 0) return [];

      const details = await mapWithConcurrency(rawTxs, REQUEST_CONCURRENCY, (tx) =>
        tryFetchJson<TxDetailsResponse>(baseUrl, `/v1/tx/${tx.hash}/details?format=json`)
      );

      return rawTxs.map((tx, idx) => toRow(tx, details[idx]));
    },

    async getBlockTransactionDetails(block: BlockReference): Promise<TemplateTransaction[]> {
      const id = encodeURIComponent(block.id);
      const path = `/v1/block/${block.chain}/${id}/txs/details?format=data`;
      return toDetails(await fetchBytes(baseUrl, path));
    },

    async getCandidateBlockHash(height: number): Promise<string> {
      const path = `/v1/block/candidate/${String(height)}/header?format=json`;
      const header = await fetchJson<BlockHeaderResponse>(baseUrl, path);
      return header.hash;
    },

    async getBlockTransactionHash(block: BlockReference, position: number): Promise<string> {
      const id = encodeURIComponent(block.id);
      const path = `/v1/block/${block.chain}/${id}/txs?format=json`;
      const hashes = hashesSchema.parse(await fetchJson<unknown>(baseUrl, path));
      const hash = hashes.at(position);
      if (hash === undefined) throw new LibbitcoinApiError('Transaction position not found', 404);
      return hash;
    },

    async getFees(mode?: EstimateMode): Promise<PoolFees> {
      const query = mode ? `&mode=${mode}` : '';
      return feesSchema.parse(await fetchJson<unknown>(baseUrl, `/v1/fees?format=json${query}`));
    },

    async getTemplate(height: number): Promise<TemplateTransaction[]> {
      const path = `/v1/template/${String(height)}/txs/details?format=data`;
      return toDetails(await fetchBytes(baseUrl, path));
    },

    async getBlockTransactionHashes(block: BlockReference): Promise<string[]> {
      const id = encodeURIComponent(block.id);
      const path = `/v1/block/${block.chain}/${id}/txs?format=data`;
      return toHashes(await fetchBytes(baseUrl, path));
    },

    async getTemplateTransactionHashes(height: number): Promise<string[]> {
      const path = `/v1/template/${String(height)}/txs?format=data`;
      return toHashes(await fetchBytes(baseUrl, path));
    },

    async getBlockMiner(hashOrHeight: string): Promise<Miner | null> {
      const { path } = await resolveBlockPath(baseUrl, hashOrHeight);
      return fetchMiner(path);
    },

    async getRecentMiners(count: number, signal?: AbortSignal): Promise<RecentMiners> {
      const top = await observeHttp(fetchTopHeightCached(baseUrl));
      const bottom = Math.max(0, top - count + 1);
      for (const height of minersByHeight.keys()) {
        if (height < bottom) minersByHeight.delete(height);
      }

      const heights = Array.from({ length: top - bottom + 1 }, (_, idx) => top - idx);
      const miners = await mapWithConcurrency(heights, REQUEST_CONCURRENCY, async (height) => {
        const cached = minersByHeight.get(height);
        if (cached !== undefined) return cached;
        const miner = await fetchMiner(`/v1/block/confirmed/${String(height)}`);
        if (height <= top - MINER_CACHE_DEPTH) minersByHeight.set(height, miner);
        return miner;
      }, signal);
      return { top, miners };
    },

    async getTransaction(txid: string): Promise<TransactionSummary> {
      const networkParams = await resolveNetworkParams();
      const tx = await fetchJson<TxResponse>(baseUrl, `/v1/tx/${txid}?format=json`);
      const resolvePrevout = createPrevoutResolver(baseUrl);
      const prevoutsPromise = mapWithConcurrency(tx.inputs, REQUEST_CONCURRENCY, (input) => {
        const isCoinbase = /^0{64}$/u.test(input.point.hash);
        if (isCoinbase) return Promise.resolve(null);
        return resolvePrevout(input.point.hash, input.point.index);
      });

      const [header, tip, details, prevouts] = await Promise.all([
        tryFetchJson<BlockHeaderResponse>(baseUrl, `/v1/tx/${txid}/header?format=json`),
        fetchTopHeightCached(baseUrl).catch(() => null),
        tryFetchJson<TxDetailsResponse>(baseUrl, `/v1/tx/${txid}/details?format=json`),
        prevoutsPromise,
      ]);
      if (typeof tip === 'number') {
        lastHeight = Math.max(lastHeight ?? 0, tip);
      }

      const mapped = mapTransaction(
        tx,
        prevouts,
        networkParams,
        {
          blockHeight: header?.height,
          blockHash: header?.hash,
          tipHeight: typeof tip === 'number' ? tip : undefined,
          timestamp: header?.timestamp ?? details?.time,
        },
        typeof details?.fee === 'number' ? details.fee : undefined
      );
      return applyTxDetails(mapped, details);
    },

    async getPrunedInputs(txid: string): Promise<TransactionInput[]> {
      const networkParams = await resolveNetworkParams();
      const inputs = await fetchJson<PrunedInputResponse[]>(
        baseUrl,
        `/v1/input/${txid}?format=json`
      );
      const resolvePrevout = createPrevoutResolver(baseUrl);
      const prevouts = await mapWithConcurrency(inputs, REQUEST_CONCURRENCY, (input) =>
        resolvePrevout(input.point.hash, input.point.index)
      );
      return inputs.map((input, index) =>
        mapPrunedInput(input, prevouts[index], networkParams, index)
      );
    },

    // Of the parts of a tx, which are provided when the tx itself is pruned:
    // its block, details and outputs. Its inputs are of the whole tx.
    async getTransactionHeader(txid: string): Promise<TransactionSummary> {
      const networkParams = await resolveNetworkParams();
      const [details, outputs, header, tip] = await Promise.all([
        fetchJson<TxDetailsResponse>(baseUrl, `/v1/tx/${txid}/details?format=json`),
        fetchJson<TxOutputResponse[]>(baseUrl, `/v1/output/${txid}?format=json`),
        tryFetchJson<BlockHeaderResponse>(baseUrl, `/v1/tx/${txid}/header?format=json`),
        fetchTopHeightCached(baseUrl).catch(() => null),
      ]);
      if (typeof tip === 'number') {
        lastHeight = Math.max(lastHeight ?? 0, tip);
      }

      const height = header?.height;
      const confirmed = typeof height === 'number';
      const confirmations =
        confirmed && typeof tip === 'number' ? Math.max(0, tip - height + 1) : 0;
      return {
        txid,
        hash: txid,
        status: confirmed ? 'confirmed' : 'unconfirmed',
        confirmable: details.confirmable === true,
        coinbase: details.coinbase === true,
        confirmations,
        blockHeight: height,
        blockHash: header?.hash,
        receivedTime: header?.timestamp ?? details.time,
        feeSats: details.fee ?? null,
        feeRate: feeRate(details),
        size: details.maximal,
        weight: details.weight,
        totalInput: details.coinbase ? null : (details.value ?? null),
        totalOutput: details.spend ?? 0,
        inputs: [],
        outputs: outputs.map((output, index) => mapOutput(output, index, networkParams)),
      };
    },

    async getTransactionInputScriptHex(txid: string, inputIndex: number): Promise<string> {
      const raw = await fetchJson<string>(
        baseUrl,
        `/v1/input/${txid}/${String(inputIndex)}/script?format=text`
      );
      return raw.trim();
    },

    async getTransactionOutputScriptHex(txid: string, outputIndex: number): Promise<string> {
      const raw = await fetchJson<string>(
        baseUrl,
        `/v1/output/${txid}/${String(outputIndex)}/script?format=text`
      );
      return raw.trim();
    },

    getTransactionOutputSpends(txid, outputIndices): Promise<TransactionOutputSpend[]> {
      return mapWithConcurrency(outputIndices, SPENDER_LOOKUP_CONCURRENCY, async (outputIndex) => {
        try {
          const spender = await fetchOutputSpender(baseUrl, txid, outputIndex);
          if (spender === null) return { outputIndex, status: 'unspent' };
          if (!isHexHash(spender.hash) || spender.index > 0xffffffff) {
            return { outputIndex, status: 'unknown' };
          }
          return {
            outputIndex,
            status: 'spent',
            txid: spender.hash.toLowerCase(),
            inputIndex: spender.index,
          };
        } catch {
          // Preserve successful siblings when a node cannot resolve one output.
          return { outputIndex, status: 'unknown' };
        }
      });
    },

    async getAddressSummary(address: string): Promise<AddressSummary> {
      const networkParams = await resolveNetworkParams();
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) throw new Error('Unsupported address format for libbitcoin adapter');

      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) {
        throw new AddressQueriesDisabledError();
      }
      const turbo = turboAddress && isTurboEnabled(config);

      const [balanceRaw, utxos] = await Promise.all([
        tryFetchJson<unknown>(baseUrl, `/v1/address/${scriptInfo.hash}/balance?format=json`, turbo),
        getConfirmedUtxosCached(scriptInfo.hash, turbo),
      ]);
      const balance = resolveAddressBalance(balanceRaw, utxos);

      return {
        address,
        addressType: scriptInfo.type,
        balance,
        totalReceived: undefined,
        totalSent: undefined,
        txCount: undefined,
        utxoCount: utxos.length,
      };
    },

    async getAddressUtxos(address: string): Promise<AddressUtxo[]> {
      const networkParams = await resolveNetworkParams();
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) throw new Error('Unsupported address format for libbitcoin adapter');

      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) {
        throw new AddressQueriesDisabledError();
      }
      const turbo = turboAddress && isTurboEnabled(config);
      const utxos = await getConfirmedUtxosCached(scriptInfo.hash, turbo);

      return utxos.map((utxo) => ({
        txid: utxo.point.hash,
        outputIndex: utxo.point.index,
        valueSats: utxo.value,
        status: 'confirmed',
      }));
    },

    async getAddressTransactions(address: string, cursor?: string): Promise<AddressActivityItem[]> {
      const networkParams = await resolveNetworkParams();
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) return [];
      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) throw new AddressQueriesDisabledError();
      const turbo = turboAddress && isTurboEnabled(config);
      return buildAddressActivityPage(scriptInfo.hash, turbo, cursor);
    },

    subscribeToEvents(handler: BlockchainEventHandler): () => void {
      clearStopEventStreamTimer();
      listeners.add(handler);
      startEventStream();
      return () => {
        listeners.delete(handler);
        stopEventStreamIfIdle();
      };
    },

    // Template notifications demand templates of the node, so are subscribed
    // only while something watches them.
    watchTemplates(): () => void {
      if (templateWatchers++ === 0 && socket?.readyState === 1) {
        socket.send(TEMPLATE_SUBSCRIBE_PATH);
      }
      return () => {
        if (--templateWatchers === 0 && socket?.readyState === 1) {
          socket.send(TEMPLATE_UNSUBSCRIBE_PATH);
        }
      };
    },

    // The tx stream is carried only while something listens for it.
    subscribeToTransactions(handler: (txid: string) => void): () => void {
      transactionListeners.add(handler);
      if (transactionListeners.size === 1 && socket?.readyState === 1) {
        socket.send(TRANSACTION_SUBSCRIBE_PATH);
      }
      return () => {
        transactionListeners.delete(handler);
        if (transactionListeners.size === 0 && socket?.readyState === 1) {
          socket.send(TRANSACTION_UNSUBSCRIBE_PATH);
        }
      };
    },

    getEventStatus,

    subscribeToEventStatus(handler: BlockchainEventStatusHandler): () => void {
      statusListeners.add(handler);
      handler(getEventStatus());
      return () => {
        statusListeners.delete(handler);
      };
    },
  };
}
