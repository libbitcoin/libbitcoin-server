import { z } from 'zod';
import type {
  AddressDetails,
  AddressActivityItem,
  AddressSummary,
  AddressUtxo,
  BlockDetail,
  BlockHeaderDetail,
  BlockValidation,
  BlockStats,
  BlockSummary,
  BlockchainClient,
  BlockchainEventHandler,
  TransactionSummary,
} from '../client';
import { inferAddressType } from '@/lib/utils/address';

interface BlockstreamClientOptions {
  baseUrl: string;
}

interface BlockstreamState {
  lastBlockHash: string | null;
  lastKnownHeight: number | null;
}

class ApiError extends Error {
  constructor(
    message: string,
    public status: number
  ) {
    super(message);
  }
}

const blockSchema = z
  .object({
    id: z.string(),
    height: z.number(),
    timestamp: z.number().optional(),
    mediantime: z.number().optional(),
    tx_count: z.number().optional(),
    size: z.number().optional(),
    weight: z.number().optional(),
    difficulty: z.number().optional(),
    bits: z.number().optional(),
    merkle_root: z.string().optional(),
    previousblockhash: z.string().nullable().optional(),
    nonce: z.number().optional(),
    version: z.number().optional(),
  })
  .loose();

const statusSchema = z.object({
  confirmed: z.boolean(),
  block_height: z.number().optional(),
  block_hash: z.string().optional(),
  block_time: z.number().optional(),
});

const vinSchema = z
  .object({
    txid: z.string().optional().nullable(),
    vout: z.number().optional().nullable(),
    sequence: z.number().optional(),
    scriptsig: z.string().optional().nullable(),
    scriptsig_asm: z.string().optional().nullable(),
    witness: z.array(z.string()).optional(),
    is_coinbase: z.boolean().optional(),
    prevout: z
      .object({
        scriptpubkey_type: z.string().optional(),
        scriptpubkey_address: z.string().optional(),
        scriptpubkey: z.string().optional(),
        scriptpubkey_asm: z.string().optional(),
        value: z.number().optional(),
      })
      .partial()
      .nullable()
      .optional(),
  })
  .loose();

const voutSchema = z
  .object({
    scriptpubkey_type: z.string().optional(),
    scriptpubkey_address: z.string().optional(),
    scriptpubkey: z.string().optional(),
    scriptpubkey_asm: z.string().optional(),
    value: z.number(),
    n: z.number().optional(),
  })
  .loose();

const transactionSchema = z
  .object({
    txid: z.string(),
    hash: z.string().optional(),
    fee: z.number().optional(),
    version: z.number(),
    locktime: z.number(),
    vin: z.array(vinSchema),
    vout: z.array(voutSchema),
    size: z.number(),
    weight: z.number(),
    status: statusSchema,
  })
  .loose();

const blockArraySchema = z.array(blockSchema);
const transactionArraySchema = z.array(transactionSchema);

const blockStatusSchema = z.object({
  in_best_chain: z.boolean(),
  next_best: z.string().optional(),
  height: z.number().optional(),
});

const blockValidationHeaderSchema = z.object({
  id: z.string().regex(/^[0-9a-f]{64}$/iu),
  mediantime: z.number().int().nonnegative().max(0xffffffff).optional(),
});

const addressStatsSchema = z.object({
  address: z.string(),
  chain_stats: z.object({
    funded_txo_count: z.number().optional(),
    funded_txo_sum: z.number().optional(),
    spent_txo_count: z.number().optional(),
    spent_txo_sum: z.number().optional(),
    tx_count: z.number().optional(),
  }),
  mempool_stats: z.object({
    funded_txo_count: z.number().optional(),
    funded_txo_sum: z.number().optional(),
    spent_txo_count: z.number().optional(),
    spent_txo_sum: z.number().optional(),
    tx_count: z.number().optional(),
  }),
});

const addressUtxoSchema = z
  .object({
    txid: z.string(),
    vout: z.number().int().nonnegative(),
    value: z.number().nonnegative(),
    status: statusSchema,
  })
  .loose();

const addressUtxoArraySchema = z.array(addressUtxoSchema);

const outputSpendSchema = z.discriminatedUnion('spent', [
  z.object({ spent: z.literal(false) }),
  z.object({
    spent: z.literal(true),
    txid: z.string().regex(/^[0-9a-f]{64}$/iu),
    vin: z.number().int().nonnegative().max(0xffffffff),
  }),
]);

async function request<T>(baseUrl: string, path: string, schema: z.ZodType<T>): Promise<T> {
  const normalizedPath = path.startsWith('/') ? path : `/${path}`;
  const response = await fetch(`${baseUrl}${normalizedPath}`);
  if (!response.ok) {
    throw new ApiError(
      `Request to ${path} failed with status ${String(response.status)}`,
      response.status
    );
  }
  const json: unknown = await response.json();
  return schema.parse(json);
}

async function fetchBlockPages(
  baseUrl: string,
  initialPath: string,
  limit: number
): Promise<Array<z.infer<typeof blockSchema>>> {
  if (limit <= 0) return [];

  const blocks: Array<z.infer<typeof blockSchema>> = [];
  let path: string | null = initialPath;

  while (path && blocks.length < limit) {
    const page: Array<z.infer<typeof blockSchema>> = await request(baseUrl, path, blockArraySchema);
    if (page.length === 0) break;

    blocks.push(...page);

    const lastBlock: z.infer<typeof blockSchema> | undefined = page.at(-1);
    if (!lastBlock || lastBlock.height <= 0) break;
    path = `/blocks/${String(lastBlock.height - 1)}`;
  }

  return blocks.slice(0, limit);
}

async function fetchTipHeight(baseUrl: string, state: BlockstreamState): Promise<number> {
  const response = await fetch(`${baseUrl}/blocks/tip/height`);
  if (!response.ok) {
    throw new ApiError('Unable to fetch tip height', response.status);
  }
  const text = await response.text();
  const height = Number(text.trim());
  if (!Number.isFinite(height)) {
    throw new TypeError('Invalid tip height response');
  }
  state.lastKnownHeight = height;
  return height;
}

async function ensureTipHeight(baseUrl: string, state: BlockstreamState, minimumHeight?: number) {
  if (state.lastKnownHeight === null) {
    await fetchTipHeight(baseUrl, state);
    return;
  }
  if (typeof minimumHeight === 'number' && state.lastKnownHeight < minimumHeight) {
    await fetchTipHeight(baseUrl, state);
  }
}

function computeAddressDelta(address: string, tx: z.infer<typeof transactionSchema>): number {
  const received = tx.vout.reduce((total, output) => {
    if (output.scriptpubkey_address === address) {
      return total + output.value;
    }
    return total;
  }, 0);

  const spent = tx.vin.reduce((total, input) => {
    const prevout = input.prevout;
    if (prevout?.scriptpubkey_address === address && typeof prevout.value === 'number') {
      return total + prevout.value;
    }
    return total;
  }, 0);

  return received - spent;
}

async function resolveBlockHash(baseUrl: string, hashOrHeight: string): Promise<string> {
  if (hashOrHeight === 'latest' || hashOrHeight === 'tip') {
    const blocks = await request(baseUrl, '/blocks', blockArraySchema);
    if (blocks.length === 0) {
      throw new Error('No blocks returned from API');
    }
    return blocks[0].id;
  }
  if (/^\d+$/.test(hashOrHeight)) {
    const response = await fetch(`${baseUrl}/block-height/${hashOrHeight}`);
    if (!response.ok) {
      throw new ApiError('Unable to resolve block height', response.status);
    }
    const text = await response.text();
    return text.trim();
  }
  return hashOrHeight;
}

async function fetchNextBlockHash(
  baseUrl: string,
  currentHeight: number,
  state: BlockstreamState
): Promise<string | null> {
  if (state.lastKnownHeight == null || currentHeight >= state.lastKnownHeight) {
    return null;
  }
  const nextHeight = currentHeight + 1;
  try {
    const response = await fetch(`${baseUrl}/block-height/${String(nextHeight)}`);
    if (!response.ok) {
      return null;
    }
    const text = await response.text();
    const nextHash = text.trim();
    return nextHash || null;
  } catch (error) {
    console.warn(`Failed to resolve hash for height ${String(nextHeight)}`, error);
    return null;
  }
}

function mapBlockSummary(block: z.infer<typeof blockSchema>): BlockSummary {
  return {
    hash: block.id,
    height: block.height,
    timestamp: block.timestamp ?? block.mediantime ?? 0,
    txCount: block.tx_count ?? 0,
    size: block.size ?? 0,
    weight: block.weight ?? 0,
    difficulty: block.difficulty,
    previousBlockHash: block.previousblockhash ?? null,
  };
}

function mapTransaction(
  tx: z.infer<typeof transactionSchema>,
  state: BlockstreamState
): TransactionSummary {
  const status = tx.status.confirmed ? 'confirmed' : 'unconfirmed';
  const inputs = tx.vin.map((input, index) => {
    const prevOutputIndex =
      input.is_coinbase || input.txid == null
        ? undefined
        : typeof input.vout === 'number' && input.vout !== 0xffffffff
          ? input.vout
          : undefined;
    return {
      index,
      prevTxId: input.txid ?? undefined,
      prevOutputIndex,
      address: input.prevout?.scriptpubkey_address ?? undefined,
      valueSats: input.prevout?.value,
      scriptType: input.prevout?.scriptpubkey_type,
      isCoinbase: input.is_coinbase ?? false,
      script:
        typeof input.scriptsig === 'string' || input.scriptsig_asm
          ? {
              hex: input.scriptsig ?? undefined,
              mnemonic: input.scriptsig_asm ?? undefined,
            }
          : undefined,
      witness: input.witness ?? undefined,
      sequence: input.sequence,
    };
  });
  const outputs = tx.vout.map((output, index) => ({
    index,
    address: output.scriptpubkey_address ?? undefined,
    valueSats: output.value,
    scriptType: output.scriptpubkey_type,
    spent: undefined,
    script:
      typeof output.scriptpubkey === 'string' || output.scriptpubkey_asm
        ? {
            hex: output.scriptpubkey ?? undefined,
            mnemonic: output.scriptpubkey_asm ?? undefined,
          }
        : undefined,
  }));
  const totalOutput = outputs.reduce((total, output) => total + output.valueSats, 0);
  const tipHeight = state.lastKnownHeight;
  let confirmations = 0;
  if (tx.status.block_height && status === 'confirmed') {
    confirmations =
      tipHeight && tipHeight >= tx.status.block_height ? tipHeight - tx.status.block_height + 1 : 1;
  }
  return {
    txid: tx.txid,
    hash: tx.hash ?? tx.txid,
    status,
    confirmations,
    blockHeight: tx.status.block_height,
    blockHash: tx.status.block_hash,
    receivedTime: tx.status.block_time,
    feeSats: tx.fee ?? null,
    feeRate: tx.fee ? (tx.fee / tx.weight) * 4 : null,
    size: tx.size,
    weight: tx.weight,
    version: tx.version,
    locktime: tx.locktime,
    totalInput: null,
    totalOutput,
    inputs,
    outputs,
  };
}

function mapAddress(
  stats: z.infer<typeof addressStatsSchema>,
  transactions: Array<z.infer<typeof transactionSchema>>
): AddressDetails {
  const chain = stats.chain_stats;
  const mempool = stats.mempool_stats;
  const balance = (chain.funded_txo_sum ?? 0) - (chain.spent_txo_sum ?? 0);
  const txs = mapAddressActivity(stats.address, transactions.slice(0, 25));
  return {
    address: stats.address,
    addressType: inferAddressType(stats.address),
    balance,
    totalReceived: (chain.funded_txo_sum ?? 0) + (mempool.funded_txo_sum ?? 0),
    totalSent: (chain.spent_txo_sum ?? 0) + (mempool.spent_txo_sum ?? 0),
    txCount: (chain.tx_count ?? 0) + (mempool.tx_count ?? 0),
    utxoCount: (chain.funded_txo_count ?? 0) - (chain.spent_txo_count ?? 0),
    transactions: txs,
  };
}

function mapAddressSummary(stats: z.infer<typeof addressStatsSchema>): AddressSummary {
  const chain = stats.chain_stats;
  const mempool = stats.mempool_stats;
  const balance = (chain.funded_txo_sum ?? 0) - (chain.spent_txo_sum ?? 0);
  return {
    address: stats.address,
    addressType: inferAddressType(stats.address),
    balance,
    totalReceived: (chain.funded_txo_sum ?? 0) + (mempool.funded_txo_sum ?? 0),
    totalSent: (chain.spent_txo_sum ?? 0) + (mempool.spent_txo_sum ?? 0),
    txCount: (chain.tx_count ?? 0) + (mempool.tx_count ?? 0),
    utxoCount: (chain.funded_txo_count ?? 0) - (chain.spent_txo_count ?? 0),
  };
}

function mapAddressActivity(
  address: string,
  transactions: Array<z.infer<typeof transactionSchema>>
): AddressActivityItem[] {
  return transactions.map((tx) => ({
    txid: tx.txid,
    timestamp: tx.status.block_time,
    valueChange: computeAddressDelta(address, tx),
    blockHeight: tx.status.block_height,
  }));
}

async function fetchBlockTransactions(baseUrl: string, hash: string, startIndex = 0) {
  const path = startIndex > 0 ? `/block/${hash}/txs/${String(startIndex)}` : `/block/${hash}/txs`;
  return request(baseUrl, path, transactionArraySchema);
}

async function fetchAddressTransactions(baseUrl: string, address: string, lastSeenTxid?: string) {
  if (lastSeenTxid) {
    return request(
      baseUrl,
      `/address/${address}/txs/chain/${lastSeenTxid}`,
      transactionArraySchema
    );
  }
  return request(baseUrl, `/address/${address}/txs`, transactionArraySchema);
}

export function createBlockstreamClient({ baseUrl }: BlockstreamClientOptions): BlockchainClient {
  const listeners = new Set<BlockchainEventHandler>();
  let poller: ReturnType<typeof setInterval> | null = null;
  const state: BlockstreamState = {
    lastBlockHash: null,
    lastKnownHeight: null,
  };

  async function fetchTransactionSummary(txid: string): Promise<TransactionSummary> {
    const tx = await request(baseUrl, `/tx/${txid}`, transactionSchema);
    await (tx.status.block_height
      ? ensureTipHeight(baseUrl, state, tx.status.block_height)
      : ensureTipHeight(baseUrl, state));
    return mapTransaction(tx, state);
  }

  function ensurePolling() {
    if (poller || listeners.size === 0) {
      return;
    }
    poller = setInterval(async () => {
      try {
        const blocks = await request(baseUrl, '/blocks', blockArraySchema);
        if (blocks.length === 0) return;
        const latest = blocks[0];
        if (latest.id !== state.lastBlockHash) {
          state.lastBlockHash = latest.id;
          state.lastKnownHeight = Math.max(latest.height, state.lastKnownHeight ?? 0);
          const summary = mapBlockSummary(latest);
          for (const listener of listeners) {
            listener({ type: 'block.new', payload: summary });
          }
        }
      } catch (error) {
        console.warn('Block polling failed', error);
      }
    }, 15_000);
  }

  function stopPolling() {
    if (poller) {
      clearInterval(poller);
      poller = null;
    }
  }

  return {
    metadata: {
      id: 'blockstream',
      label: 'Blockstream.info',
      type: 'external',
      connectionState: 'connected',
      supportsWebsocket: false,
      endpoint: baseUrl,
      description: 'Read-only adapter using Blockstream public REST API.',
    },
    getConfiguration() {
      return Promise.resolve({
        address: true,
        filter: false,
        turbo: false,
        witness: true,
      });
    },
    async getLatestBlocks(limit = 10): Promise<BlockSummary[]> {
      const blocks = await fetchBlockPages(baseUrl, '/blocks', limit);
      if (blocks.length > 0) {
        state.lastBlockHash = blocks[0].id;
        state.lastKnownHeight = Math.max(blocks[0].height, state.lastKnownHeight ?? 0);
      }
      return blocks.map((block) => mapBlockSummary(block));
    },
    async getBlocksBefore(height: number, limit = 10): Promise<BlockSummary[]> {
      const startHeight = Math.max(height, 0);
      const blocks = await fetchBlockPages(baseUrl, `/blocks/${String(startHeight)}`, limit);
      if (blocks.length > 0) {
        state.lastKnownHeight = Math.max(blocks[0].height, state.lastKnownHeight ?? 0);
      }
      return blocks.map((block) => mapBlockSummary(block));
    },
    async getBlockHeader(hashOrHeight: string): Promise<BlockHeaderDetail> {
      const hash = await resolveBlockHash(baseUrl, hashOrHeight);
      await ensureTipHeight(baseUrl, state);
      const [block, status] = await Promise.all([
        request(baseUrl, `/block/${hash}`, blockSchema),
        request(baseUrl, `/block/${hash}/status`, blockStatusSchema).catch(() => null),
      ]);

      const summary = mapBlockSummary(block);
      state.lastBlockHash = summary.hash;
      state.lastKnownHeight = Math.max(summary.height, state.lastKnownHeight ?? summary.height);

      let nextBlockHash = status?.next_best ?? null;
      if (!nextBlockHash) {
        nextBlockHash = await fetchNextBlockHash(baseUrl, summary.height, state);
      }

      return {
        ...summary,
        merkleRoot: block.merkle_root,
        nonce: block.nonce,
        bits: block.bits,
        version: block.version,
        nextBlockHash,
      };
    },
    async getBlockValidation(hash: string): Promise<BlockValidation> {
      const [block, status] = await Promise.all([
        request(baseUrl, `/block/${hash}`, blockValidationHeaderSchema),
        request(baseUrl, `/block/${hash}/status`, blockStatusSchema),
      ]);
      if (block.id.toLowerCase() !== hash.toLowerCase()) {
        throw new Error('Block validation response does not match the requested block');
      }
      return { medianTimePast: block.mediantime, inBestChain: status.in_best_chain };
    },
    getBlockStats(): Promise<BlockStats> {
      return Promise.resolve({});
    },
    async getBlock(hashOrHeight: string): Promise<BlockDetail> {
      const hash = await resolveBlockHash(baseUrl, hashOrHeight);
      await ensureTipHeight(baseUrl, state);
      const block = await request(baseUrl, `/block/${hash}`, blockSchema);
      const [transactionsResult, statusResult] = await Promise.allSettled([
        fetchBlockTransactions(baseUrl, hash),
        request(baseUrl, `/block/${hash}/status`, blockStatusSchema),
      ]);

      if (transactionsResult.status === 'rejected') {
        console.warn(
          'Failed to load block transactions, defaulting to empty set',
          transactionsResult.reason
        );
      }
      if (statusResult.status === 'rejected') {
        console.warn('Failed to load block status, defaulting to unknown', statusResult.reason);
      }

      const summary = mapBlockSummary(block);
      state.lastBlockHash = summary.hash;
      state.lastKnownHeight = Math.max(summary.height, state.lastKnownHeight ?? summary.height);

      const transactions =
        transactionsResult.status === 'fulfilled' ? transactionsResult.value : [];
      const status = statusResult.status === 'fulfilled' ? statusResult.value : null;

      let nextBlockHash = status?.next_best ?? null;
      if (!nextBlockHash) {
        nextBlockHash = await fetchNextBlockHash(baseUrl, summary.height, state);
      }

      const detail: BlockDetail = {
        ...summary,
        merkleRoot: block.merkle_root,
        nonce: block.nonce,
        bits: block.bits,
        version: block.version,
        nextBlockHash,
        transactions: transactions.map((tx) => mapTransaction(tx, state)),
      };
      return detail;
    },
    async getBlockTransactions(
      hashOrHeight: string,
      startIndex = 0
    ): Promise<TransactionSummary[]> {
      const hash = await resolveBlockHash(baseUrl, hashOrHeight);
      const transactions = await fetchBlockTransactions(baseUrl, hash, startIndex);
      const highestHeight = transactions.reduce<number | null>((max, tx) => {
        if (tx.status.block_height) {
          return max === null ? tx.status.block_height : Math.max(max, tx.status.block_height);
        }
        return max;
      }, null);
      await (typeof highestHeight === 'number'
        ? ensureTipHeight(baseUrl, state, highestHeight)
        : ensureTipHeight(baseUrl, state));
      return transactions.map((tx) => mapTransaction(tx, state));
    },
    async getTransactionHeader(txid: string): Promise<TransactionSummary> {
      return fetchTransactionSummary(txid);
    },
    async getTransaction(txid: string): Promise<TransactionSummary> {
      return fetchTransactionSummary(txid);
    },
    async getTransactionInputScriptHex(txid: string, inputIndex: number): Promise<string> {
      const tx = await request(baseUrl, `/tx/${txid}`, transactionSchema);
      if (inputIndex < 0 || inputIndex >= tx.vin.length) {
        throw new RangeError(`Input index ${String(inputIndex)} not found`);
      }
      const input = tx.vin[inputIndex];
      const scriptsig = input.scriptsig;
      return typeof scriptsig === 'string' ? scriptsig : '';
    },
    async getTransactionOutputScriptHex(txid: string, outputIndex: number): Promise<string> {
      const tx = await request(baseUrl, `/tx/${txid}`, transactionSchema);
      if (outputIndex < 0 || outputIndex >= tx.vout.length) {
        throw new RangeError(`Output index ${String(outputIndex)} not found`);
      }
      const output = tx.vout[outputIndex];
      const scriptpubkey = output.scriptpubkey;
      if (typeof scriptpubkey !== 'string') {
        throw new TypeError('Output script missing in provider response');
      }
      return scriptpubkey;
    },
    async getTransactionOutputSpends(txid, outputIndices) {
      if (outputIndices.length === 0) return [];
      const spends = await request(baseUrl, `/tx/${txid}/outspends`, z.array(z.unknown()));
      return outputIndices.map((outputIndex) => {
        const result = outputSpendSchema.safeParse(spends[outputIndex]);
        if (!result.success) return { outputIndex, status: 'unknown' as const };
        if (!result.data.spent) return { outputIndex, status: 'unspent' as const };
        return {
          outputIndex,
          status: 'spent' as const,
          txid: result.data.txid.toLowerCase(),
          inputIndex: result.data.vin,
        };
      });
    },
    async getAddressSummary(address: string): Promise<AddressSummary> {
      const stats = await request(baseUrl, `/address/${address}`, addressStatsSchema);
      return mapAddressSummary(stats);
    },
    async getAddressUtxos(address: string): Promise<AddressUtxo[]> {
      const utxos = await request(baseUrl, `/address/${address}/utxo`, addressUtxoArraySchema);
      return utxos.map((utxo) => ({
        txid: utxo.txid,
        outputIndex: utxo.vout,
        valueSats: utxo.value,
        status: utxo.status.confirmed ? 'confirmed' : 'unconfirmed',
        blockHeight: utxo.status.block_height,
        blockHash: utxo.status.block_hash,
        timestamp: utxo.status.block_time,
      }));
    },
    async getAddress(address: string): Promise<AddressDetails> {
      const stats = await request(baseUrl, `/address/${address}`, addressStatsSchema);
      const txs = await request(baseUrl, `/address/${address}/txs`, transactionArraySchema);
      return mapAddress(stats, txs);
    },
    async getAddressTransactions(
      address: string,
      lastSeenTxid?: string
    ): Promise<AddressActivityItem[]> {
      const txs = await fetchAddressTransactions(baseUrl, address, lastSeenTxid);
      return mapAddressActivity(address, txs);
    },
    subscribeToEvents(handler: BlockchainEventHandler): () => void {
      listeners.add(handler);
      ensurePolling();
      return () => {
        listeners.delete(handler);
        if (listeners.size === 0) {
          stopPolling();
        }
      };
    },
  };
}
