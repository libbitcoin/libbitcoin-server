// The move of a scene's templates along the horizontal of its view, as they
// are replaced: the shown templates move out to the left, the next move in
// from the right. Times are in milliseconds.

interface Motion {
  from: number;
  to: number;
  start: number | null;
  duration: number;
  done?: () => void;
}

export interface SlideScene<Pool> {
  // Shows the pool (in place, at the current shift).
  load: (pool: Pool) => void;
  // Far enough along the view for the templates to leave it.
  travel: () => number;
  // False where motion is reduced, moves then complete at once.
  animate: boolean;
}

export class TemplateSlide<Pool> {
  // Offset of the templates along the horizontal of the view.
  shift = 0;
  private motion: Motion | null = null;
  private duration = 0;
  private loaded = false;
  private out = false;
  private parked: Pool | null = null;

  constructor(private readonly scene: SlideScene<Pool>) {}

  // True while a move is under way (and frames are needed).
  get moving(): boolean {
    return this.motion !== null;
  }

  // A pool set while the templates move out comes in once they are out, one
  // set after they are out comes in at once, otherwise it is shown in place.
  set(pool: Pool): void {
    if (this.motion && this.motion.to < 0) {
      this.parked = pool;
      return;
    }

    if (this.out) {
      this.enter(pool);
      return;
    }

    this.load(pool);
  }

  // Moves the templates out to the left of the view over the duration. The
  // next pool set then moves in from the right over the same duration.
  moveOut(duration: number): void {
    if (!this.loaded || this.out || (this.motion && this.motion.to < 0)) return;
    this.duration = duration;
    if (!this.scene.animate) {
      this.out = true;
      return;
    }

    this.slideTo(-this.scene.travel(), duration, () => {
      this.out = true;
      const parked = this.parked;
      this.parked = null;
      if (parked) this.enter(parked);
    });
  }

  // Advances the move to the time; true while it is still under way.
  step(time: number): boolean {
    const motion = this.motion;
    if (!motion) return false;
    motion.start ??= time;
    const progress = Math.min(1, (time - motion.start) / motion.duration);
    this.shift = motion.from + (motion.to - motion.from) * ease(progress, motion.to < 0);
    if (progress >= 1) {
      this.motion = null;
      motion.done?.();
    }
    return this.motion !== null;
  }

  private enter(pool: Pool): void {
    this.out = false;
    this.load(pool);
    if (!this.scene.animate) {
      this.shift = 0;
      return;
    }

    this.shift = this.scene.travel();
    this.slideTo(0, this.duration);
  }

  private load(pool: Pool): void {
    this.loaded = true;
    this.scene.load(pool);
  }

  private slideTo(to: number, duration: number, done?: () => void): void {
    this.motion = { from: this.shift, to, start: null, duration: Math.max(1, duration), done };
  }
}

// Out accelerates away, in decelerates to rest.
export function ease(progress: number, out: boolean): number {
  return out ? progress ** 3 : 1 - (1 - progress) ** 3;
}
