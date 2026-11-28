import { formatHashrate } from '@/lib/mining/format';

// A round step of about a third of the value, for the gridlines.
export function gridStep(maximum: number): number {
  const rough = maximum / 3;
  const power = 10 ** Math.floor(Math.log10(rough));
  const scaled = rough / power;
  return (scaled <= 1 ? 1 : scaled <= 2 ? 2 : scaled <= 5 ? 5 : 10) * power;
}

// An axis rate, without trailing zeros (6 MH/s, 2.5 MH/s).
export function axisRate(rate: number): string {
  return formatHashrate(rate).replace(/(\.\d*?)0+ /, '$1 ').replace('. ', ' ');
}

// A time before now, in seconds, as an axis label.
export function ago(seconds: number): string {
  if (seconds <= 0) return 'now';
  return seconds < 60 ? `−${String(seconds)} s` : `−${String(seconds / 60)} min`;
}
