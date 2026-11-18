import { useSyncExternalStore } from 'react';

export const THEMES = [
  { id: 'current', label: 'Classic' },
  { id: 'light', label: 'Light' },
  { id: 'midnight', label: 'Midnight' },
] as const;

export type Theme = (typeof THEMES)[number]['id'];
const STORAGE_KEY = 'libbitcoinTheme';
const CHANGE_EVENT = 'libbitcoin-theme-change';

function parseTheme(value: unknown): Theme {
  return value === 'light' || value === 'midnight' ? value : 'current';
}

function readTheme(): Theme {
  try {
    return parseTheme(localStorage.getItem(STORAGE_KEY));
  } catch {
    return 'current';
  }
}

function applyTheme(theme: Theme): void {
  document.documentElement.dataset.theme = theme;
  const background = getComputedStyle(document.documentElement)
    .getPropertyValue('--theme-background')
    .trim();
  if (background) {
    document.querySelector('meta[name="theme-color"]')?.setAttribute('content', background);
  }
}

export function initializeTheme(): void {
  applyTheme(readTheme());
  // Keep other tabs in sync even while the theme selector is closed.
  globalThis.addEventListener('storage', onStorage);
}

export function setTheme(value: string): void {
  const theme = parseTheme(value);
  applyTheme(theme);
  try {
    localStorage.setItem(STORAGE_KEY, theme);
  } catch {
    // Theme changes still work for this page when storage is unavailable.
  }
  globalThis.dispatchEvent(new Event(CHANGE_EVENT));
}

function getTheme(): Theme {
  return parseTheme(document.documentElement.dataset.theme);
}

function onStorage(event: StorageEvent): void {
  if (event.key !== null && event.key !== STORAGE_KEY) return;
  applyTheme(readTheme());
  globalThis.dispatchEvent(new Event(CHANGE_EVENT));
}

function subscribe(listener: () => void): () => void {
  globalThis.addEventListener(CHANGE_EVENT, listener);
  return () => {
    globalThis.removeEventListener(CHANGE_EVENT, listener);
  };
}

export function useTheme(): Theme {
  return useSyncExternalStore(subscribe, getTheme, () => 'current');
}
