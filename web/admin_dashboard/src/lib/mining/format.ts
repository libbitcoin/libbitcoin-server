const UNITS = ['H/s', 'kH/s', 'MH/s', 'GH/s', 'TH/s', 'PH/s', 'EH/s'];

export function formatHashrate(rate: number): string {
  if (!(rate > 0)) return '0 H/s';
  let value = rate;
  let unit = 0;
  while (value >= 1000 && unit < UNITS.length - 1) {
    value /= 1000;
    unit++;
  }
  return `${value.toFixed(value < 10 && unit > 0 ? 2 : value < 100 && unit > 0 ? 1 : 0)} ${UNITS[unit]}`;
}

export function formatDifficulty(difficulty?: number): string {
  if (difficulty === undefined) return '—';
  if (difficulty >= 1e6) {
    return new Intl.NumberFormat(undefined, { notation: 'compact', maximumFractionDigits: 2 }).format(
      difficulty
    );
  }
  if (difficulty >= 1) return new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 }).format(difficulty);
  return difficulty.toPrecision(3);
}

// A duration in seconds, in its largest sensible unit.
export function formatDuration(seconds: number): string {
  if (!Number.isFinite(seconds)) return '—';
  if (seconds < 1) return 'under a second';
  const steps: [number, string][] = [
    [60, 'second'],
    [60, 'minute'],
    [24, 'hour'],
    [365.25, 'day'],
    [Infinity, 'year'],
  ];
  let value = seconds;
  for (const [size, unit] of steps) {
    if (value < size) {
      const rounded = value < 10 ? Math.round(value * 10) / 10 : Math.round(value);
      const text = new Intl.NumberFormat(undefined, { maximumFractionDigits: 1 }).format(rounded);
      return `${text} ${unit}${rounded === 1 ? '' : 's'}`;
    }
    value /= size;
  }
  return '—';
}

// A short time relative to now, for a timestamp in seconds.
export function formatShortRelativeTime(timestamp?: number | null, now = Date.now()): string {
  if (!timestamp) return '—';
  const seconds = Math.round((timestamp * 1000 - now) / 1000);
  const size = Math.abs(seconds);
  const [count, unit] =
    size < 60
      ? [size, 'sec']
      : size < 3600
        ? [Math.round(size / 60), 'min']
        : size < 86_400
          ? [Math.round(size / 3600), 'hr']
          : [Math.round(size / 86_400), 'day'];
  const text = `${String(count)} ${unit}${count === 1 ? '' : 's'}`;
  return seconds > 0 ? `in ${text}` : `${text} ago`;
}
