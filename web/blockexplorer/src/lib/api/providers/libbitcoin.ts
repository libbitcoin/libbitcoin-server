import { z } from 'zod';
import {
  AddressActivityItem,
  AddressDetails,
  AddressSummary,
  AddressUtxo,
  BlockchainClientConfiguration,
  BlockDetail,
  BlockHeaderDetail,
  BlockValidation,
  BlockStats,
  BlockSummary,
  BlockchainClient,
  BlockchainEventHandler,
  BlockchainEventStatus,
  BlockchainEventStatusHandler,
  BlockchainEventTransport,
  TransactionSummary,
  TransactionOutputSpend,
} from '../client';
import {
  BlockHeaderResponse,
  TxOutputResponse,
  TxPoint,
  TxResponse,
  getNetwork,
  getNetworkParams,
  scriptHashFromAddress,
  toBlockSummary,
  mapTransaction,
} from './libbitcoin-mapper';

interface LibbitcoinClientOptions {
  baseUrl: string;
  pollIntervalMs?: number;
  turbo?: boolean;
}

const TRANSACTION_PAGE_SIZE = 25;
// Bounds parallel node requests for multi-item fetches (block lists, tx pages,
// prevout resolution) so bursts don't overwhelm the embedded node.
const REQUEST_CONCURRENCY = 8;
const TOP_SUBSCRIBE_PATH = '/v1/top/subscribe?format=json';
const TOP_UNSUBSCRIBE_PATH = '/v1/top/subscribe?format=json&stop=true';
const WEBSOCKET_CONNECT_TIMEOUT_MS = 5_000;
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
  confirmed?: TxDetailsConfirmedResponse;
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

const DEFAULT_CONFIGURATION: BlockchainClientConfiguration = {
  address: true,
  filter: false,
  turbo: true,
  witness: true,
  retarget: false,
  difficult: false,
};

function normalizeConfiguration(raw: unknown): BlockchainClientConfiguration {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) {
    return DEFAULT_CONFIGURATION;
  }

  const record = raw as Record<string, unknown>;
  const coerceBoolean = (key: keyof BlockchainClientConfiguration) =>
    typeof record[key] === 'boolean' ? record[key] : DEFAULT_CONFIGURATION[key];

  return {
    address: coerceBoolean('address'),
    filter: coerceBoolean('filter'),
    turbo: coerceBoolean('turbo'),
    witness: coerceBoolean('witness'),
    retarget: coerceBoolean('retarget'),
    difficult: coerceBoolean('difficult'),
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
  // In header context, size is nominal bytes and weight is maximal serialized bytes.
  return state?.weight ?? state?.size;
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

  const promise = (async () => {
    const response = await fetch(url, {
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

  inFlightRequests.set(url, promise);

  try {
    return await promise;
  } finally {
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

async function fetchTopHeight(baseUrl: string, turbo?: boolean) {
  const raw = await fetchJson<unknown>(baseUrl, '/v1/top?format=json', turbo);
  const height = typeof raw === 'number' ? raw : Number(raw);
  if (!Number.isFinite(height)) throw new Error('Invalid top height');
  return height;
}

function isHexHash(value: string) {
  return /^[0-9a-f]{64}$/iu.test(value);
}

function parseNumericHeight(value: unknown): number | null {
  if (typeof value === 'number' && Number.isInteger(value) && value >= 0) return value;
  if (typeof value === 'string' && /^\d+$/u.test(value.trim())) return Number(value.trim());
  return null;
}

function extractTopSubscriptionIdentifier(message: unknown): string | number | null {
  const height = parseNumericHeight(message);
  if (height !== null) return height;

  if (typeof message === 'string') {
    const trimmed = message.trim();
    return isHexHash(trimmed) ? trimmed : null;
  }

  if (!message || typeof message !== 'object' || Array.isArray(message)) return null;

  const record = message as Record<string, unknown>;
  const candidates = [
    record.payload,
    record.top,
    record.block,
    record.header,
    record.hash,
    record.height,
  ];

  for (const candidate of candidates) {
    if (candidate === message) continue;
    const identifier = extractTopSubscriptionIdentifier(candidate);
    if (identifier !== null) return identifier;
  }

  return null;
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

function parseTopSubscriptionMessage(text: string): string | number | null {
  const trimmed = text.trim();
  if (!trimmed) return null;

  try {
    return extractTopSubscriptionIdentifier(JSON.parse(trimmed));
  } catch {
    return extractTopSubscriptionIdentifier(trimmed);
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
    return { path: `/v1/block/height/${String(tip)}`, height: tip };
  }
  if (/^\d+$/u.test(hashOrHeight)) {
    return { path: `/v1/block/height/${hashOrHeight}` };
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
      fetchJson<BlockHeaderResponse>(baseUrl, `/v1/block/height/${String(h)}/header?format=json`),
      tryFetchJson<unknown>(baseUrl, `/v1/block/height/${String(h)}/header/context?format=json`),
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
// preserving input order in the result.
async function mapWithConcurrency<T, R>(
  items: T[],
  limit: number,
  fn: (item: T, index: number) => Promise<R>
): Promise<R[]> {
  const results = Array.from<R>({ length: items.length });
  let next = 0;
  async function worker() {
    while (next < items.length) {
      const current = next;
      next += 1;
      results[current] = await fn(items[current], current);
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
  const statusListeners = new Set<BlockchainEventStatusHandler>();
  let poller: ReturnType<typeof setInterval> | null = null;
  let socket: WebSocket | null = null;
  let socketConnectTimer: ReturnType<typeof setTimeout> | null = null;
  let stopEventStreamTimer: ReturnType<typeof setTimeout> | null = null;
  let websocketRetryTimer: ReturnType<typeof setTimeout> | null = null;
  let websocketRetryDelayMs = WEBSOCKET_RETRY_MIN_DELAY_MS;
  let websocketUnavailable = false;
  let closingSocket = false;
  let eventTransport: BlockchainEventTransport = 'idle';
  let lastHeight: number | null = null;
  let lastBlockHash: string | null = null;
  const networkParams = getNetworkParams(getNetwork());
  let configuration: BlockchainClientConfiguration | null = null;
  let configurationPromise: Promise<BlockchainClientConfiguration> | null = null;
  const websocketUrl = resolveWebSocketUrl(baseUrl);
  const supportsWebsocket = Boolean(websocketUrl && globalThis.WebSocket);
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
    configurationPromise = fetchJson<unknown>(baseUrl, '/v1/configuration?format=json')
      .then((raw) => {
        const cfg = normalizeConfiguration(raw);
        configuration = cfg;
        return cfg;
      })
      .catch((error: unknown) => {
        console.warn('Libbitcoin configuration fetch failed', error);
        configuration = DEFAULT_CONFIGURATION;
        return DEFAULT_CONFIGURATION;
      });
    return configurationPromise;
  }

  function isAddressEnabled(cfg: BlockchainClientConfiguration) {
    return cfg.address;
  }

  function isTurboEnabled(cfg: BlockchainClientConfiguration) {
    return isAddressEnabled(cfg) && cfg.turbo;
  }

  function getEventStatus(): BlockchainEventStatus {
    return {
      transport: eventTransport,
      websocketAvailable: supportsWebsocket && !websocketUnavailable,
    };
  }

  function setEventTransport(transport: BlockchainEventTransport) {
    if (eventTransport === transport) return;
    eventTransport = transport;
    const status = getEventStatus();
    for (const listener of statusListeners) {
      listener(status);
    }
  }

  async function emitBlockEvent(identifier: string | number) {
    const path =
      typeof identifier === 'number'
        ? `/v1/block/height/${String(identifier)}`
        : `/v1/block/hash/${identifier}`;
    const [header, context] = await Promise.all([
      fetchJson<BlockHeaderResponse>(baseUrl, `${path}/header?format=json`),
      tryFetchJson<unknown>(baseUrl, `${path}/header/context?format=json`),
    ]);

    if (lastBlockHash === header.hash || (lastHeight !== null && header.height < lastHeight)) {
      lastHeight = Math.max(lastHeight ?? 0, header.height);
      lastBlockHash = header.hash;
      return;
    }

    const state = extractHeaderContextState(context);
    const summary = toBlockSummary(header, state?.count);
    const enriched: BlockSummary = {
      ...summary,
      size: getHeaderContextMaximalSize(state),
    };
    lastHeight = Math.max(lastHeight ?? 0, header.height);
    lastBlockHash = header.hash;

    for (const listener of listeners) {
      listener({ type: 'block.new', payload: enriched });
    }
  }

  async function checkForNewTop() {
    try {
      const height = await fetchTopHeight(baseUrl);
      if (lastHeight === null) {
        lastHeight = height;
        return;
      }
      if (height > lastHeight) {
        await emitBlockEvent(height);
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
    if (!socket) return;
    closingSocket = true;
    try {
      if (sendStop && socket.readyState === 1) {
        socket.send(TOP_UNSUBSCRIBE_PATH);
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
        stopPolling();
        websocketUnavailable = false;
        websocketRetryDelayMs = WEBSOCKET_RETRY_MIN_DELAY_MS;
        setEventTransport('websocket');
      } catch (error) {
        console.warn('Libbitcoin websocket subscribe failed, falling back to polling', error);
        handleWebSocketFailure();
      }
    });

    activeSocket.addEventListener('message', (event) => {
      void (async () => {
        try {
          const text = await normalizeTextMessage(event.data);
          if (text === null) return;
          const identifier = parseTopSubscriptionMessage(text);
          if (identifier === null) return;
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

  return {
    metadata: {
      id: 'libbitcoin',
      label: 'Server',
      type: 'local',
      connectionState: 'connected',
      supportsWebsocket,
      endpoint: baseUrl,
      description: 'Adapter for the libbitcoin embedded REST API.',
    },

    getConfiguration() {
      return ensureConfiguration();
    },

    async getLatestBlocks(limit = 10): Promise<BlockSummary[]> {
      const tip = await fetchTopHeightCached(baseUrl);
      lastHeight = tip;
      const heights = Array.from({ length: limit }, (_, idx) => tip - idx).filter((h) => h >= 0);
      const summaries = await fetchBlockSummaries(baseUrl, heights);
      if (summaries.length > 0) {
        lastBlockHash = summaries[0].hash;
      }
      return summaries;
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
          `/v1/block/height/${String(header.height + 1)}/header?format=json`
        );
        nextBlockHash = nextHeader?.hash ?? null;
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

    async getBlock(hashOrHeight: string): Promise<BlockDetail> {
      const { path, height } = await resolveBlockPath(baseUrl, hashOrHeight);
      const tipPromise =
        height === undefined ? fetchTopHeightCached(baseUrl) : Promise.resolve(height);
      const [header, tip, context, details] = await Promise.all([
        fetchJson<BlockHeaderResponse>(baseUrl, `${path}/header?format=json`),
        tipPromise,
        tryFetchJson<unknown>(baseUrl, `${path}/header/context?format=json`),
        tryFetchJson<BlockDetailsResponse>(baseUrl, `${path}/details?format=json`),
      ]);
      lastHeight = Math.max(lastHeight ?? 0, tip);

      let nextBlockHash: string | null = null;
      if (header.height < tip) {
        const nextHeader = await tryFetchJson<BlockHeaderResponse>(
          baseUrl,
          `/v1/block/height/${String(header.height + 1)}/header?format=json`
        );
        nextBlockHash = nextHeader?.hash ?? null;
      }

      const state = extractHeaderContextState(context);
      const txCount = state?.count ?? details?.count;
      const summary = toBlockSummary(header, txCount);

      const pageSize =
        typeof txCount === 'number'
          ? Math.min(TRANSACTION_PAGE_SIZE, txCount)
          : TRANSACTION_PAGE_SIZE;
      const positions = Array.from({ length: pageSize }, (_, idx) => idx);
      const rawTxs = await fetchBlockTransactionPositions(
        baseUrl,
        path,
        positions,
        typeof txCount !== 'number'
      );
      const txDetails = await mapWithConcurrency(rawTxs, REQUEST_CONCURRENCY, (tx) =>
        tryFetchJson<TxDetailsResponse>(baseUrl, `/v1/tx/${tx.hash}/details?format=json`)
      );

      const txs = rawTxs.map((tx, idx) => {
        const detail = txDetails[idx];
        const mapped = mapTransaction(
          tx,
          [], // No prevouts for block list
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
      });

      return {
        ...summary,
        size: details?.maximal ?? getHeaderContextMaximalSize(state),
        weight: details?.weight,
        merkleRoot: header.merkle_root,
        nonce: header.nonce,
        bits: header.bits,
        version: header.version,
        nextBlockHash,
        feesSats: details?.fees,
        subsidySats: details?.subsidy,
        sigops: details?.sigops,
        transactions: txs,
      };
    },

    async getBlockTransactions(
      hashOrHeight: string,
      startIndex = 0
    ): Promise<TransactionSummary[]> {
      const { path } = await resolveBlockPath(baseUrl, hashOrHeight);
      const [header, tip, context] = await Promise.all([
        fetchJson<BlockHeaderResponse>(baseUrl, `${path}/header?format=json`),
        fetchTopHeightCached(baseUrl),
        tryFetchJson<unknown>(baseUrl, `${path}/header/context?format=json`),
      ]);
      lastHeight = Math.max(lastHeight ?? 0, tip);

      const count = extractHeaderContextState(context)?.count;
      if (typeof count === 'number' && startIndex >= count) return [];

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

      return rawTxs.map((tx, idx) => {
        const detail = details[idx];
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
      });
    },

    async getTransaction(txid: string): Promise<TransactionSummary> {
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
          timestamp: header?.timestamp,
        },
        typeof details?.fee === 'number' ? details.fee : undefined
      );
      return applyTxDetails(mapped, details);
    },

    async getTransactionHeader(txid: string): Promise<TransactionSummary> {
      const tx = await fetchJson<TxResponse>(baseUrl, `/v1/tx/${txid}?format=json`);
      const [header, tip, details] = await Promise.all([
        tryFetchJson<BlockHeaderResponse>(baseUrl, `/v1/tx/${txid}/header?format=json`),
        fetchTopHeightCached(baseUrl).catch(() => null),
        tryFetchJson<TxDetailsResponse>(baseUrl, `/v1/tx/${txid}/details?format=json`),
      ]);
      if (typeof tip === 'number') {
        lastHeight = Math.max(lastHeight ?? 0, tip);
      }

      const mapped = mapTransaction(
        tx,
        [],
        networkParams,
        {
          blockHeight: header?.height,
          blockHash: header?.hash,
          tipHeight: typeof tip === 'number' ? tip : undefined,
          timestamp: header?.timestamp,
        },
        typeof details?.fee === 'number' ? details.fee : undefined
      );
      return applyTxDetails(mapped, details);
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

    async getAddress(address: string): Promise<AddressDetails> {
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) throw new Error('Unsupported address format for libbitcoin adapter');

      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) {
        throw new Error('Address endpoints are disabled by server configuration');
      }
      const turbo = turboAddress && isTurboEnabled(config);

      const [balanceRaw, utxos] = await Promise.all([
        tryFetchJson<unknown>(baseUrl, `/v1/address/${scriptInfo.hash}/balance?format=json`, turbo),
        getConfirmedUtxosCached(scriptInfo.hash, turbo),
      ]);

      const balance = resolveAddressBalance(balanceRaw, utxos);
      const transactions = await buildAddressActivityPage(scriptInfo.hash, turbo);

      return {
        address,
        addressType: scriptInfo.type,
        balance,
        totalReceived: undefined,
        totalSent: undefined,
        txCount: undefined,
        utxoCount: utxos.length,
        transactions,
      };
    },

    async getAddressSummary(address: string): Promise<AddressSummary> {
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) throw new Error('Unsupported address format for libbitcoin adapter');

      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) {
        throw new Error('Address endpoints are disabled by server configuration');
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
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) throw new Error('Unsupported address format for libbitcoin adapter');

      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) {
        throw new Error('Address endpoints are disabled by server configuration');
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
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) return [];
      const config = configuration ?? (await ensureConfiguration());
      if (!isAddressEnabled(config)) return [];
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
