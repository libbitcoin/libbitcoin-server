import { describe, expect, it } from 'vitest';
import {
  loadCamera,
  loadPlotView,
  saveCamera,
  savePlotView,
} from '@/lib/utils/template-settings';

function memoryStorage(): Storage {
  const items = new Map<string, string>();
  return {
    getItem: (key: string) => items.get(key) ?? null,
    setItem: (key: string, value: string) => void items.set(key, value),
  } as Storage;
}

describe('template settings', () => {
  it('restores a stored camera', () => {
    const storage = memoryStorage();
    const camera = { azimuth: 1, elevation: 0.5, distance: 30, x: 2, y: 0.6, z: 3 };
    saveCamera(camera, storage);
    expect(loadCamera(storage)).toEqual(camera);
  });

  it('restores a stored plot view', () => {
    const storage = memoryStorage();
    savePlotView({ scale: 4, x: -1, y: -2 }, storage);
    expect(loadPlotView(storage)).toEqual({ scale: 4, x: -1, y: -2 });
  });

  it('has no view where none is stored', () => {
    expect(loadCamera(memoryStorage())).toBeNull();
    expect(loadPlotView(null)).toBeNull();
  });

  it('rejects a malformed view', () => {
    const storage = memoryStorage();
    storage.setItem('libbitcoinTemplateCamera', '{"azimuth":1,"elevation":"up"}');
    storage.setItem('libbitcoinTemplatePlot', 'not json');
    expect(loadCamera(storage)).toBeNull();
    expect(loadPlotView(storage)).toBeNull();
  });
});
