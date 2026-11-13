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

export function formatHex(value?: number | null) {
  if (value === null || value === undefined) return '—';
  return `0x${value.toString(16)}`;
}
