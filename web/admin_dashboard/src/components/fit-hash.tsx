import { useLayoutEffect, useRef } from 'react';
import { formatHash } from '@/lib/format';

// The fewest characters kept on each side of a shortened hash.
const MINIMUM = 4;

// A hash shown whole where its line has room, else shortened in the middle
// to fit. The line is the nearest [data-fit] ancestor, a single line whose
// width is set by its layout, not by its content.
export function FitHash({ value, className }: { value: string; className?: string }) {
  const ref = useRef<HTMLSpanElement>(null);

  useLayoutEffect(() => {
    const element = ref.current;
    const line = element?.closest<HTMLElement>('[data-fit]');
    const node = element?.firstChild;
    if (!element || !line || !node) return;

    const fits = (text: string) => {
      node.nodeValue = text;
      return line.scrollWidth <= line.clientWidth;
    };

    const fit = () => {
      if (fits(value)) return;
      let low = MINIMUM;
      let high = Math.floor(value.length / 2) - 1;
      let best = formatHash(value, MINIMUM);
      while (low <= high) {
        const middle = Math.floor((low + high) / 2);
        const text = formatHash(value, middle);
        if (fits(text)) {
          best = text;
          low = middle + 1;
        } else {
          high = middle - 1;
        }
      }
      node.nodeValue = best;
    };

    fit();
    if (typeof ResizeObserver === 'undefined') return;
    const observer = new ResizeObserver(fit);
    observer.observe(line);
    return () => {
      observer.disconnect();
    };
  }, [value]);

  return (
    <span ref={ref} className={className}>
      {value}
    </span>
  );
}
