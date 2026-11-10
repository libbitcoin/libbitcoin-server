import { z } from 'zod';

const nativeConfigurationSchema = z.object({
  address: z.boolean(),
  filter: z.boolean(),
  turbo: z.boolean(),
  witness: z.boolean(),
  retarget: z.boolean(),
  difficult: z.boolean(),
  pruned: z.number().int().nonnegative(),
});

export type NativeConfiguration = z.infer<typeof nativeConfigurationSchema>;

const nativeHeaderSchema = z.object({
  height: z.number().int().nonnegative(),
  timestamp: z.number().int().positive().max(0xffff_ffff),
});

export type NativeHeader = z.infer<typeof nativeHeaderSchema>;

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
  top: number;
  topHeader: NativeHeader | null;
  receivedAt: number;
}

export type NativeFrame =
  | { type: 'configuration'; payload: NativeConfiguration }
  | { type: 'header'; payload: NativeHeader }
  | { type: 'fees'; payload: NativeFees }
  | { type: 'top'; payload: number };

const nativeTopSchema = z.number().int().nonnegative();

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

    const top = nativeTopSchema.safeParse(value);
    return top.success ? { type: 'top', payload: top.data } : null;
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
  const top = nativeTopSchema.parse(topValue);
  const receivedAt = Date.now();
  let topHeader: NativeHeader | null = null;
  try {
    const header = nativeHeaderSchema.parse(
      await fetchJson(endpointUrl(baseUrl, nativeHeaderPath(top)), signal)
    );
    if (header.height === top) topHeader = header;
  } catch {
    // A missing timestamp must not hide an otherwise valid canonical top.
    signal?.throwIfAborted();
  }
  return { configuration, top, topHeader, receivedAt };
}

export function nativeHeaderPath(height: number): string {
  return `/v1/block/height/${String(height)}/header?format=json`;
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
