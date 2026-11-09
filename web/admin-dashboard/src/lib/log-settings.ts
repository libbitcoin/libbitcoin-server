import { ALL_LOG_FILTER, DEFAULT_LOG_FILTER } from './admin-contract';

const STORAGE_KEY = 'libbitcoinAdminDashboardLogFilter';

function browserStorage(): Storage | null {
  try {
    return globalThis.localStorage;
  } catch {
    return null;
  }
}

export function loadLogFilter(storage: Storage | null = browserStorage()): number {
  try {
    const saved = storage?.getItem(STORAGE_KEY);
    if (!saved) return DEFAULT_LOG_FILTER;
    const filter: unknown = JSON.parse(saved) as unknown;
    return typeof filter === 'number' &&
      Number.isInteger(filter) &&
      filter >= 0 &&
      filter <= ALL_LOG_FILTER
      ? filter
      : DEFAULT_LOG_FILTER;
  } catch {
    return DEFAULT_LOG_FILTER;
  }
}

export function saveLogFilter(filter: number, storage: Storage | null = browserStorage()): void {
  try {
    storage?.setItem(STORAGE_KEY, String(filter));
  } catch {
    // The in-memory selection still works when storage is unavailable.
  }
}
