const integerFormatter = new Intl.NumberFormat('en-US', { maximumFractionDigits: 0 });

export function formatInteger(value: number): string {
  return integerFormatter.format(value);
}

export function formatNumber(value?: number | null, options: Intl.NumberFormatOptions = {}) {
  if (value === null || value === undefined) return '—';
  const formatter = new Intl.NumberFormat('en-US', {
    maximumFractionDigits: 2,
    ...options,
  });
  return formatter.format(value);
}

export function formatHash(hash?: string | null, visible = 8) {
  if (!hash) return '—';
  if (hash.length <= visible * 2) return hash;
  return `${hash.slice(0, visible)}…${hash.slice(-visible)}`;
}

export function satsToBtc(value?: number | null) {
  if (value === null || value === undefined) return;
  return value / 100_000_000;
}

export function formatSats(value?: number | null) {
  if (value === null || value === undefined) return '—';
  return `${formatNumber(value)} sats`;
}

export function formatBtc(value?: number | null) {
  if (value === null || value === undefined) return '—';
  return `${formatNumber(value, { minimumFractionDigits: 8, maximumFractionDigits: 8 })} btc`;
}

export function formatBytes(value?: number | null) {
  if (value === null || value === undefined) return '—';
  if (value >= 1_000_000) {
    return `${formatNumber(value / 1_000_000, { maximumFractionDigits: 2 })} MB`;
  }
  if (value >= 1_000) {
    return `${formatNumber(value / 1_000, { maximumFractionDigits: 1 })} kB`;
  }
  return `${formatNumber(value, { maximumFractionDigits: 0 })} bytes`;
}

export function formatTimestamp(timestamp?: number | null) {
  if (!timestamp) return '—';
  const date = new Date(timestamp * 1000);
  return date.toLocaleString();
}

export function formatRelativeTime(timestamp?: number | null, now = Date.now()) {
  if (!timestamp) return '—';
  const formatter = new Intl.RelativeTimeFormat('en', { numeric: 'auto' });
  const diffSeconds = Math.round((timestamp * 1000 - now) / 1000);
  const absSeconds = Math.abs(diffSeconds);
  if (absSeconds < 60) {
    return formatter.format(diffSeconds, 'second');
  }
  const diffMinutes = Math.round(diffSeconds / 60);
  if (Math.abs(diffMinutes) < 60) {
    return formatter.format(diffMinutes, 'minute');
  }
  const diffHours = Math.round(diffMinutes / 60);
  if (Math.abs(diffHours) < 24) {
    return formatter.format(diffHours, 'hour');
  }
  const diffDays = Math.round(diffHours / 24);
  return formatter.format(diffDays, 'day');
}

// The age of a timestamp in short units, as "38 mins ago" or "in 2 hrs".
export function formatShortRelativeTime(timestamp?: number | null, now = Date.now()) {
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

export function formatHex(value?: number | null) {
  if (value === null || value === undefined) return '—';
  return `0x${value.toString(16)}`;
}
