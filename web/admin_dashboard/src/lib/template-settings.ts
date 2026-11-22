export type TemplateDisplay = '2d' | '3d';

const STORAGE_KEY = 'libbitcoinAdminDashboardTemplateDisplay';

function browserStorage(): Storage | null {
  try {
    return globalThis.localStorage;
  } catch {
    return null;
  }
}

export function loadTemplateDisplay(storage: Storage | null = browserStorage()): TemplateDisplay {
  try {
    return storage?.getItem(STORAGE_KEY) === '3d' ? '3d' : '2d';
  } catch {
    return '2d';
  }
}

export function saveTemplateDisplay(
  display: TemplateDisplay,
  storage: Storage | null = browserStorage()
): void {
  try {
    storage?.setItem(STORAGE_KEY, display);
  } catch {
    // The in-memory selection still works when storage is unavailable.
  }
}
