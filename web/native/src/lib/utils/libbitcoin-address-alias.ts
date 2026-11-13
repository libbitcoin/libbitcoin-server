import { inferAddressType } from './address';

interface AddressAliasEntry {
  network: string;
  scriptHash: string;
  address: string;
  seenAtMs: number;
}

const STORAGE_KEY = 'libbitcoinAddressAliases';
const MAX_ENTRIES = 200;
const SCRIPT_HASH_PATTERN = /^[0-9a-f]{64}$/u;

function getLocalStorage() {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  if (!window_) return null;
  try {
    return window_.localStorage;
  } catch {
    return null;
  }
}

function normalizeScriptHash(value: string) {
  return value.trim().toLowerCase();
}

function parseAliases(raw: string | null): AddressAliasEntry[] {
  if (!raw) return [];
  try {
    const data: unknown = JSON.parse(raw);
    if (!Array.isArray(data)) return [];
    const entries: AddressAliasEntry[] = [];
    for (const item of data) {
      if (!item || typeof item !== 'object') continue;
      const record = item as Partial<AddressAliasEntry>;
      if (typeof record.network !== 'string' || !record.network.trim()) continue;
      if (typeof record.scriptHash !== 'string') continue;
      const hash = normalizeScriptHash(record.scriptHash);
      if (!SCRIPT_HASH_PATTERN.test(hash)) continue;
      if (typeof record.address !== 'string' || !record.address.trim()) continue;
      if (!inferAddressType(record.address)) continue;
      if (typeof record.seenAtMs !== 'number' || !Number.isFinite(record.seenAtMs)) continue;
      entries.push({
        network: record.network.trim(),
        scriptHash: hash,
        address: record.address.trim(),
        seenAtMs: record.seenAtMs,
      });
    }
    entries.sort((a, b) => b.seenAtMs - a.seenAtMs);
    const seen = new Set<string>();
    const deduped: AddressAliasEntry[] = [];
    for (const entry of entries) {
      const key = `${entry.network}|${entry.scriptHash}`;
      if (seen.has(key)) continue;
      seen.add(key);
      deduped.push(entry);
      if (deduped.length >= MAX_ENTRIES) break;
    }
    return deduped;
  } catch {
    return [];
  }
}

function saveAliases(storage: Storage, entries: AddressAliasEntry[]) {
  try {
    storage.setItem(STORAGE_KEY, JSON.stringify(entries.slice(0, MAX_ENTRIES)));
  } catch {
    // ignore storage failures
  }
}

export function rememberLibbitcoinAddressAlias(
  network: string,
  scriptHash: string,
  address: string
) {
  const networkNormalized = network.trim();
  if (!networkNormalized) return;
  const hash = normalizeScriptHash(scriptHash);
  if (!SCRIPT_HASH_PATTERN.test(hash)) return;
  const addressTrimmed = address.trim();
  if (!addressTrimmed) return;
  if (!inferAddressType(addressTrimmed)) return;

  const storage = getLocalStorage();
  if (!storage) return;

  const existing = parseAliases(storage.getItem(STORAGE_KEY));
  const nextEntry: AddressAliasEntry = {
    network: networkNormalized,
    scriptHash: hash,
    address: addressTrimmed,
    seenAtMs: Date.now(),
  };

  const filtered = existing.filter(
    (entry) => !(entry.network === networkNormalized && entry.scriptHash === hash)
  );
  const merged = [nextEntry, ...filtered].slice(0, MAX_ENTRIES);
  saveAliases(storage, merged);
}

export function lookupLibbitcoinAddressAlias(network: string, scriptHash: string): string | null {
  const networkNormalized = network.trim();
  if (!networkNormalized) return null;
  const hash = normalizeScriptHash(scriptHash);
  if (!SCRIPT_HASH_PATTERN.test(hash)) return null;
  const storage = getLocalStorage();
  if (!storage) return null;
  const entries = parseAliases(storage.getItem(STORAGE_KEY));
  const match = entries.find(
    (entry) => entry.network === networkNormalized && entry.scriptHash === hash
  );
  if (!match) return null;
  return match.address;
}
