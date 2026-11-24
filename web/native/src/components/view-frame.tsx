import { useState, type ComponentType, type PointerEvent, type ReactNode, type SVGProps } from 'react';
import {
  ChevronDownIcon,
  ChevronLeftIcon,
  ChevronRightIcon,
  ChevronUpIcon,
} from '@heroicons/react/24/outline';

type Edge = 'left' | 'right' | 'top' | 'bottom';

interface ViewFrameProps {
  children: ReactNode;
  // Pans the view by a share of its extent, positive toward the right/bottom.
  onPan: (dx: number, dy: number) => void;
  onReset: () => void;
  // How the view is moved with a mouse and with touch.
  hint: string;
  touchHint: string;
}

const STEP = 0.15;
const NEAR = 72;

interface Arrow {
  label: string;
  place: string;
  dx: number;
  dy: number;
  Icon: ComponentType<SVGProps<SVGSVGElement>>;
}

const arrows: Record<Edge, Arrow> =
  {
    left: {
      label: 'Pan left',
      place: 'left-2 top-1/2 -translate-y-1/2',
      dx: -STEP,
      dy: 0,
      Icon: ChevronLeftIcon,
    },
    right: {
      label: 'Pan right',
      place: 'right-2 top-1/2 -translate-y-1/2',
      dx: STEP,
      dy: 0,
      Icon: ChevronRightIcon,
    },
    top: {
      label: 'Pan up',
      place: 'top-2 left-1/2 -translate-x-1/2',
      dx: 0,
      dy: -STEP,
      Icon: ChevronUpIcon,
    },
    bottom: {
      label: 'Pan down',
      place: 'bottom-2 left-1/2 -translate-x-1/2',
      dx: 0,
      dy: STEP,
      Icon: ChevronDownIcon,
    },
  };

const edges = Object.keys(arrows) as Edge[];

// The edges a point of the frame is near, as a key that changes only with them.
function nearEdges(event: PointerEvent<HTMLDivElement>): string {
  const bounds = event.currentTarget.getBoundingClientRect();
  const x = event.clientX - bounds.left;
  const y = event.clientY - bounds.top;
  const near: Record<Edge, boolean> = {
    left: x < NEAR,
    right: bounds.width - x < NEAR,
    top: y < NEAR,
    bottom: bounds.height - y < NEAR,
  };
  return edges.filter((edge) => near[edge]).join(' ');
}

// A pannable, zoomable view with pan arrows at its edges when the mouse nears
// them, and a reset below. Touch devices use gestures in place of the arrows.
export function ViewFrame({
  children,
  onPan,
  onReset,
  hint,
  touchHint,
}: ViewFrameProps) {
  const [near, setNear] = useState('');
  return (
    <div className="space-y-2">
      <div className="flex items-stretch gap-3">
        <div
          data-gestures
          className="relative min-w-0 flex-1"
          onPointerMove={(event) => {
            if (event.pointerType !== 'mouse') return;
            const next = nearEdges(event);
            if (next !== near) setNear(next);
          }}
          onPointerLeave={() => {
            setNear('');
          }}
        >
          {children}
          {edges.map((edge) => {
            const arrow = arrows[edge];
            const shown = near.includes(edge);
            return (
              <button
                key={edge}
                type="button"
                aria-label={arrow.label}
                title={arrow.label}
                tabIndex={shown ? 0 : -1}
                onPointerDown={(event) => {
                  event.stopPropagation();
                }}
                onClick={() => {
                  onPan(arrow.dx, arrow.dy);
                }}
                className={`absolute z-20 flex h-10 w-10 items-center justify-center rounded-full bg-black/35 text-white shadow-md transition-opacity duration-150 hover:bg-black/60 focus-visible:opacity-100 pointer-coarse:hidden ${arrow.place} ${
                  shown ? 'opacity-100' : 'pointer-events-none opacity-0'
                }`}
              >
                <arrow.Icon aria-hidden="true" strokeWidth={2.5} className="h-6 w-6 drop-shadow" />
              </button>
            );
          })}
        </div>
      </div>
      <div className="text-textMuted flex flex-wrap items-center justify-between gap-2 text-xs">
        <span className="pointer-coarse:hidden">{hint}</span>
        <span className="hidden pointer-coarse:inline">{touchHint}</span>
        <button
          type="button"
          onClick={onReset}
          className="border-accent/30 text-textMuted hover:text-accent hover:bg-accentSecondary/10 rounded-md border px-2 py-1.5"
        >
          Reset view
        </button>
      </div>
    </div>
  );
}

