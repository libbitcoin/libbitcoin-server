import { useEffect, useLayoutEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';

const DISTANCE = 64;
const DURATION = 700;

// A touch from within an element that takes its own gestures, or that scrolls
// across, is not a page swipe.
function claimed(target: EventTarget | null): boolean {
  for (let node = target instanceof Element ? target : null; node; node = node.parentElement) {
    if (node instanceof HTMLElement && node.dataset.gestures !== undefined) return true;
    if (node.scrollWidth > node.clientWidth) {
      const overflow = getComputedStyle(node).overflowX;
      if (overflow === 'auto' || overflow === 'scroll') return true;
    }
  }
  return false;
}

// A one-finger horizontal swipe across the page opens the previous (right) or
// next (left) path.
export function useSwipeNavigation(previous: string | null, next: string | null) {
  const navigate = useNavigate();
  const paths = useRef({ previous, next });

  useLayoutEffect(() => {
    paths.current = { previous, next };
  }, [previous, next]);

  useEffect(() => {
    let start: { x: number; y: number; time: number } | null = null;
    const touchStart = (event: TouchEvent) => {
      const touch = event.touches.item(0);
      start =
        event.touches.length === 1 && touch && !claimed(event.target)
          ? { x: touch.clientX, y: touch.clientY, time: event.timeStamp }
          : null;
    };
    const touchEnd = (event: TouchEvent) => {
      const touch = event.changedTouches.item(0);
      const begin = start;
      start = null;
      if (!begin || !touch || event.timeStamp - begin.time > DURATION) return;
      const dx = touch.clientX - begin.x;
      const dy = touch.clientY - begin.y;
      if (Math.abs(dx) < DISTANCE || Math.abs(dx) < 2 * Math.abs(dy)) return;
      const path = dx > 0 ? paths.current.previous : paths.current.next;
      if (path) void navigate(path);
    };
    const touchCancel = () => {
      start = null;
    };

    globalThis.window.addEventListener('touchstart', touchStart, { passive: true });
    globalThis.window.addEventListener('touchend', touchEnd, { passive: true });
    globalThis.window.addEventListener('touchcancel', touchCancel, { passive: true });
    return () => {
      globalThis.window.removeEventListener('touchstart', touchStart);
      globalThis.window.removeEventListener('touchend', touchEnd);
      globalThis.window.removeEventListener('touchcancel', touchCancel);
    };
  }, [navigate]);
}
