import type { MouseEvent } from 'react';

// A press outside the box of a modal dialog, on its backdrop, closes it.
export function closeOnBackdrop(close: () => void) {
  return (event: MouseEvent<HTMLDialogElement>) => {
    if (event.target !== event.currentTarget) return;
    const box = event.currentTarget.getBoundingClientRect();
    const inside =
      event.clientX >= box.left &&
      event.clientX <= box.right &&
      event.clientY >= box.top &&
      event.clientY <= box.bottom;
    if (!inside) close();
  };
}
