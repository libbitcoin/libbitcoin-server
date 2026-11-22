import { expect, test } from 'vitest';
import { mixColor, parseHexColor } from '@/lib/template-scene';
import { loadTemplateDisplay, saveTemplateDisplay } from '@/lib/template-settings';

test('template_scene__parse_hex_color__long__rgb', () => {
  expect(parseHexColor('#ffa022', [0, 0, 0])).toEqual([255, 160, 34]);
});

test('template_scene__parse_hex_color__short__rgb', () => {
  expect(parseHexColor(' #fa2 ', [0, 0, 0])).toEqual([255, 170, 34]);
});

test('template_scene__parse_hex_color__invalid__fallback', () => {
  expect(parseHexColor('rgb(1, 2, 3)', [7, 8, 9])).toEqual([7, 8, 9]);
});

test('template_scene__mix_color__share__weighted', () => {
  expect(mixColor([200, 100, 0], [0, 0, 100], 50)).toBe('rgb(100,50,50)');
});

test('template_scene__mix_color__shade__darkened', () => {
  expect(mixColor([200, 100, 0], [0, 0, 0], 100, 0.5)).toBe('rgb(100,50,0)');
});

function memoryStorage(): Storage {
  const entries = new Map<string, string>();
  return {
    get length() {
      return entries.size;
    },
    clear: () => {
      entries.clear();
    },
    getItem: (key) => entries.get(key) ?? null,
    key: (index) => [...entries.keys()][index] ?? null,
    removeItem: (key) => {
      entries.delete(key);
    },
    setItem: (key, value) => {
      entries.set(key, value);
    },
  };
}

test('template_settings__load_template_display__unset__2d', () => {
  expect(loadTemplateDisplay(memoryStorage())).toBe('2d');
});

test('template_settings__save_template_display__3d__loads_3d', () => {
  const storage = memoryStorage();
  saveTemplateDisplay('3d', storage);
  expect(loadTemplateDisplay(storage)).toBe('3d');
});

test('template_settings__load_template_display__no_storage__2d', () => {
  expect(loadTemplateDisplay(null)).toBe('2d');
});
