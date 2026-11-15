export interface EndpointConfiguration {
  adminUrl: string;
  nativeUrl: string;
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

export function loadEndpointConfiguration(): EndpointConfiguration {
  try {
    // Discard endpoint overrides saved by older dashboards without reading them.
    globalThis.localStorage.removeItem('libbitcoinAdminDashboardEndpoints');
  } catch {
    // Endpoint selection does not depend on browser storage being available.
  }

  return {
    adminUrl: normalizeEndpointUrl(import.meta.env.VITE_ADMIN_API_URL ?? browserOrigin()),
    nativeUrl: normalizeEndpointUrl(import.meta.env.VITE_NATIVE_API_URL ?? browserOrigin()),
  };
}
