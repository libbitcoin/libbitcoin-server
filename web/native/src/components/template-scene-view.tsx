import { useEffect, useRef, type MutableRefObject } from 'react';
import type { PoolTemplates } from '@/lib/utils/pool-template';
import {
  parseHexColor,
  TemplateScene,
  type Camera,
  type HeightMode,
  type ScenePalette,
} from '@/lib/utils/template-scene';
import { useTheme } from '@/lib/theme';

interface TemplateSceneViewProps {
  pool: PoolTemplates;
  template: number;
  selected: number | null;
  mode: HeightMode;
  onSelect: (transaction: number) => void;
  // The camera kept while the scene is closed, restored when it reopens.
  view?: MutableRefObject<Camera | null>;
}

function readPalette(): ScenePalette {
  const styles = getComputedStyle(document.documentElement);
  const color = (name: string, fallback: readonly [number, number, number]) =>
    parseHexColor(styles.getPropertyValue(name), fallback);
  return {
    surface: color('--theme-surface', [31, 33, 28]),
    accent: color('--theme-accent', [255, 160, 34]),
    text: color('--theme-text-primary', [237, 237, 237]),
    muted: color('--theme-text-muted', [207, 207, 207]),
    font: getComputedStyle(document.body).fontFamily || 'sans-serif',
  };
}

export function TemplateSceneView({
  pool,
  template,
  selected,
  mode,
  onSelect,
  view,
}: TemplateSceneViewProps) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const scene = useRef<TemplateScene | null>(null);
  const select = useRef(onSelect);
  const theme = useTheme();

  useEffect(() => {
    select.current = onSelect;
  }, [onSelect]);

  useEffect(() => {
    if (!canvas.current) return;
    const animate = !globalThis.matchMedia('(prefers-reduced-motion: reduce)').matches;
    const created = new TemplateScene(
      canvas.current,
      (transaction) => {
        select.current(transaction);
      },
      animate
    );
    if (view?.current) created.setView(view.current);
    scene.current = created;
    return () => {
      if (view) view.current = created.getView();
      created.destroy();
      scene.current = null;
    };
  }, [view]);

  useEffect(() => {
    scene.current?.setPool(pool);
  }, [pool]);

  useEffect(() => {
    scene.current?.setPalette(readPalette());
  }, [theme]);

  useEffect(() => {
    scene.current?.setMode(mode);
  }, [mode]);

  useEffect(() => {
    scene.current?.setSelection(template, selected);
  }, [template, selected]);

  return (
    <div className="space-y-2">
      <canvas
        ref={canvas}
        tabIndex={0}
        aria-label="Block templates in 3D. Drag to orbit, shift-drag or right-drag to pan, scroll to zoom. Arrow keys orbit and plus or minus zoom while focused."
        className="bg-surface focus-visible:outline-textPrimary block h-[min(640px,70vh)] min-h-80 w-full cursor-grab touch-none rounded-xl focus-visible:outline-2 focus-visible:outline-offset-2 active:cursor-grabbing"
      />
      <div className="text-textMuted flex flex-wrap items-center justify-between gap-2 text-xs">
        <span>Drag to orbit · shift- or right-drag to pan · scroll to zoom · arrow keys and + / − when focused</span>
        <button
          type="button"
          onClick={() => {
            scene.current?.resetView();
          }}
          className="border-accent/30 text-textMuted hover:text-accent hover:bg-accentSecondary/10 rounded-md border px-2 py-1.5"
        >
          Reset view
        </button>
      </div>
    </div>
  );
}
