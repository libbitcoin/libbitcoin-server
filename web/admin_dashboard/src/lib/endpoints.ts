export interface EndpointConfiguration {
  adminUrl: string;
  nativeUrl: string;
}

const STORAGE_KEY = 'libbitcoinAdminDashboardEndpoints';

function browserStorage(): Storage | null {
  try {
    const window_ = (globalThis as unknown as { window?: Window }).window;
    return window_?.localStorage ?? null;
  } catch {
    return null;
  }
}

function browserOrigin(): string {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  return window_?.location.origin ?? 'http://localhost';
}

export function normalizeEndpointUrl(value: string): string {
  const trimmed = value.trim();
  if (!trimmed) throw new Error('Endpoint URL is required');

  const withScheme = /^[a-z][a-z\d+\-.]*:\/\//iu.test(trimmed) ? trimmed : `http://${trimmed}`;
  const url = new URL(withScheme);
  if (url.protocol !== 'http:' && url.protocol !== 'https:') {
    throw new Error('Endpoint must use HTTP or HTTPS');
  }
  url.pathname = url.pathname.replace(/\/+$/u, '') || '/';
  url.search = '';
  url.hash = '';
  return url.toString().replace(/\/$/u, '');
}

export function toWebSocketUrl(baseUrl: string): string {
  const url = new URL(normalizeEndpointUrl(baseUrl));
  url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
  url.pathname = '/';
  url.search = '';
  url.hash = '';
  return url.toString();
}

export function isSameOriginEndpoint(baseUrl: string, pageOrigin = browserOrigin()): boolean {
  return new URL(normalizeEndpointUrl(baseUrl)).origin === new URL(pageOrigin).origin;
}

export function defaultEndpointConfiguration(): EndpointConfiguration {
  return {
    adminUrl: normalizeEndpointUrl(import.meta.env.VITE_ADMIN_API_URL ?? browserOrigin()),
    nativeUrl: normalizeEndpointUrl(import.meta.env.VITE_NATIVE_API_URL ?? browserOrigin()),
  };
}

export function loadEndpointConfiguration(
  storage: Storage | null = browserStorage()
): EndpointConfiguration {
  const defaults = defaultEndpointConfiguration();
  try {
    const stored = storage?.getItem(STORAGE_KEY);
    if (!stored) return defaults;
    const value: unknown = JSON.parse(stored) as unknown;
    if (!value || typeof value !== 'object' || Array.isArray(value)) return defaults;
    const record = value as Record<string, unknown>;
    return {
      adminUrl:
        typeof record.adminUrl === 'string'
          ? normalizeEndpointUrl(record.adminUrl)
          : defaults.adminUrl,
      nativeUrl:
        typeof record.nativeUrl === 'string'
          ? normalizeEndpointUrl(record.nativeUrl)
          : defaults.nativeUrl,
    };
  } catch {
    return defaults;
  }
}

export function saveEndpointConfiguration(
  configuration: EndpointConfiguration,
  storage: Storage | null = browserStorage()
): void {
  const normalized = {
    adminUrl: normalizeEndpointUrl(configuration.adminUrl),
    nativeUrl: normalizeEndpointUrl(configuration.nativeUrl),
  };
  try {
    storage?.setItem(STORAGE_KEY, JSON.stringify(normalized));
  } catch {
    // Storage may be disabled or full; the in-memory configuration remains usable.
  }
}

export function clearEndpointConfiguration(storage: Storage | null = browserStorage()): void {
  try {
    storage?.removeItem(STORAGE_KEY);
  } catch {
    // Storage may be disabled; callers can still restore the in-memory defaults.
  }
}
