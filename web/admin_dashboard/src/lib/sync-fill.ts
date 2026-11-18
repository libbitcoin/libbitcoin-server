import { EVENT_DEFINITIONS } from './admin-contract';

export const SYNC_FILL_BINS = 600;

export interface SyncFrontier {
  height: number;
  receivedAt: number;
}

export interface SyncFill {
  height: number;
  headers: number;
  archived: Uint8Array;
  validated: Uint8Array;
  candidate: SyncFrontier | null;
  confirmed: SyncFrontier | null;
}

const eventId = (name: string) => EVENT_DEFINITIONS.findIndex((event) => event.name === name);
const HEADER_ARCHIVED = eventId('header_archived');
const HEADER_ORGANIZED = eventId('header_organized');
const HEADER_REORGANIZED = eventId('header_reorganized');
const BLOCK_ARCHIVED = eventId('block_archived');
const BLOCK_VALIDATED = eventId('block_validated');
const VALIDATE_BYPASSED = eventId('validate_bypassed');
const BLOCK_ORGANIZED = eventId('block_organized');
const BLOCK_REORGANIZED = eventId('block_reorganized');

const BIT_COUNTS = Uint8Array.from({ length: 256 }, (_, value) => {
  let count = 0;
  for (let bits = value; bits > 0; bits >>= 1) count += bits & 1;
  return count;
});

class HeightSet {
  #bits = new Uint8Array(0);

  add(height: number): void {
    const index = height >> 3;
    if (index >= this.#bits.length) {
      const grown = new Uint8Array(Math.max(index + 1, this.#bits.length * 2, 1_024));
      grown.set(this.#bits);
      this.#bits = grown;
    }
    this.#bits[index] |= 1 << (height & 7);
  }

  clear(): void {
    this.#bits = new Uint8Array(0);
  }

  count(first: number, last: number): number {
    let total = 0;
    let height = first;
    while (height <= last && (height & 7) !== 0) total += this.#has(height++) ? 1 : 0;
    while (height + 7 <= last) {
      total += BIT_COUNTS[this.#bits[height >> 3] ?? 0] ?? 0;
      height += 8;
    }
    while (height <= last) total += this.#has(height++) ? 1 : 0;
    return total;
  }

  #has(height: number): boolean {
    return ((this.#bits[height >> 3] ?? 0) & (1 << (height & 7))) !== 0;
  }
}

// Heights reported by block archive and validation events, which arrive out
// of order, with the header, candidate and confirmed frontiers as last reported.
export class SyncFillTracker {
  #headers = 0;
  #archived = new HeightSet();
  #validated = new HeightSet();
  #height = 0;
  #candidate: SyncFrontier | null = null;
  #confirmed: SyncFrontier | null = null;

  record(event: number, height: number, receivedAt: number): void {
    if (event === HEADER_ARCHIVED) {
      this.#headers = Math.max(this.#headers, height);
    } else if (event === BLOCK_ARCHIVED) {
      this.#archived.add(height);
    } else if (event === BLOCK_VALIDATED || event === VALIDATE_BYPASSED) {
      this.#validated.add(height);
    } else if (event === HEADER_ORGANIZED) {
      this.#candidate = { height, receivedAt };
    } else if (event === HEADER_REORGANIZED) {
      this.#candidate = { height: Math.max(0, height - 1), receivedAt };
    } else if (event === BLOCK_ORGANIZED) {
      this.#confirmed = { height, receivedAt };
    } else if (event === BLOCK_REORGANIZED) {
      this.#confirmed = { height: Math.max(0, height - 1), receivedAt };
    } else {
      return;
    }
    this.#height = Math.max(this.#height, height);
  }

  reset(): void {
    this.#headers = 0;
    this.#archived.clear();
    this.#validated.clear();
    this.#height = 0;
    this.#candidate = null;
    this.#confirmed = null;
  }

  // Each bin holds the filled fraction of its height interval, scaled to 255.
  snapshot(bins = SYNC_FILL_BINS): SyncFill {
    const archived = new Uint8Array(bins);
    const validated = new Uint8Array(bins);
    const span = this.#height + 1;
    for (let bin = 0; bin < bins; bin++) {
      const first = Math.floor((bin * span) / bins);
      const last = Math.floor(((bin + 1) * span) / bins) - 1;
      if (last < first) continue;
      const size = last - first + 1;
      archived[bin] = Math.round((this.#archived.count(first, last) * 255) / size);
      validated[bin] = Math.round((this.#validated.count(first, last) * 255) / size);
    }
    return {
      height: this.#height,
      headers: this.#headers,
      archived,
      validated,
      candidate: this.#candidate,
      confirmed: this.#confirmed,
    };
  }
}
