import {
  memo,
  useEffect,
  useMemo,
  useRef,
  useState,
  type MouseEvent,
  type PointerEvent,
} from 'react';
import { ViewFrame } from '@/components/view-frame';
import {
  formatTemplateRate,
  rateMix,
  templateTiles,
  transactionRate,
  type PoolTemplates,
  type TemplateTile,
} from '@/lib/utils/pool-template';
import { formatInteger } from '@/lib/utils/format';

interface TemplateTreemapProps {
  pool: PoolTemplates;
  template: number;
  selected: number | null;
  onSelect: (transaction: number) => void;
}

// The plot is 16:10; tiles are laid out in that aspect and placed in percent.
const WIDTH = 1.6;
const HEIGHT = 1;

// The view is the plot scaled about its top left and offset, the offsets in
// shares of the plot's extent. It is kept across blocks.
interface View {
  scale: number;
  x: number;
  y: number;
}

const HOME: View = { scale: 1, x: 0, y: 0 };
const MAXIMUM_SCALE = 32;
let kept: View = HOME;

const clamp = (value: number, low: number, high: number) => Math.max(low, Math.min(high, value));

// The scaled plot always covers the frame.
function bound(view: View): View {
  const scale = clamp(view.scale, 1, MAXIMUM_SCALE);
  return { scale, x: clamp(view.x, 1 - scale, 0), y: clamp(view.y, 1 - scale, 0) };
}

// Scales the view about a point of the frame, in shares of its extent.
function zoomAbout(view: View, factor: number, cx: number, cy: number): View {
  const scale = clamp(view.scale * factor, 1, MAXIMUM_SCALE);
  const ratio = scale / view.scale;
  return bound({ scale, x: cx - (cx - view.x) * ratio, y: cy - (cy - view.y) * ratio });
}

const percent = (value: number, extent: number) => `${((value / extent) * 100).toFixed(3)}%`;

function placement(tile: TemplateTile) {
  return {
    left: percent(tile.rect.x, WIDTH),
    top: percent(tile.rect.y, HEIGHT),
    width: percent(tile.rect.width, WIDTH),
    height: percent(tile.rect.height, HEIGHT),
  };
}

export function TemplateTreemap({
  pool,
  template,
  selected,
  onSelect,
}: TemplateTreemapProps) {
  const tiles = useMemo(() => templateTiles(pool, template, WIDTH, HEIGHT), [pool, template]);
  const marks = useMemo(() => {
    const entry = selected === null ? undefined : pool.transactions[selected];
    if (entry?.template !== template) return [];
    const members = new Set(pool.chunks[entry.chunk].transactions);
    return tiles.filter((tile) => members.has(tile.transaction));
  }, [pool, template, selected, tiles]);

  const frame = useRef<HTMLDivElement>(null);
  const pointers = useRef(new Map<number, { x: number; y: number }>());
  const moved = useRef(false);
  const [view, setView] = useState<View>(kept);

  useEffect(() => {
    kept = view;
  }, [view]);

  useEffect(() => {
    const element = frame.current;
    if (!element) return;
    const wheel = (event: WheelEvent) => {
      event.preventDefault();
      const bounds = element.getBoundingClientRect();
      const cx = (event.clientX - bounds.left) / bounds.width;
      const cy = (event.clientY - bounds.top) / bounds.height;
      setView((current) => zoomAbout(current, Math.exp(-event.deltaY * 0.0015), cx, cy));
    };
    element.addEventListener('wheel', wheel, { passive: false });
    return () => {
      element.removeEventListener('wheel', wheel);
    };
  }, []);

  function pointerDown(event: PointerEvent<HTMLDivElement>) {
    if (event.pointerType === 'mouse' && event.button !== 0) return;
    if (pointers.current.size === 0) moved.current = false;
    pointers.current.set(event.pointerId, { x: event.clientX, y: event.clientY });
  }

  // One pointer pans; two pan by their midpoint and zoom by their spread.
  // Capture waits for movement, so that a click without it reaches its tile.
  function pointerMove(event: PointerEvent<HTMLDivElement>) {
    const element = frame.current;
    const last = pointers.current.get(event.pointerId);
    if (!element || !last) return;
    const next = { x: event.clientX, y: event.clientY };
    if (!moved.current) {
      if (Math.abs(next.x - last.x) + Math.abs(next.y - last.y) < 4) return;
      moved.current = true;
    }
    if (!element.hasPointerCapture(event.pointerId)) element.setPointerCapture(event.pointerId);

    const others = [...pointers.current].filter(([id]) => id !== event.pointerId);
    pointers.current.set(event.pointerId, next);
    const bounds = element.getBoundingClientRect();
    const [, other] = others[0] ?? [];
    if (!other) {
      const dx = (next.x - last.x) / bounds.width;
      const dy = (next.y - last.y) / bounds.height;
      setView((current) => bound({ ...current, x: current.x + dx, y: current.y + dy }));
      return;
    }

    const spread = Math.hypot(last.x - other.x, last.y - other.y);
    const factor = spread === 0 ? 1 : Math.hypot(next.x - other.x, next.y - other.y) / spread;
    const dx = (next.x - last.x) / 2 / bounds.width;
    const dy = (next.y - last.y) / 2 / bounds.height;
    const cx = ((next.x + other.x) / 2 - bounds.left) / bounds.width;
    const cy = ((next.y + other.y) / 2 - bounds.top) / bounds.height;
    setView((current) =>
      zoomAbout({ ...current, x: current.x + dx, y: current.y + dy }, factor, cx, cy)
    );
  }

  function pointerUp(event: PointerEvent<HTMLDivElement>) {
    pointers.current.delete(event.pointerId);
  }

  // A drag ends in a click on its last tile, which is not a selection.
  function clickCapture(event: MouseEvent<HTMLDivElement>) {
    if (!moved.current) return;
    moved.current = false;
    event.stopPropagation();
  }

  return (
    <ViewFrame
      onPan={(dx, dy) => {
        setView((current) => bound({ ...current, x: current.x - dx, y: current.y - dy }));
      }}
      onReset={() => {
        setView(HOME);
      }}
      hint="Drag to pan · scroll to zoom"
      touchHint="Drag to pan · pinch to zoom"
    >
      <div
        ref={frame}
        onPointerDown={pointerDown}
        onPointerMove={pointerMove}
        onPointerUp={pointerUp}
        onPointerCancel={pointerUp}
        onClickCapture={clickCapture}
        className="bg-inset relative aspect-[16/10] w-full cursor-grab touch-none overflow-hidden rounded-xl select-none active:cursor-grabbing"
      >
        <div
          className="absolute inset-0 origin-top-left"
          style={{
            transform: `translate(${String(view.x * 100)}%, ${String(view.y * 100)}%) scale(${String(view.scale)})`,
          }}
        >
          <Tiles pool={pool} tiles={tiles} onSelect={onSelect} />
          {marks.map((tile) => (
            <div
              key={tile.transaction}
              aria-hidden="true"
              className={`pointer-events-none absolute z-10 ${
                tile.transaction === selected
                  ? 'ring-accent ring-offset-surface ring-2 ring-offset-1'
                  : 'ring-textPrimary ring-1 ring-inset'
              }`}
              style={placement(tile)}
            />
          ))}
        </div>
      </div>
    </ViewFrame>
  );
}

// Tiles depend only on the template, so a selection change does not
// re-render them. They are left out of the tab order: a template holds
// thousands, and the chunk list and template cards reach the same details.
const Tiles = memo(function Tiles({
  pool,
  tiles,
  onSelect,
}: {
  pool: PoolTemplates;
  tiles: TemplateTile[];
  onSelect: (transaction: number) => void;
}) {
  return tiles.map((tile) => {
    const transaction = pool.transactions[tile.transaction];
    const rate = transactionRate(transaction);
    return (
      <button
        key={tile.transaction}
        type="button"
        tabIndex={-1}
        aria-label={`${formatTemplateRate(rate)} sat/vB, ${formatInteger(transaction.size)} vB`}
        title={`${formatTemplateRate(rate)} sat/vB · ${formatInteger(transaction.size)} vB`}
        onClick={() => {
          onSelect(tile.transaction);
        }}
        className="absolute border-0 p-0 shadow-[inset_0_0_0_0.5px_var(--theme-surface)] hover:brightness-125"
        style={{ ...placement(tile), background: rateMix(rate) }}
      />
    );
  });
});
