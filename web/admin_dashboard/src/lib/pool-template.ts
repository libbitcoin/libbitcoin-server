// Block templates as the pool would fill them: each cluster of related
// transactions is linearized into chunks, and chunks are taken by descending
// fee rate. Sizes are virtual bytes, fees are satoshis.

export interface PoolTransaction {
  hash: string;
  size: number;
  fee: number;
  parents: number[];
  children: number[];
  cluster: number;
  chunk: number;
  // Index of the template that holds the transaction, -1 if none does.
  template: number;
  // One-based position of the transaction in its template.
  position: number;
}

export interface PoolChunk {
  cluster: number;
  transactions: number[];
  size: number;
  fee: number;
}

export interface PoolCluster {
  transactions: number[];
  // Chunks in mining order.
  chunks: number[];
}

export interface BlockTemplate {
  // Chunks in mining order.
  chunks: number[];
  count: number;
  size: number;
  fee: number;
}

export interface PoolTemplates {
  transactions: PoolTransaction[];
  chunks: PoolChunk[];
  clusters: PoolCluster[];
  templates: BlockTemplate[];
  capacity: number;
}

export interface TemplateRates {
  minimum: number;
  median: number;
  maximum: number;
}

export function chunkRate(chunk: PoolChunk): number {
  return chunk.fee / chunk.size;
}

export function transactionRate(transaction: PoolTransaction): number {
  return transaction.fee / transaction.size;
}

// Chunks are in descending rate order, so the median is the rate of the chunk
// that covers the middle virtual byte.
export function templateRates(pool: PoolTemplates, template: BlockTemplate): TemplateRates {
  const chunks = template.chunks.map((index) => pool.chunks[index]);
  if (chunks.length === 0) return { minimum: 0, median: 0, maximum: 0 };
  let median = chunkRate(chunks[chunks.length - 1]);
  let covered = 0;
  for (const chunk of chunks) {
    covered += chunk.size;
    if (covered * 2 >= template.size) {
      median = chunkRate(chunk);
      break;
    }
  }

  return {
    minimum: chunkRate(chunks[chunks.length - 1]),
    median,
    maximum: chunkRate(chunks[0]),
  };
}

export function templateName(index: number): string {
  return index === 0 ? 'Block 1 · next' : `Block ${String(index + 1)}`;
}

// The first transaction of the first chunk of more than two transactions, so
// the initial selection shows a package where there is one.
export function featuredTransaction(pool: PoolTemplates, template: number): number | null {
  const chunks = pool.templates.at(template)?.chunks ?? [];
  const featured = chunks.find((index) => pool.chunks[index].transactions.length > 2);
  const chunk = featured ?? chunks.at(0);
  return chunk === undefined ? null : pool.chunks[chunk].transactions[0];
}

export function describePlacement(chunk: PoolChunk, own: number): string {
  if (chunk.transactions.length === 1)
    return 'Mined at its own fee rate. No unconfirmed relative changes its place.';
  // Rates within a thousandth of each other print the same.
  const rate = chunkRate(chunk);
  if (rate > own * 1.001)
    return `Its chunk pays ${formatTemplateRate(rate)} sat/vB, more than its own ${formatTemplateRate(own)}: a descendant pays for it.`;
  if (rate < own * 0.999)
    return `It pays ${formatTemplateRate(own)} sat/vB but waits for lower-rate ancestors, so it is mined at the chunk rate of ${formatTemplateRate(rate)}.`;
  return 'Its chunk rate equals its own rate.';
}

// Fee rates are shown on a log scale from 1 to this rate, in sat/vB.
export const RATE_CEILING = 300;

export function ratePosition(rate: number): number {
  return Math.min(1, Math.max(0, Math.log(Math.max(rate, 1)) / Math.log(RATE_CEILING)));
}

export type ThemeColor = 'surface' | 'accent' | 'text';

export interface RateBlend {
  tint: ThemeColor;
  base: ThemeColor;
  // Percentage of the tint mixed into the base.
  share: number;
}

// Rates up to this position mix the accent into the surface; higher rates mix
// the text color into the accent. The scale keeps moving away from the
// surface in lightness, in light and dark themes.
const ACCENT_POSITION = 0.7;

export function rateBlend(rate: number): RateBlend {
  const position = ratePosition(rate);
  return position <= ACCENT_POSITION
    ? { tint: 'accent', base: 'surface', share: 22 + (78 * position) / ACCENT_POSITION }
    : {
        tint: 'text',
        base: 'accent',
        share: (55 * (position - ACCENT_POSITION)) / (1 - ACCENT_POSITION),
      };
}

const themeVariables: Record<ThemeColor, string> = {
  surface: '--theme-surface',
  accent: '--theme-accent',
  text: '--theme-text-primary',
};

export function rateMix(rate: number): string {
  const { tint, base, share } = rateBlend(rate);
  return `color-mix(in srgb, var(${themeVariables[tint]}) ${share.toFixed(1)}%, var(${themeVariables[base]}))`;
}

export function rateGradient(): string {
  return `linear-gradient(to right, ${rateMix(1)}, var(--theme-accent) ${String(ACCENT_POSITION * 100)}%, ${rateMix(RATE_CEILING)})`;
}

const wholeRate = new Intl.NumberFormat(undefined, { maximumFractionDigits: 0 });
const fractionalRate = new Intl.NumberFormat(undefined, {
  minimumFractionDigits: 1,
  maximumFractionDigits: 1,
});
const bitcoin = new Intl.NumberFormat(undefined, {
  minimumFractionDigits: 4,
  maximumFractionDigits: 4,
});

export function formatTemplateRate(rate: number): string {
  return rate >= 100 ? wholeRate.format(rate) : fractionalRate.format(rate);
}

export function formatBitcoin(satoshis: number): string {
  return `${bitcoin.format(satoshis / 100_000_000)} BTC`;
}

export function shortHash(hash: string): string {
  return hash.length > 20 ? `${hash.slice(0, 10)}…${hash.slice(-8)}` : hash;
}

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

// Squarified treemap that keeps the given order, so the highest-rate chunks
// fill from the top left. Areas are in units of width × height; space left
// over when the areas sum to less stays empty at the bottom right.
export function squarify(areas: readonly number[], width: number, height: number): Rect[] {
  const rects: Rect[] = [];
  let x = 0;
  let y = 0;
  let free = { width, height };
  let index = 0;
  while (index < areas.length && free.width > 1e-9 && free.height > 1e-9) {
    const side = Math.min(free.width, free.height);
    const row: number[] = [];
    let sum = 0;
    let worst = Infinity;
    let largest = 0;
    let smallest = Infinity;
    while (index < areas.length) {
      const area = Math.max(0, areas[index]);
      const total = sum + area;
      const high = Math.max(largest, area);
      const low = Math.min(smallest, area);
      const ratio =
        total === 0 || low === 0
          ? Infinity
          : Math.max((side * side * high) / (total * total), (total * total) / (side * side * low));
      if (row.length > 0 && ratio > worst) break;
      row.push(area);
      sum = total;
      worst = ratio;
      largest = high;
      smallest = low;
      index += 1;
    }

    if (free.width >= free.height) {
      const column = sum / free.height;
      let offset = y;
      for (const area of row) {
        const extent = column > 0 ? area / column : 0;
        rects.push({ x, y: offset, width: column, height: extent });
        offset += extent;
      }
      x += column;
      free = { width: free.width - column, height: free.height };
    } else {
      const band = sum / free.width;
      let offset = x;
      for (const area of row) {
        const extent = band > 0 ? area / band : 0;
        rects.push({ x: offset, y, width: extent, height: band });
        offset += extent;
      }
      y += band;
      free = { width: free.width, height: free.height - band };
    }
  }

  while (rects.length < areas.length) rects.push({ x, y, width: 0, height: 0 });
  return rects;
}

export interface TemplateTile {
  transaction: number;
  rect: Rect;
}

// Tiles in mining order, scaled so a full template covers width × height.
export function templateTiles(
  pool: PoolTemplates,
  template: number,
  width: number,
  height: number
): TemplateTile[] {
  const order = (pool.templates.at(template)?.chunks ?? []).flatMap(
    (index) => pool.chunks[index].transactions
  );
  const scale = (width * height) / pool.capacity;
  const rects = squarify(
    order.map((index) => pool.transactions[index].size * scale),
    width,
    height
  );
  return order.map((transaction, index) => ({ transaction, rect: rects[index] }));
}
