import { getEventDefinition } from './admin-contract';

const numberFormatter = new Intl.NumberFormat(undefined, {
  maximumFractionDigits: 2,
});

const integerFormatter = new Intl.NumberFormat(undefined, {
  maximumFractionDigits: 0,
});

export function formatNumber(value: number, maximumFractionDigits = 2): string {
  return new Intl.NumberFormat(undefined, { maximumFractionDigits }).format(value);
}

export function formatInteger(value: number): string {
  return integerFormatter.format(value);
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

export function formatDateTime(value: number): string {
  const date = new Date(value);
  if (!Number.isFinite(date.getTime())) return 'Invalid time';
  return date.toLocaleString();
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

export function formatDuration(milliseconds: number): string {
  if (milliseconds < 1) return `${numberFormatter.format(milliseconds * 1_000)} µs`;
  if (milliseconds < 1_000) return `${numberFormatter.format(milliseconds)} ms`;
  return `${numberFormatter.format(milliseconds / 1_000)} s`;
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

export function formatRate(value: number): string {
  if (value >= 100) return `${formatNumber(value, 0)}/s`;
  if (value >= 10) return `${formatNumber(value, 1)}/s`;
  return `${formatNumber(value, 2)}/s`;
}

export function compactEndpoint(endpoint: string): string {
  try {
    const url = new URL(endpoint);
    const path = url.pathname === '/' ? '' : url.pathname.replace(/\/$/u, '');
    return `${url.host}${path}`;
  } catch {
    return endpoint;
  }
}
