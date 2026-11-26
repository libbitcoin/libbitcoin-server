import { useEffect, useRef } from 'react';
import type { PoolTemplates } from '@/lib/utils/pool-template';
import {
  parseHexColor,
  TemplateScene,
  type HeightMode,
  type ScenePalette,
} from '@/lib/utils/template-scene';
import { useTheme } from '@/lib/theme';
import { loadCamera, saveCamera } from '@/lib/utils/template-settings';
import { ViewFrame } from '@/components/view-frame';

interface TemplateSceneViewProps {
  pool: PoolTemplates;
  template: number;
  selected: number | null;
  mode: HeightMode;
  orbit?: boolean;
  onSelect: (transaction: number) => void;
  // The camera is stored when the scene closes and restored when it opens.
  kept?: boolean;
  // The templates move out over this many milliseconds, the next pool set
  // then moves in over as many.
  departing?: number;
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
  orbit = false,
  onSelect,
  kept = false,
  departing,
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
    const camera = kept ? loadCamera() : null;
    if (camera) created.setView(camera);
    scene.current = created;
    const keep = () => {
      if (kept) saveCamera(created.getView());
    };
    globalThis.addEventListener('pagehide', keep);
    return () => {
      globalThis.removeEventListener('pagehide', keep);
      keep();
      created.destroy();
      scene.current = null;
    };
  }, [kept]);

  useEffect(() => {
    if (departing) scene.current?.moveOut(departing);
  }, [departing]);

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
    scene.current?.setOrbit(orbit);
  }, [orbit]);

  useEffect(() => {
    scene.current?.setSelection(template, selected);
  }, [template, selected]);

  return (
    <ViewFrame
      onPan={(dx, dy) => {
        scene.current?.panBy(-dx, -dy);
      }}
      onReset={() => {
        scene.current?.resetView();
      }}
      hint="Drag to orbit · shift- or right-drag to pan · scroll to zoom · arrow keys and + / − when focused"
      touchHint="Drag to orbit · two fingers to pan, pinch to zoom, twist to turn"
    >
      <canvas
        ref={canvas}
        tabIndex={0}
        aria-label="Block templates in 3D. Drag to orbit, shift-drag or right-drag to pan, scroll to zoom. Arrow keys orbit and plus or minus zoom while focused."
        className="bg-surface focus-visible:outline-textPrimary block h-[min(640px,70vh)] min-h-80 w-full cursor-grab touch-none rounded-xl focus-visible:outline-2 focus-visible:outline-offset-2 active:cursor-grabbing"
      />
    </ViewFrame>
  );
}
