import { z } from 'zod';

const nativeConfigurationSchema = z.object({
  store: z.object({
    address: z.boolean(),
    filter: z.boolean(),
    silent: z.number().int().nonnegative().nullable(),
    turbo: z.boolean(),
    pruned: z.number().int().nonnegative(),
  }),
  node: z.object({
    network: z.string(),
    limited: z.boolean().optional(),
    blocks: z.boolean().optional(),
    witness: z.boolean().optional(),
    filters: z.boolean().optional(),
    privacy: z.boolean().optional(),
  }),
  wallet: z
    .object({
      p2kh: z.number().int(),
      p2sh: z.number().int(),
      wif: z.number().int(),
      witness: z.string(),
      silent: z.string(),
      hd_private: z.number().int(),
      hd_public: z.number().int(),
    })
    .optional(),
  network: z
    .object({
      relay: z.boolean(),
      alert: z.boolean(),
      reject: z.boolean(),
      compact: z.boolean(),
      address: z.boolean(),
      witness_tx: z.boolean(),
      memory_pool: z.boolean(),
      not_found: z.boolean(),
      protocol_maximum: z.number().int(),
      protocol_minimum: z.number().int(),
    })
    .optional(),
  forks: z
    .record(z.string(), z.boolean())
    .transform((forks) => Object.keys(forks).filter((fork) => forks[fork])),
});

export type NativeConfiguration = z.infer<typeof nativeConfigurationSchema>;

const hashSchema = z.string().regex(/^[0-9a-f]{64}$/u);

const nativeHeaderSchema = z.object({
  hash: hashSchema,
  height: z.number().int().nonnegative(),
  timestamp: z.number().int().positive().max(0xffff_ffff),
});

export type NativeHeader = z.infer<typeof nativeHeaderSchema>;

const nativeTopSchema = z.object({
  hash: hashSchema,
  height: z.number().int().nonnegative(),
  header: z.object({
    timestamp: z.number().int().positive().max(0xffff_ffff),
  }),
});

const nativeTopsSchema = z.object({
  confirmed: nativeTopSchema,
  candidate: nativeTopSchema,
});

export type NativeTop = z.infer<typeof nativeTopSchema>;
export type NativeTops = z.infer<typeof nativeTopsSchema>;

// The candidate top header is carried by the tops.
export function topHeaderOf(tops: NativeTops): NativeHeader {
  const { hash, height, header } = tops.candidate;
  return { hash, height, timestamp: header.timestamp };
}

// Native /v1/fees returns existing pool gauges, not transaction arrival counters.
const poolIntegerSchema = z.number().int().nonnegative().max(Number.MAX_SAFE_INTEGER);
const nativeFeesSchema = z.object({
  count: poolIntegerSchema,
  virtual: poolIntegerSchema,
  fee: poolIntegerSchema,
  histogram: z.array(z.tuple([z.number().nonnegative(), poolIntegerSchema])),
});

export type NativeFees = z.infer<typeof nativeFeesSchema>;
export interface NativeFeesSnapshot {
  data: NativeFees;
  receivedAt: number;
}

export const NATIVE_FEES_PATH = '/v1/fees?format=json';
export const POOL_POLL_INTERVAL_MS = 5_000;
export const POOL_STALE_AFTER_MS = 15_000;

export interface NativeSnapshot {
  configuration: NativeConfiguration;
  tops: NativeTops;
  branch: NativeHeader | null;
  topHeader: NativeHeader | null;
  receivedAt: number;
}

export type NativeFrame =
  | { type: 'configuration'; payload: NativeConfiguration }
  | { type: 'header'; payload: NativeHeader }
  | { type: 'fees'; payload: NativeFees }
  | { type: 'top'; payload: NativeTops };

export function parseNativeFrame(text: string): NativeFrame | null {
  try {
    const value: unknown = JSON.parse(text) as unknown;
    const configuration = nativeConfigurationSchema.safeParse(value);
    if (configuration.success) {
      return { type: 'configuration', payload: configuration.data };
    }

    const header = nativeHeaderSchema.safeParse(value);
    if (header.success) {
      return { type: 'header', payload: header.data };
    }

    const fees = nativeFeesSchema.safeParse(value);
    if (fees.success) {
      return { type: 'fees', payload: fees.data };
    }

    const tops = nativeTopsSchema.safeParse(value);
    return tops.success ? { type: 'top', payload: tops.data } : null;
  } catch {
    return null;
  }
}

function endpointUrl(baseUrl: string, path: string): string {
  return new URL(path, `${baseUrl.replace(/\/+$/u, '')}/`).toString();
}

async function fetchJson(url: string, signal?: AbortSignal): Promise<unknown> {
  const response = await fetch(url, {
    headers: { Accept: 'application/json' },
    signal,
  });
  if (!response.ok) {
    throw new Error(`Native API request failed with status ${String(response.status)}`);
  }
  return (await response.json()) as unknown;
}

export async function fetchNativeSnapshot(
  baseUrl: string,
  signal?: AbortSignal
): Promise<NativeSnapshot> {
  const [configurationValue, topValue] = await Promise.all([
    fetchJson(endpointUrl(baseUrl, '/v1/configuration?format=json'), signal),
    fetchJson(endpointUrl(baseUrl, '/v1/top?format=json'), signal),
  ]);

  const configuration = nativeConfigurationSchema.parse(configurationValue);
  const tops = nativeTopsSchema.parse(topValue);
  const receivedAt = Date.now();
  let branch: NativeHeader | null = null;
  if (tops.candidate.height > tops.confirmed.height) {
    try {
      const path = nativeBranchPath(tops.confirmed.height);
      const header = nativeHeaderSchema.parse(await fetchJson(endpointUrl(baseUrl, path), signal));
      if (header.height === tops.confirmed.height) branch = header;
    } catch {
      // An unresolved branch must not hide otherwise valid tops.
      signal?.throwIfAborted();
    }
  }
  return { configuration, tops, branch, topHeader: topHeaderOf(tops), receivedAt };
}

// The candidate header at the given height, compared against the confirmed top.
export function nativeBranchPath(height: number): string {
  return `/v1/block/height/${String(height)}/header?format=json&candidate=true`;
}

export async function fetchNativeFees(
  baseUrl: string,
  signal?: AbortSignal
): Promise<NativeFeesSnapshot> {
  const data = nativeFeesSchema.parse(
    await fetchJson(endpointUrl(baseUrl, NATIVE_FEES_PATH), signal)
  );
  return { data, receivedAt: Date.now() };
}
