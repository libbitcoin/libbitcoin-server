import type { AdvertisedService } from '@/lib/api/client';

// The first advertised url of the service with the scheme of the page, as a
// websocket url (ws on an http port, wss on an https port).
export function serviceSocketUrl(
  service: AdvertisedService | undefined,
  protocol: string = globalThis.location?.protocol ?? 'http:'
): string | undefined {
  const scheme = protocol === 'https:' ? 'https://' : 'http://';
  const url = service?.advertise.find((value) => value.toLowerCase().startsWith(scheme));
  return url && url.replace(/^http/i, 'ws');
}
