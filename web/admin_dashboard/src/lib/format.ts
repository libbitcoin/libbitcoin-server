import { getEventDefinition } from './admin-contract';

const integerFormatter = new Intl.NumberFormat(undefined, {
  maximumFractionDigits: 0,
});

function formatNumber(value: number, maximumFractionDigits = 2): string {
  return new Intl.NumberFormat(undefined, { maximumFractionDigits }).format(value);
}

// A hash shortened to its leading and trailing characters.
export function formatHash(hash: string, visible = 8): string {
  if (hash.length <= visible * 2) return hash;
  return `${hash.slice(0, visible)}…${hash.slice(-visible)}`;
}

export function formatInteger(value: number): string {
  return integerFormatter.format(value);
}

const scaledFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 1 });
const scales = [
  [1_000_000_000, 'G'],
  [1_000_000, 'M'],
  [1_000, 'k'],
] as const;

export function formatScaled(value: number, unit: string): string {
  const magnitude = Math.abs(value);
  for (const [divisor, prefix] of scales) {
    if (magnitude >= divisor) return `${scaledFormatter.format(value / divisor)} ${prefix}${unit}`;
  }
  return `${scaledFormatter.format(value)} ${unit}`;
}

export function formatIsoTimestamp(value: number): string | undefined {
  const date = new Date(value);
  return Number.isFinite(date.getTime()) ? date.toISOString() : undefined;
}

export function formatTimestamp(value: number, includeMilliseconds = true): string {
  const date = new Date(value);
  if (!Number.isFinite(date.getTime())) return 'Invalid time';
  const formatted = date.toLocaleTimeString(undefined, {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  });
  return includeMilliseconds
    ? `${formatted}.${String(date.getMilliseconds()).padStart(3, '0')}`
    : formatted;
}

export function formatElapsed(milliseconds: number): string {
  const totalSeconds = Math.max(0, Math.floor(milliseconds / 1_000));
  const days = Math.floor(totalSeconds / 86_400);
  const hours = Math.floor((totalSeconds % 86_400) / 3_600);
  const minutes = Math.floor((totalSeconds % 3_600) / 60);
  const seconds = totalSeconds % 60;

  if (days > 0) return `${String(days)}d ${String(hours)}h`;
  if (hours > 0) return `${String(hours)}h ${String(minutes)}m`;
  if (minutes > 0) return `${String(minutes)}m ${String(seconds)}s`;
  return `${String(seconds)}s`;
}

export function formatEventValue(event: number, value: number): string {
  const definition = getEventDefinition(event);
  switch (definition.valueKind) {
    case 'height': {
      return `height ${formatInteger(value)}`;
    }
    case 'seconds': {
      return `${formatNumber(value)} s`;
    }
    case 'milliseconds': {
      return `${formatNumber(value)} ms`;
    }
    case 'microseconds': {
      return `${formatNumber(value)} µs`;
    }
    case 'count': {
      return formatInteger(value);
    }
    case 'fee-rate': {
      return `${formatNumber(value / 1_000, 3)} sat/vB`;
    }
    default: {
      return String(value);
    }
  }
}

export function formatRate(value: number, unit = ''): string {
  const per = unit ? ` ${unit}/s` : '/s';
  if (value >= 100) return `${formatNumber(value, 0)}${per}`;
  if (value >= 10) return `${formatNumber(value, 1)}${per}`;
  return `${formatNumber(value, 2)}${per}`;
}

