import { z } from 'zod';
import type { BlockchainClientMetadata } from '@/lib/api/client';
import { getNetwork } from '@/lib/api/providers/libbitcoin-mapper';
import { inferAddressType } from './address';
import {
  addressPath,
  blockByHashPath,
  transactionInputPath,
  transactionOutputPath,
  transactionPath,
} from './routes';

export const INVESTIGATIONS_STORAGE_KEY = 'savedInvestigations';
export const INBOX_ID = 'bookmarks';
const hash = z
  .string()
  .regex(/^[0-9a-f]{64}$/iu)
  .transform((value) => value.toLowerCase());
const address = z
  .string()
  .regex(/^[a-z0-9]+$/iu)
  .max(120)
  .refine(
    (value) => /^[0-9a-f]{64}$/iu.test(value) || Boolean(inferAddressType(value)),
    'Invalid address or script hash'
  )
  .transform((value) =>
    /^[0-9a-f]{64}$/iu.test(value) || /^(bc1|tb1|bcrt1)/iu.test(value) ? value.toLowerCase() : value
  );
const index = z.number().int().min(0).max(0xffffffff);
const timestamp = z.number().int().nonnegative().max(Number.MAX_SAFE_INTEGER);
const id = z.string().min(1).max(100);
export const bookmarkTargetSchema = z.discriminatedUnion('kind', [
  z.object({ kind: z.literal('block'), hash, height: z.number().int().nonnegative().optional() }),
  z.object({ kind: z.literal('transaction'), txid: hash }),
  z.object({ kind: z.literal('address'), address, displayAddress: address.optional() }),
  z.object({ kind: z.literal('input'), txid: hash, index }),
  z.object({ kind: z.literal('output'), txid: hash, index }),
]);
const sourceSchema = z.object({
  providerId: z.string().min(1).max(80),
  endpoint: z
    .string()
    .max(2048)
    .transform((value) => value.replace(/\/+$/u, '')),
  network: z.enum(['mainnet', 'testnet', 'regtest']).optional(),
});
const investigationSchema = z.object({
  id,
  name: z.string().trim().min(1).max(80),
  notes: z.string().max(5000),
  createdAt: timestamp,
  updatedAt: timestamp,
});
const bookmarkSchema = z.object({
  id,
  investigationId: id,
  target: bookmarkTargetSchema,
  source: sourceSchema,
  label: z.string().trim().min(1).max(120),
  notes: z.string().max(5000),
  createdAt: timestamp,
  updatedAt: timestamp,
});
const dataSchema = z.object({
  version: z.literal(1),
  investigations: z.array(investigationSchema).min(1).max(200),
  bookmarks: z.array(bookmarkSchema).max(5000),
});
export type BookmarkTarget = z.infer<typeof bookmarkTargetSchema>;
export type BookmarkSource = z.infer<typeof sourceSchema>;
export type SavedBookmark = z.infer<typeof bookmarkSchema>;
export type Investigation = z.infer<typeof investigationSchema>;
export type InvestigationsData = z.infer<typeof dataSchema>;

export function newInvestigationId() {
  // Unlike randomUUID, getRandomValues also works on HTTP-hosted node frontends.
  return Array.from(crypto.getRandomValues(new Uint8Array(16)), (byte) =>
    byte.toString(16).padStart(2, '0')
  ).join('');
}

export function emptyInvestigations(): InvestigationsData {
  return {
    version: 1,
    investigations: [
      { id: INBOX_ID, name: 'Bookmarks', notes: '', createdAt: Date.now(), updatedAt: Date.now() },
    ],
    bookmarks: [],
  };
}

export function bookmarkSource(metadata: BlockchainClientMetadata): BookmarkSource {
  return {
    providerId: metadata.id,
    endpoint: (metadata.endpoint ?? '').replace(/\/+$/u, ''),
    ...(metadata.id === 'libbitcoin' ? { network: getNetwork() } : {}),
  };
}

export function sourceKey(source: BookmarkSource) {
  return JSON.stringify([source.providerId, source.endpoint, source.network ?? '']);
}

export function bookmarkPath(target: BookmarkTarget) {
  switch (target.kind) {
    case 'block': {
      return blockByHashPath(target.hash);
    }
    case 'transaction': {
      return transactionPath(target.txid);
    }
    case 'address': {
      return addressPath(target.address);
    }
    case 'input': {
      return transactionInputPath(target.txid, target.index);
    }
    case 'output': {
      return transactionOutputPath(target.txid, target.index);
    }
  }
}

export function bookmarkIdentity(target: BookmarkTarget, source: BookmarkSource) {
  return JSON.stringify([sourceKey(source), bookmarkPath(target)]);
}

export function bookmarkIdentifier(target: BookmarkTarget) {
  if (target.kind === 'block') return target.hash;
  if (target.kind === 'address') return target.displayAddress ?? target.address;
  return target.kind === 'transaction' ? target.txid : `${target.txid}:${String(target.index)}`;
}

export function defaultBookmarkLabel(target: BookmarkTarget) {
  if (target.kind === 'block' && target.height !== undefined)
    return `Block #${String(target.height)}`;
  const identifier = bookmarkIdentifier(target);
  return `${target.kind[0].toUpperCase()}${target.kind.slice(1)} ${identifier.slice(0, 12)}…${identifier.slice(-8)}`;
}

export function parseInvestigations(raw: string): InvestigationsData {
  if (raw.length > 10_000_000) throw new Error('Investigation data exceeds the 10 MB limit.');
  const result = dataSchema.safeParse(JSON.parse(raw) as unknown);
  if (!result.success) throw new Error('Invalid saved investigations data or unsupported version.');
  const data = result.data;
  const investigationIds = new Set(data.investigations.map((item) => item.id));
  const bookmarkIds = new Set(data.bookmarks.map((item) => item.id));
  const identities = new Set(
    data.bookmarks.map((item) =>
      JSON.stringify([item.investigationId, bookmarkIdentity(item.target, item.source)])
    )
  );
  if (
    !investigationIds.has(INBOX_ID) ||
    investigationIds.size !== data.investigations.length ||
    bookmarkIds.size !== data.bookmarks.length ||
    identities.size !== data.bookmarks.length ||
    data.bookmarks.some((item) => !investigationIds.has(item.investigationId))
  ) {
    throw new Error('Saved investigations contain duplicate or missing references.');
  }
  return data;
}

/** Import as new investigations, preserving existing labels and notes. */
export function mergeInvestigations(
  current: InvestigationsData,
  incoming: InvestigationsData
): InvestigationsData {
  const ids = new Map(incoming.investigations.map((item) => [item.id, newInvestigationId()]));
  return {
    version: 1,
    investigations: [
      ...current.investigations,
      ...incoming.investigations.map((item) => ({
        ...item,
        id: ids.get(item.id) ?? '',
        name: `${item.name.slice(0, 69)} (imported)`,
      })),
    ],
    bookmarks: [
      ...current.bookmarks,
      ...incoming.bookmarks.map((item) => ({
        ...item,
        id: newInvestigationId(),
        investigationId: ids.get(item.investigationId) ?? '',
      })),
    ],
  };
}
