import { createContext, useContext } from 'react';
import type { ReactNode } from 'react';
import { createBlockstreamClient } from './providers/blockstream';
import { createMempoolClient } from './providers/mempool';
import { createLibbitcoinClient } from './providers/libbitcoin';

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

export interface BlockchainClientConfiguration {
  address: boolean;
  filter: boolean;
  turbo: boolean;
  witness: boolean;
  retarget: boolean;
  difficult: boolean;
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
  version: number;
  locktime: number;
  totalInput?: number | null;
  totalOutput: number;
  inputs: TransactionInput[];
  outputs: TransactionOutput[];
}

export interface BlockDetail extends BlockSummary {
  merkleRoot?: string;
  nonce?: number;
  bits?: number;
  version?: number;
  nextBlockHash?: string | null;
  feesSats?: number;
  subsidySats?: number;
  sigops?: number;
  transactions: TransactionSummary[];
}

export type BlockHeaderDetail = Omit<BlockDetail, 'transactions'>;

export interface BlockValidation {
  medianTimePast?: number;
  inBestChain?: boolean;
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
  timestamp?: number;
  valueChange: number;
  blockHeight?: number;
  /** Output index within the txid, when an activity row maps to a specific output (libbitcoin). */
  outputIndex?: number;
  /** Stable unique row id when the provider can supply one (libbitcoin received/spent ledger). */
  id?: string;
  /** Direction relative to the address, when reconstructed as a ledger (libbitcoin). */
  direction?: 'received' | 'sent';
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

export interface AddressDetails {
  address: string;
  addressType?: string;
  balance: number;
  totalReceived?: number;
  totalSent?: number;
  txCount?: number;
  utxoCount?: number;
  transactions: AddressActivityItem[];
}

export type AddressSummary = Omit<AddressDetails, 'transactions'>;

export type BlockchainEvent =
  | { type: 'block.new'; payload: BlockSummary }
  | { type: 'tx.new'; payload: TransactionSummary };

export type BlockchainEventHandler = (event: BlockchainEvent) => void;
export type BlockchainEventTransport = 'idle' | 'connecting' | 'websocket' | 'polling';

export interface BlockchainEventStatus {
  transport: BlockchainEventTransport;
  websocketAvailable: boolean;
}

export type BlockchainEventStatusHandler = (status: BlockchainEventStatus) => void;

export interface BlockchainClientMetadata {
  id: string;
  label: string;
  type: 'external' | 'local';
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
  getBlock(hashOrHeight: string): Promise<BlockDetail>;
  getBlockTransactions(hashOrHeight: string, startIndex?: number): Promise<TransactionSummary[]>;
  getTransactionHeader(txid: string): Promise<TransactionSummary>;
  getTransaction(txid: string): Promise<TransactionSummary>;
  getTransactionInputScriptHex(txid: string, inputIndex: number): Promise<string>;
  getTransactionOutputScriptHex(txid: string, outputIndex: number): Promise<string>;
  /** Spend status for known outputs; failed lookups must never be reported as unspent. */
  getTransactionOutputSpends(
    txid: string,
    outputIndices: number[]
  ): Promise<TransactionOutputSpend[]>;
  getAddressSummary(address: string): Promise<AddressSummary>;
  getAddress(address: string): Promise<AddressDetails>;
  getAddressUtxos(address: string): Promise<AddressUtxo[]>;
  getAddressTransactions(address: string, lastSeenTxid?: string): Promise<AddressActivityItem[]>;
  subscribeToEvents(handler: BlockchainEventHandler): () => void;
  getEventStatus?(): BlockchainEventStatus;
  subscribeToEventStatus?(handler: BlockchainEventStatusHandler): () => void;
}

const BlockchainClientContext = createContext<BlockchainClient | null>(null);
const ProviderSwitcherContext = createContext<{
  provider: string;
  setProvider: (provider: string) => void;
  availableProviders: string[];
  /** Whether the provider dropdown is shown in the header (toggled via command palette). */
  selectorVisible: boolean;
  setSelectorVisible: (visible: boolean) => void;
} | null>(null);
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

export function ProviderSwitcherProvider({
  children,
  provider,
  setProvider,
  availableProviders,
  selectorVisible,
  setSelectorVisible,
}: {
  children: ReactNode;
  provider: string;
  setProvider: (provider: string) => void;
  availableProviders: string[];
  selectorVisible: boolean;
  setSelectorVisible: (visible: boolean) => void;
}) {
  return (
    <ProviderSwitcherContext.Provider
      value={{ provider, setProvider, availableProviders, selectorVisible, setSelectorVisible }}
    >
      {children}
    </ProviderSwitcherContext.Provider>
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

export function useProviderInfo() {
  const client = useBlockchainClient();
  return client.metadata;
}

export function useProviderSwitcher() {
  const ctx = useContext(ProviderSwitcherContext);
  if (!ctx) {
    throw new Error('ProviderSwitcherProvider missing in component tree');
  }
  return ctx;
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

export function createDefaultBlockchainClient(options?: {
  turboEnabled?: boolean;
}): BlockchainClient {
  const provider = import.meta.env.VITE_BLOCKCHAIN_PROVIDER ?? 'libbitcoin';
  return createBlockchainClient(provider, options);
}

export function createBlockchainClient(
  provider: string,
  options?: { turboEnabled?: boolean }
): BlockchainClient {
  if (provider === 'blockstream') {
    const baseUrl = import.meta.env.VITE_BLOCKSTREAM_API_URL ?? 'https://blockstream.info/api';
    return createBlockstreamClient({ baseUrl });
  }
  if (provider === 'mempool') {
    const baseUrl = import.meta.env.VITE_MEMPOOL_API_URL ?? 'https://mempool.space/api';
    return createMempoolClient({ baseUrl });
  }
  if (provider === 'libbitcoin') {
    const baseUrl = resolveLibbitcoinBaseUrl();
    return createLibbitcoinClient({ baseUrl, turbo: options?.turboEnabled });
  }
  if (provider === 'external') {
    const baseUrl = import.meta.env.VITE_BLOCKCHAIN_API_URL;
    if (!baseUrl) {
      console.warn('No external API URL provided, falling back to libbitcoin provider');
      const fallbackBaseUrl = resolveLibbitcoinBaseUrl();
      return createLibbitcoinClient({ baseUrl: fallbackBaseUrl, turbo: options?.turboEnabled });
    }
    const client = createBlockstreamClient({ baseUrl });
    return {
      ...client,
      metadata: {
        ...client.metadata,
        id: 'external',
        label: 'External',
        description: 'Read-only adapter using a custom REST API endpoint.',
      },
    };
  }
  console.warn(`Unknown provider "${provider}", falling back to libbitcoin provider`);
  const fallbackBaseUrl = resolveLibbitcoinBaseUrl();
  return createLibbitcoinClient({ baseUrl: fallbackBaseUrl, turbo: options?.turboEnabled });
}

export function getAvailableProviders(): string[] {
  const providers = new Set<string>(['libbitcoin', 'blockstream', 'mempool']);
  if (import.meta.env.VITE_BLOCKCHAIN_API_URL) providers.add('external');
  return [...providers];
}
