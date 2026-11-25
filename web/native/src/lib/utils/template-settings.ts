import type { Camera, HeightMode } from './template-scene';

export type TemplateDisplay = '2d' | '3d';

/** A collapsible section of the block and template pages. */
export type PageSection = 'details' | 'size' | 'members';

/** The 2D plot scaled about its top left and offset, in shares of its extent. */
export interface PlotView {
  scale: number;
  x: number;
  y: number;
}

const STORAGE_KEY = 'libbitcoinTemplateDisplay';
const HEIGHT_KEY = 'libbitcoinTemplateHeight';
const CAMERA_KEY = 'libbitcoinTemplateCamera';
const PLOT_KEY = 'libbitcoinTemplatePlot';
const CAMERA_FIELDS = ['azimuth', 'elevation', 'distance', 'x', 'y', 'z'] as const;
const PLOT_FIELDS = ['scale', 'x', 'y'] as const;
const HEIGHT_MODES: readonly HeightMode[] = ['chunk', 'own', 'fee'];
const SECTION_KEYS: Record<PageSection, string> = {
  details: 'libbitcoinSectionDetails',
  size: 'libbitcoinSectionSize',
  members: 'libbitcoinSectionMembers',
};

function browserStorage(): Storage | null {
  try {
    return globalThis.localStorage;
  } catch {
    return null;
  }
}

// A stored record of finite numbers, or null where it is missing or malformed.
function loadNumbers<Field extends string>(
  key: string,
  fields: readonly Field[],
  storage: Storage | null
): Record<Field, number> | null {
  try {
    const value: unknown = JSON.parse(storage?.getItem(key) ?? 'null');
    if (typeof value !== 'object' || value === null) return null;
    const record = value as Record<string, unknown>;
    if (!fields.every((field) => Number.isFinite(record[field]))) return null;
    return Object.fromEntries(fields.map((field) => [field, record[field]])) as Record<Field, number>;
  } catch {
    return null;
  }
}

function saveNumbers(key: string, value: object, storage: Storage | null): void {
  try {
    storage?.setItem(key, JSON.stringify(value));
  } catch {
    // The in-memory view still works when storage is unavailable.
  }
}

export function loadCamera(storage: Storage | null = browserStorage()): Camera | null {
  return loadNumbers(CAMERA_KEY, CAMERA_FIELDS, storage);
}

export function saveCamera(camera: Camera, storage: Storage | null = browserStorage()): void {
  saveNumbers(CAMERA_KEY, camera, storage);
}

export function loadPlotView(storage: Storage | null = browserStorage()): PlotView | null {
  return loadNumbers(PLOT_KEY, PLOT_FIELDS, storage);
}

export function savePlotView(view: PlotView, storage: Storage | null = browserStorage()): void {
  saveNumbers(PLOT_KEY, view, storage);
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
