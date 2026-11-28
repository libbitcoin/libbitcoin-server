import { z } from 'zod';

interface LogLevel {
  id: number;
  name: string;
  label: string;
  disabled?: boolean;
}

// Disabled levels are shown but never requested from the server.
export const LOG_LEVELS: readonly LogLevel[] = [
  { id: 0, name: 'application', label: 'Application' },
  { id: 1, name: 'news', label: 'News' },
  { id: 2, name: 'session', label: 'Session' },
  { id: 3, name: 'protocol', label: 'Protocol' },
  { id: 4, name: 'proxy', label: 'Proxy', disabled: true },
  { id: 5, name: 'remote', label: 'Remote' },
  { id: 6, name: 'fault', label: 'Fault' },
  { id: 7, name: 'quitting', label: 'Quitting' },
  { id: 9, name: 'verbose', label: 'Verbose', disabled: true },
];

export type EventCategory =
  | 'headers'
  | 'blocks'
  | 'transactions'
  | 'chain'
  | 'mining'
  | 'timing'
  | 'pool'
  | 'peers';
export type EventValueKind =
  'height' | 'count' | 'fee-rate' | 'seconds' | 'milliseconds' | 'microseconds' | 'unknown';

interface EventDefinition {
  id: number;
  name: string;
  label: string;
  category: EventCategory;
  valueKind: EventValueKind;
}

export const EVENT_DEFINITIONS = [
  {
    id: 0,
    name: 'header_archived',
    label: 'Header archived',
    category: 'headers',
    valueKind: 'height',
  },
  {
    id: 1,
    name: 'header_organized',
    label: 'Header organized',
    category: 'headers',
    valueKind: 'height',
  },
  {
    id: 2,
    name: 'header_reorganized',
    label: 'Header reorganized',
    category: 'headers',
    valueKind: 'height',
  },
  {
    id: 3,
    name: 'block_archived',
    label: 'Block archived',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 4,
    name: 'block_buffered',
    label: 'Block buffered',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 5,
    name: 'block_validated',
    label: 'Block validated',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 6,
    name: 'block_confirmed',
    label: 'Block confirmed',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 7,
    name: 'block_unconfirmable',
    label: 'Block unconfirmable',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 8,
    name: 'validate_bypassed',
    label: 'Validation bypassed',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 9,
    name: 'confirm_bypassed',
    label: 'Confirmation bypassed',
    category: 'blocks',
    valueKind: 'height',
  },
  {
    id: 10,
    name: 'tx_archived',
    label: 'Transaction archived',
    category: 'transactions',
    valueKind: 'fee-rate',
  },
  {
    id: 11,
    name: 'tx_validated',
    label: 'Transaction validated',
    category: 'transactions',
    valueKind: 'count',
  },
  {
    id: 12,
    name: 'tx_invalidated',
    label: 'Transaction invalidated',
    category: 'transactions',
    valueKind: 'count',
  },
  {
    id: 13,
    name: 'block_organized',
    label: 'Block organized',
    category: 'chain',
    valueKind: 'height',
  },
  {
    id: 14,
    name: 'block_reorganized',
    label: 'Block reorganized',
    category: 'chain',
    valueKind: 'height',
  },
  {
    id: 15,
    name: 'template_issued',
    label: 'Mining template issued',
    category: 'mining',
    valueKind: 'count',
  },
  {
    id: 16,
    name: 'snapshot_secs',
    label: 'Snapshot',
    category: 'timing',
    valueKind: 'seconds',
  },
  {
    id: 17,
    name: 'prune_msecs',
    label: 'Prune',
    category: 'timing',
    valueKind: 'milliseconds',
  },
  {
    id: 18,
    name: 'reload_msecs',
    label: 'Store reload',
    category: 'timing',
    valueKind: 'milliseconds',
  },
  {
    id: 19,
    name: 'block_usecs',
    label: 'Serve block',
    category: 'timing',
    valueKind: 'microseconds',
  },
  {
    id: 20,
    name: 'ancestry_msecs',
    label: 'Serve ancestry',
    category: 'timing',
    valueKind: 'milliseconds',
  },
  {
    id: 21,
    name: 'filter_msecs',
    label: 'Serve filter',
    category: 'timing',
    valueKind: 'milliseconds',
  },
  {
    id: 22,
    name: 'filterhashes_msecs',
    label: 'Serve filter hashes',
    category: 'timing',
    valueKind: 'milliseconds',
  },
  {
    id: 23,
    name: 'filterchecks_msecs',
    label: 'Serve filter checkpoints',
    category: 'timing',
    valueKind: 'milliseconds',
  },
  {
    id: 24,
    name: 'ecdsa_secs',
    label: 'ECDSA batch verification',
    category: 'timing',
    valueKind: 'seconds',
  },
  {
    id: 25,
    name: 'schnorr_secs',
    label: 'Schnorr batch verification',
    category: 'timing',
    valueKind: 'seconds',
  },
  {
    id: 26,
    name: 'silent_secs',
    label: 'Silent payment scan',
    category: 'timing',
    valueKind: 'seconds',
  },
  {
    id: 27,
    name: 'validate_usecs',
    label: 'Block validation',
    category: 'timing',
    valueKind: 'microseconds',
  },
  {
    id: 28,
    name: 'top_usecs',
    label: 'Header to top',
    category: 'timing',
    valueKind: 'microseconds',
  },
  {
    id: 29,
    name: 'pool_rows',
    label: 'Pool rows',
    category: 'pool',
    valueKind: 'count',
  },
  {
    id: 30,
    name: 'pool_submit_usecs',
    label: 'Pool submit',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 31,
    name: 'pool_coverage_usecs',
    label: 'Pool coverage',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 32,
    name: 'pool_connect_usecs',
    label: 'Pool script connect',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 33,
    name: 'pool_admit_usecs',
    label: 'Pool admission',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 34,
    name: 'pool_compact_usecs',
    label: 'Pool compact block fill',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 35,
    name: 'pool_block_in_usecs',
    label: 'Pool organized block',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 36,
    name: 'pool_block_out_usecs',
    label: 'Pool reorganized block',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 37,
    name: 'pool_context_usecs',
    label: 'Pool context sweep',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 38,
    name: 'pool_template_usecs',
    label: 'Pool template',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 39,
    name: 'pool_project_usecs',
    label: 'Pool fee projection',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 40,
    name: 'pool_list_usecs',
    label: 'Pool listing',
    category: 'pool',
    valueKind: 'microseconds',
  },
  {
    id: 41,
    name: 'tx_dropped',
    label: 'Peer dropped for transactions',
    category: 'peers',
    valueKind: 'count',
  },
  {
    id: 42,
    name: 'tx_refused',
    label: 'Transactions refused, peer kept',
    category: 'peers',
    valueKind: 'count',
  },
  {
    id: 43,
    name: 'tx_unserved',
    label: 'Transactions not served by peer',
    category: 'peers',
    valueKind: 'count',
  },
  { id: 44, name: 'unknown', label: 'Unknown', category: 'chain', valueKind: 'unknown' },
] as const satisfies readonly EventDefinition[];

// JavaScript Date supports at most 100,000,000 days from the Unix epoch.
const adminTimestampSchema = z.number().int().nonnegative().max(8_640_000_000_000_000);

const adminLogFrameSchema = z.object({
  level: z.number().int().nonnegative(),
  time: adminTimestampSchema,
  message: z.string(),
});

const adminEventFrameSchema = z.object({
  event: z.number().int().nonnegative(),
  value: z.number().nonnegative(),
  time: adminTimestampSchema,
});

const adminAckFrameSchema = z.object({
  previous: z.number().nonnegative(),
});

const miningCountSchema = z.number().int().nonnegative();

const miningShareSchema = z.object({
  time: adminTimestampSchema,
  height: miningCountSchema,
  difficulty: z.number().nonnegative(),
  result: z.string(),
});

const miningChannelSchema = z.object({
  id: z.string(),
  service: z.string(),
  transport: z.string(),
  address: z.string(),
  client: z.string(),
  workers: z.array(z.string()),
  connected: adminTimestampSchema,
  difficulty: z.number().nonnegative(),
  hashrate: z.number().nonnegative(),
  accepted: miningCountSchema,
  rejected: miningCountSchema,
  rejections: z.record(z.string(), miningCountSchema),
  last_share: adminTimestampSchema.nullable(),
  shares: z.array(miningShareSchema),
  history: z.array(z.tuple([adminTimestampSchema, z.number().nonnegative()])),
});

const miningBlockSchema = z.object({
  hash: z.string(),
  height: miningCountSchema,
  time: adminTimestampSchema,
  channel: z.string(),
});

// The mining channels of the stratum services and the blocks they found.
const adminMiningFrameSchema = z.object({
  channels: z.array(miningChannelSchema),
  blocks: z.array(miningBlockSchema),
});

export const adminFrameSchema = z.union([
  adminLogFrameSchema,
  adminEventFrameSchema,
  adminAckFrameSchema,
  adminMiningFrameSchema,
]);

export type AdminLogFrame = z.infer<typeof adminLogFrameSchema>;
export type AdminEventFrame = z.infer<typeof adminEventFrameSchema>;
export type AdminAckFrame = z.infer<typeof adminAckFrameSchema>;
export type AdminMiningFrame = z.infer<typeof adminMiningFrameSchema>;
export type MiningChannel = z.infer<typeof miningChannelSchema>;
export type AdminFrame = z.infer<typeof adminFrameSchema>;

export const ALL_LOG_FILTER = LOG_LEVELS.reduce(
  (filter, level) => (level.disabled ? filter : filter | (1 << level.id)),
  0
);
export const FAULT_LOG_FILTER = 2 ** 6;
export const DEFAULT_LOG_FILTER = FAULT_LOG_FILTER;
export const OPERATIONAL_LOG_FILTER = 2 ** 0 + 2 ** 1 + 2 ** 2 + 2 ** 6 + 2 ** 7;
export const ALL_EVENT_FILTER = 2 ** EVENT_DEFINITIONS.length - 1;

export function includesLogLevel(filter: number, level: number): boolean {
  return (
    Number.isInteger(level) &&
    level >= 0 &&
    level < 31 &&
    (filter & ALL_LOG_FILTER & (1 << level)) !== 0
  );
}

export function isAdminLogFrame(frame: AdminFrame): frame is AdminLogFrame {
  return 'level' in frame;
}

export function isAdminEventFrame(frame: AdminFrame): frame is AdminEventFrame {
  return 'event' in frame;
}

export function isAdminAckFrame(frame: AdminFrame): frame is AdminAckFrame {
  return 'previous' in frame;
}

export function isAdminMiningFrame(frame: AdminFrame): frame is AdminMiningFrame {
  return 'channels' in frame;
}

// The first header archived, which begins a synchronization from genesis.
export function isFirstHeaderFrame(frame: AdminFrame): boolean {
  return isAdminEventFrame(frame) && frame.event === 0 && frame.value === 1;
}

export function parseAdminFrame(text: string): AdminFrame | null {
  try {
    const value: unknown = JSON.parse(text) as unknown;
    if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
    // Select a schema directly instead of testing every union branch per event.
    const schema =
      'level' in value
        ? adminLogFrameSchema
        : 'event' in value
          ? adminEventFrameSchema
          : 'channels' in value
            ? adminMiningFrameSchema
            : adminAckFrameSchema;
    const result = schema.safeParse(value);
    return result.success ? result.data : null;
  } catch {
    return null;
  }
}

export function getEventDefinition(event: number): EventDefinition {
  return (
    EVENT_DEFINITIONS[event] ?? {
      id: event,
      name: `event_${String(event)}`,
      label: `Event ${String(event)}`,
      category: 'chain',
      valueKind: 'unknown',
    }
  );
}

export function getLogLevel(level: number) {
  return (
    LOG_LEVELS.find((item) => item.id === level) ?? {
      id: level,
      name: `level_${String(level)}`,
      label: `Level ${String(level)}`,
    }
  );
}

export function eventValueInMilliseconds(event: number, value: number): number | null {
  switch (getEventDefinition(event).valueKind) {
    case 'seconds': {
      return value * 1_000;
    }
    case 'milliseconds': {
      return value;
    }
    case 'microseconds': {
      return value / 1_000;
    }
    default: {
      return null;
    }
  }
}
