import type { HeightMode } from './template-scene';

export type TemplateDisplay = '2d' | '3d';

/** A collapsible section of the block and template pages. */
export type PageSection = 'details' | 'size';

const STORAGE_KEY = 'libbitcoinTemplateDisplay';
const HEIGHT_KEY = 'libbitcoinTemplateHeight';
const HEIGHT_MODES: readonly HeightMode[] = ['chunk', 'own', 'fee'];
const SECTION_KEYS: Record<PageSection, string> = {
  details: 'libbitcoinSectionDetails',
  size: 'libbitcoinSectionSize',
};

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

export function loadSectionExpanded(
  section: PageSection,
  storage: Storage | null = browserStorage()
): boolean {
  try {
    return storage?.getItem(SECTION_KEYS[section]) !== 'collapsed';
  } catch {
    return true;
  }
}

export function saveSectionExpanded(
  section: PageSection,
  expanded: boolean,
  storage: Storage | null = browserStorage()
): void {
  try {
    storage?.setItem(SECTION_KEYS[section], expanded ? 'expanded' : 'collapsed');
  } catch {
    // The in-memory state still works when storage is unavailable.
  }
}

export function loadHeightMode(storage: Storage | null = browserStorage()): HeightMode {
  try {
    const value = storage?.getItem(HEIGHT_KEY);
    return HEIGHT_MODES.find((mode) => mode === value) ?? 'chunk';
  } catch {
    return 'chunk';
  }
}

export function saveHeightMode(mode: HeightMode, storage: Storage | null = browserStorage()): void {
  try {
    storage?.setItem(HEIGHT_KEY, mode);
  } catch {
    // The in-memory selection still works when storage is unavailable.
  }
}
