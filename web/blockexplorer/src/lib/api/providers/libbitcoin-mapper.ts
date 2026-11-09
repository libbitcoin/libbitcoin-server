import { bech32, bech32m, createBase58check } from '@scure/base';
import { sha256 } from '@noble/hashes/sha2.js';
import { bytesToHex } from '@noble/hashes/utils.js';
import { BlockSummary, TransactionInput, TransactionOutput, TransactionSummary } from '../client';

export interface BlockHeaderResponse {
  hash: string;
  version: number;
  previous: string;
  merkle_root: string;
  timestamp: number;
  bits: number;
  nonce: number;
  height: number;
}

export interface TxPoint {
  hash: string;
  index: number;
}

export interface TxInputResponse {
  point: TxPoint;
  script: string;
  witness?: string;
  sequence: number;
}

export interface TxOutputResponse {
  value: number;
  script: string;
}

export interface TxResponse {
  hash: string;
  version: number;
  inputs: TxInputResponse[];
  outputs: TxOutputResponse[];
  locktime: number;
}

export interface BlockResponse {
  header: BlockHeaderResponse;
  transactions: TxResponse[];
}

const base58c = createBase58check(sha256);
const MAX_TARGET = 0x00000000ffff0000000000000000000000000000000000000000000000000000n;

export type BitcoinNetwork = 'mainnet' | 'testnet' | 'regtest';

export function hexToBytes(hex: string): Uint8Array {
  if (hex.length % 2 !== 0) throw new Error('Invalid hex string');
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i += 1) {
    out[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  }
  return out;
}

export function bitsToDifficulty(bits?: number): number | undefined {
  if (bits === undefined) return undefined;
  const exponent = bits >>> 24;
  const mantissa = bits & 0x007fffff;
  if (mantissa === 0 || exponent < 3) return undefined;
  const target = BigInt(mantissa) * (1n << BigInt(8 * (exponent - 3)));
  if (target === 0n) return undefined;
  const ratio = Number(MAX_TARGET) / Number(target);
  return Number.isFinite(ratio) ? ratio : undefined;
}

function parseBracketedList(value?: string): string[] | undefined {
  if (!value) return undefined;
  const matches = [...value.matchAll(/\[([^\]]*)\]/g)].map((m) => m[1]);
  if (matches.length > 0) return matches;
  return [value];
}

function mapScriptType(script: string): string | undefined {
  const lower = script.toLowerCase();
  if (lower.includes('dup hash160') && lower.includes('equalverify checksig')) return 'p2pkh';
  if (lower.startsWith('hash160') && lower.includes('equal')) return 'p2sh';
  if (lower.startsWith('zero [')) {
    const match = lower.match(/zero \[([0-9a-f]+)\]/);
    const hex = match?.[1] ?? '';
    if (hex.length === 40) return 'p2wpkh';
    if (hex.length === 64) return 'p2wsh';
    return 'segwit';
  }
  if (lower.startsWith('1 [')) return 'p2tr';
  return undefined;
}

export function getNetwork(): BitcoinNetwork {
  const env = import.meta.env.VITE_BITCOIN_NETWORK;
  const normalized = env ? env.toLowerCase() : undefined;
  if (normalized === 'testnet' || normalized === 'test') return 'testnet';
  if (normalized === 'regtest' || normalized === 'reg') return 'regtest';
  return 'mainnet';
}

export function getNetworkParams(network: BitcoinNetwork) {
  if (network === 'testnet' || network === 'regtest') {
    return {
      p2pkh: 0x6f,
      p2sh: 0xc4,
      hrp: network === 'regtest' ? 'bcrt' : 'tb',
    };
  }
  return { p2pkh: 0x00, p2sh: 0x05, hrp: 'bc' };
}

function encodeBase58(version: number, hash: string) {
  const data = new Uint8Array(hash.length / 2 + 1);
  data[0] = version;
  data.set(hexToBytes(hash), 1);
  return base58c.encode(data);
}

function encodeSegwit(hrp: string, version: number, programHex: string) {
  const program = hexToBytes(programHex);
  const words = bech32.toWords(program);
  words.unshift(version);
  const encoder = version === 0 ? bech32 : bech32m;
  return encoder.encode(hrp, words);
}

function decodeScriptAddress(
  script: string,
  params: ReturnType<typeof getNetworkParams>
): { address?: string; type?: string } {
  const lower = script.toLowerCase();
  const p2pkh = lower.match(/dup hash160 \[([0-9a-f]{40})\] equalverify checksig/);
  if (p2pkh) return { address: encodeBase58(params.p2pkh, p2pkh[1]), type: 'p2pkh' };
  const p2sh = lower.match(/^hash160 \[([0-9a-f]{40})\] equal$/);
  if (p2sh) return { address: encodeBase58(params.p2sh, p2sh[1]), type: 'p2sh' };
  const p2wpkh = lower.match(/^zero \[([0-9a-f]{40})\]/);
  if (p2wpkh) return { address: encodeSegwit(params.hrp, 0, p2wpkh[1]), type: 'p2wpkh' };
  const p2wsh = lower.match(/^zero \[([0-9a-f]{64})\]/);
  if (p2wsh) return { address: encodeSegwit(params.hrp, 0, p2wsh[1]), type: 'p2wsh' };
  const p2tr = lower.match(/^1 \[([0-9a-f]{64})\]/);
  if (p2tr) return { address: encodeSegwit(params.hrp, 1, p2tr[1]), type: 'p2tr' };
  return { type: mapScriptType(script) };
}

function addressToScript(
  address: string,
  params: ReturnType<typeof getNetworkParams>
): { scriptHex: string; type: string } | null {
  // Base58
  try {
    const decoded = base58c.decode(address);
    const version = decoded[0];
    const hash = bytesToHex(decoded.slice(1));
    if (hash.length === 40 && version === params.p2pkh)
      return { scriptHex: `76a914${hash}88ac`, type: 'p2pkh' };
    if (hash.length === 40 && version === params.p2sh)
      return { scriptHex: `a914${hash}87`, type: 'p2sh' };
  } catch {
    // ignore
  }

  // Segwit (bech32 / bech32m)
  const candidate = address.toLowerCase();
  const bechIndex = candidate.indexOf('1');
  if (bechIndex > 0 && candidate.startsWith(`${params.hrp}1`)) {
    const bechCandidate = candidate as `${string}1${string}`;
    const decode = (decoder: typeof bech32) => {
      try {
        return decoder.decode(bechCandidate, 90);
      } catch {
        return null;
      }
    };

    const decodedV0 = decode(bech32);
    if (decodedV0 && decodedV0.prefix === params.hrp) {
      const version = decodedV0.words[0];
      const program = bech32.fromWords(decodedV0.words.slice(1));
      const progHex = bytesToHex(program);
      const pushLen = program.length.toString(16).padStart(2, '0');
      if (version === 0 && (program.length === 20 || program.length === 32)) {
        return {
          scriptHex: `00${pushLen}${progHex}`,
          type: program.length === 20 ? 'p2wpkh' : 'p2wsh',
        };
      }
    }

    const decodedV1 = decode(bech32m);
    if (decodedV1 && decodedV1.prefix === params.hrp) {
      const version = decodedV1.words[0];
      const program = bech32m.fromWords(decodedV1.words.slice(1));
      const progHex = bytesToHex(program);
      const pushLen = program.length.toString(16).padStart(2, '0');
      if (version === 1 && program.length === 32) {
        return { scriptHex: `51${pushLen}${progHex}`, type: 'p2tr' };
      }
    }
  }

  return null;
}

export function decodeAddressToScript(
  address: string,
  network: 'mainnet' | 'testnet' | 'regtest' = 'mainnet'
) {
  return addressToScript(address, getNetworkParams(network));
}

export function scriptHashFromAddress(
  address: string,
  params: ReturnType<typeof getNetworkParams>
) {
  const hexCandidate = address.toLowerCase();
  if (/^[0-9a-f]{64}$/.test(hexCandidate)) return { hash: hexCandidate, type: 'script-hash' };
  const script = addressToScript(address, params);
  if (!script) return null;
  const digest = sha256(hexToBytes(script.scriptHex));
  const reversed = Uint8Array.from(digest).reverse(); // libbitcoin expects reversed hash
  return { hash: bytesToHex(reversed), type: script.type };
}

export function toBlockSummary(header: BlockHeaderResponse, txCount?: number): BlockSummary {
  return {
    hash: header.hash,
    height: header.height,
    timestamp: header.timestamp,
    txCount,
    size: undefined,
    weight: undefined,
    difficulty: bitsToDifficulty(header.bits),
    previousBlockHash: header.previous,
  };
}

function parseWitness(value?: string) {
  const parsed = parseBracketedList(value);
  return parsed && parsed.length > 0 ? parsed : undefined;
}

function mapInput(
  input: TxInputResponse,
  prevout: TxOutputResponse | null | undefined,
  params: ReturnType<typeof getNetworkParams>,
  index: number
): TransactionInput {
  const isCoinbase = /^0{64}$/u.test(input.point.hash);
  const decoded = prevout
    ? decodeScriptAddress(prevout.script, params)
    : { address: undefined, type: undefined };
  const prevoutValue = prevout ? prevout.value : undefined;
  const prevoutScript = prevout ? prevout.script : undefined;
  return {
    index,
    prevTxId: isCoinbase ? undefined : input.point.hash,
    prevOutputIndex: isCoinbase ? undefined : input.point.index,
    address: decoded.address,
    valueSats: prevoutValue,
    scriptType: decoded.type ?? mapScriptType(prevoutScript ?? input.script),
    isCoinbase,
    script: input.script === '' ? { hex: '' } : { mnemonic: input.script },
    witness: parseWitness(input.witness),
    sequence: input.sequence,
  };
}

function mapOutput(
  output: TxOutputResponse,
  index: number,
  params: ReturnType<typeof getNetworkParams>
): TransactionOutput {
  const decoded = decodeScriptAddress(output.script, params);
  return {
    index,
    address: decoded.address,
    valueSats: output.value,
    scriptType: decoded.type ?? mapScriptType(output.script),
    spent: undefined,
    script: output.script === '' ? { hex: '' } : { mnemonic: output.script },
  };
}

export function computeConfirmations(tip?: number, blockHeight?: number) {
  if (typeof tip !== 'number' || typeof blockHeight !== 'number') return 0;
  return Math.max(0, tip - blockHeight + 1);
}

export function mapTransaction(
  tx: TxResponse,
  prevouts: (TxOutputResponse | null | undefined)[],
  params: ReturnType<typeof getNetworkParams>,
  context: {
    blockHeight?: number;
    blockHash?: string;
    tipHeight?: number;
    timestamp?: number;
  },
  feeSats?: number
): TransactionSummary {
  const outputs = tx.outputs.map((o, idx) => mapOutput(o, idx, params));
  const totalOutput = outputs.reduce((sum, o) => sum + o.valueSats, 0);
  const totalInput = prevouts.reduce<number | null>((sum, prevout) => {
    if (!prevout) return sum;
    return (sum ?? 0) + prevout.value;
  }, null);
  const confirmations = computeConfirmations(context.tipHeight, context.blockHeight);
  const status = typeof context.blockHeight === 'number' ? 'confirmed' : 'unconfirmed';

  return {
    txid: tx.hash,
    hash: tx.hash,
    status,
    confirmations,
    blockHeight: context.blockHeight,
    blockHash: context.blockHash,
    receivedTime: context.timestamp,
    feeSats: Number.isFinite(feeSats) ? feeSats : undefined,
    feeRate: null,
    size: undefined,
    weight: undefined,
    version: tx.version,
    locktime: tx.locktime,
    totalInput,
    totalOutput,
    inputs: tx.inputs.map((input, idx) => mapInput(input, prevouts[idx], params, idx)),
    outputs,
  };
}
