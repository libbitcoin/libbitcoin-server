import { createContext, useCallback, useContext, useSyncExternalStore } from 'react';
import type { ReactNode } from 'react';
import type { Miner } from '@/lib/utils/miner';
import { createLibbitcoinClient } from './providers/libbitcoin';

/** A block by hash, confirmed height or candidate height. */
export interface BlockReference {
  chain: 'hash' | 'confirmed' | 'candidate';
  id: string;
}

/** Totals of the pool and its rates (sat/vB) by virtual size. */
export interface PoolFees {
  count: number;
  virtual: number;
  fee: number;
  histogram: Array<[number, number]>;
}

/** Details of a template tx, with its cluster within the template. */
export interface TemplateTransaction {
  segregated: boolean;
  nominal: number;
  maximal: number;
  weight: number;
  virtual: number;
  value: number;
  spend: number;
  fee: number;
  cluster: number;
}

export interface RecentMiners {
  top: number;
  /** Newest first; null where the coinbase matches no known pool. */
  miners: Array<Miner | null>;
}

export interface BlockSummary {
  hash: string;
  height: number;
  timestamp: number;
  txCount?: number;
  size?: number;
  weight?: number;
  difficulty?: number;
  previousBlockHash?: string | null;
}

export interface NetworkParams {
  p2pkh: number;
  p2sh: number;
  hrp: string;
}

export interface BlockchainClientConfiguration {
  address: boolean;
  filter: boolean;
  turbo: boolean;
  silent: boolean;
  services: Readonly<Partial<Record<string, boolean>>>;
  protocol: Readonly<Partial<Record<string, boolean>>>;
  coin: string;
  network?: string;
  forks: string[];
  wallet: NetworkParams;
  /** Height at and below which tx inputs (scripts and witnesses) are pruned. */
  pruned?: number;
}

export interface BlockStats {
  weight?: number;
  size?: number;
  nominal?: number;
  feesSats?: number;
  subsidySats?: number;
  sigops?: number;
}

interface ScriptData {
  hex?: string;
  mnemonic?: string; //usually refered to as asm
}

export interface TransactionInput {
  index: number;
  prevTxId?: string;
  prevOutputIndex?: number;
  address?: string;
  valueSats?: number;
  scriptType?: string;
  isCoinbase?: boolean;
  script?: ScriptData;
  witness?: string[];
  sequence?: number;
  /** The script and witness are not provided. */
  pruned?: boolean;
}

export interface TransactionOutput {
  index: number;
  address?: string;
  valueSats: number;
  scriptType?: string;
  spent?: boolean;
  script?: ScriptData;
}

export type TransactionOutputSpend = { outputIndex: number } & (
  | { status: 'unspent' }
  | { status: 'spent'; txid: string; inputIndex: number }
  | { status: 'unknown' }
);

export interface TransactionSummary {
  txid: string;
  hash: string;
  wtxid?: string;
  status: 'unconfirmed' | 'confirmed';
  confirmations: number;
  blockHeight?: number;
  blockHash?: string;
  receivedTime?: number;
  feeSats?: number | null;
  feeRate?: number | null;
  size?: number;
  weight?: number;
  sigops?: number;
  version?: number;
  locktime?: number;
  totalInput?: number | null;
  totalOutput: number;
  inputs: TransactionInput[];
  outputs: TransactionOutput[];
}

export interface BlockHeaderDetail extends BlockSummary {
  merkleRoot?: string;
  nonce?: number;
  bits?: number;
  version?: number;
  nextBlockHash?: string | null;
  feesSats?: number;
  subsidySats?: number;
  sigops?: number;
}

export interface BlockValidation {
  medianTimePast?: number;
  state?: {
    validated?: boolean;
    confirmed?: boolean;
    confirmable?: boolean;
    unconfirmable?: boolean;
  };
  /** Flags reported for this block's context, not inferred from activation heights. */
  rules?: Record<string, boolean>;
}

export interface AddressActivityItem {
  txid: string;
  valueChange: number;
  /** Output index within the txid for received rows. */
  outputIndex?: number;
  /** Stable row ID in the received/spent ledger. */
  id: string;
  /** Direction relative to the address, in the received/spent ledger. */
  direction: 'received' | 'sent';
  /** For received rows: whether that output has been spent (undefined = not determined). */
  spent?: boolean;
  /** Opaque cursor to fetch the page after this item; absent on the last page. */
  cursor?: string;
}

export interface AddressUtxo {
  txid: string;
  outputIndex: number;
  valueSats: number;
  status: 'confirmed' | 'unconfirmed';
  blockHeight?: number;
  blockHash?: string;
  timestamp?: number;
}

export interface AddressSummary {
  address: string;
  addressType?: string;
  balance: number;
  totalReceived?: number;
  totalSent?: number;
  txCount?: number;
  utxoCount?: number;
}

export interface BlockchainEvent {
  type: 'block.new';
  payload: BlockSummary;
}

export type BlockchainEventHandler = (event: BlockchainEvent) => void;
export type BlockchainEventTransport = 'idle' | 'connecting' | 'websocket' | 'polling';

export interface BlockchainEventStatus {
  transport: BlockchainEventTransport;
  websocketAvailable: boolean;
  connectionState: BlockchainClientMetadata['connectionState'];
}

export type BlockchainEventStatusHandler = (status: BlockchainEventStatus) => void;

export interface BlockchainClientMetadata {
  label: string;
  connectionState: 'connected' | 'degraded' | 'offline' | 'unknown';
  supportsWebsocket: boolean;
  endpoint?: string;
  description?: string;
}

export interface BlockchainClient {
  metadata: BlockchainClientMetadata;
  getConfiguration(): Promise<BlockchainClientConfiguration>;
  getLatestBlocks(limit?: number): Promise<BlockSummary[]>;
  getBlocksBefore(height: number, limit?: number): Promise<BlockSummary[]>;
  getBlockHeader(hashOrHeight: string): Promise<BlockHeaderDetail>;
  /** Query a specific block hash so a height lookup cannot switch blocks mid-request. */
  getBlockValidation(hash: string): Promise<BlockValidation>;
  getBlockStats(hashOrHeight: string): Promise<BlockStats>;
  getBlockTransactions(hashOrHeight: string, startIndex?: number): Promise<TransactionSummary[]>;
  /** Txs of the block in block order, with their clusters within the block. */
  getBlockTransactionDetails(block: BlockReference): Promise<TemplateTransaction[]>;
  /** Hash of the candidate block at height. */
  getCandidateBlockHash(height: number): Promise<string>;
  /** Hash of the tx at position in the block. */
  getBlockTransactionHash(block: BlockReference, position: number): Promise<string>;
  getFees(): Promise<PoolFees>;
  /** Txs of the template of the block at height, in block order (less the coinbase). */
  getTemplate(height: number): Promise<TemplateTransaction[]>;
  /** Hashes of the txs of the template of the block at height, in block order. */
  getTemplateTransactionHashes(height: number): Promise<string[]>;
  /** Pool identified from the block's coinbase, null when unrecognized. */
  getBlockMiner(hashOrHeight: string): Promise<Miner | null>;
  getRecentMiners(count: number, signal?: AbortSignal): Promise<RecentMiners>;
  getTransactionHeader(txid: string): Promise<TransactionSummary>;
  getTransaction(txid: string): Promise<TransactionSummary>;
  getPrunedInputs(txid: string): Promise<TransactionInput[]>;
  getTransactionInputScriptHex(txid: string, inputIndex: number): Promise<string>;
  getTransactionOutputScriptHex(txid: string, outputIndex: number): Promise<string>;
  /** Spend status for known outputs; failed lookups must never be reported as unspent. */
  getTransactionOutputSpends(
    txid: string,
    outputIndices: number[]
  ): Promise<TransactionOutputSpend[]>;
  getAddressSummary(address: string): Promise<AddressSummary>;
  getAddressUtxos(address: string): Promise<AddressUtxo[]>;
  getAddressTransactions(address: string, cursor?: string): Promise<AddressActivityItem[]>;
  subscribeToEvents(handler: BlockchainEventHandler): () => void;
  getEventStatus?(): BlockchainEventStatus;
  subscribeToEventStatus?(handler: BlockchainEventStatusHandler): () => void;
}

const BlockchainClientContext = createContext<BlockchainClient | null>(null);
const TurboContext = createContext<{
  turbo: boolean;
  setTurbo: (value: boolean) => void;
} | null>(null);

export function BlockchainClientProvider({
  children,
  client,
}: {
  children: ReactNode;
  client: BlockchainClient;
}) {
  return (
    <BlockchainClientContext.Provider value={client}>{children}</BlockchainClientContext.Provider>
  );
}

export function TurboProvider({
  children,
  turbo,
  setTurbo,
}: {
  children: ReactNode;
  turbo: boolean;
  setTurbo: (value: boolean) => void;
}) {
  return <TurboContext.Provider value={{ turbo, setTurbo }}>{children}</TurboContext.Provider>;
}

export function useBlockchainClient(): BlockchainClient {
  const client = useContext(BlockchainClientContext);
  if (!client) {
    throw new Error('BlockchainClientProvider missing in component tree');
  }
  return client;
}

export function useServerInfo() {
  const client = useBlockchainClient();
  const subscribe = useCallback(
    (onChange: () => void) => client.subscribeToEventStatus?.(onChange) ?? (() => {}),
    [client]
  );
  const getConnectionState = useCallback(
    () => client.getEventStatus?.().connectionState ?? client.metadata.connectionState,
    [client]
  );
  const connectionState = useSyncExternalStore(subscribe, getConnectionState, getConnectionState);
  return { ...client.metadata, connectionState };
}

export function useTurbo() {
  const ctx = useContext(TurboContext);
  if (!ctx) {
    throw new Error('TurboProvider missing in component tree');
  }
  return ctx;
}

function resolveLibbitcoinBaseUrl() {
  if (import.meta.env.VITE_LIBBITCOIN_API_URL) {
    return import.meta.env.VITE_LIBBITCOIN_API_URL;
  }
  const window_ = (globalThis as unknown as { window?: Window }).window;
  if (window_?.location.origin) return window_.location.origin;
  return 'http://localhost:8080';
}

export function createBlockchainClient(options?: { turboEnabled?: boolean }): BlockchainClient {
  return createLibbitcoinClient({
    baseUrl: resolveLibbitcoinBaseUrl(),
    turbo: options?.turboEnabled,
  });
}
