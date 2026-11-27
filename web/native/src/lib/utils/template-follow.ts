import type { TemplateDisplay } from './template-settings';

// How the template page follows the next block: once the template shown is
// mined, the page moves on to the template of the new next block. Times are in
// milliseconds.

// The 3D templates move out (and the next in) over this.
export const MOVE = 1200;

// The next template is awaited this long after the move out, at most.
export const LOAD = 3000;

// The height a template page was opened at and the top then.
export interface Opened {
  height: number;
  top: number;
}

// True if the template shown was mined while open (not already when opened).
export function isOvertaken(opened: Opened | undefined, height: number, top?: number): boolean {
  return opened?.height === height && opened.top < height && top !== undefined && height <= top;
}

// The height of the template to follow to, if the one shown was overtaken.
export function followHeight(opened: Opened | undefined, height: number, top?: number): number | undefined {
  return top !== undefined && isOvertaken(opened, height, top) ? top + 1 : undefined;
}

// The move before following: none (null) for a hidden page or reduced motion,
// which switch at once; the 3D move, or no move (0) for the 2D view, which
// then await the next template.
export function followMove(display: TemplateDisplay, hidden: boolean, reduced: boolean): number | null {
  if (hidden || reduced) return null;
  return display === '3d' ? MOVE : 0;
}

// Resolves once the move is done and the next template is loaded, or LOAD
// after the move if it is not.
export function followWhen(
  move: number,
  loaded: Promise<unknown>,
  wait: (ms: number) => Promise<void> = delay
): Promise<void> {
  return Promise.all([wait(move), Promise.race([loaded, wait(move + LOAD)])]).then(() => undefined);
}

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, ms);
  });
}
