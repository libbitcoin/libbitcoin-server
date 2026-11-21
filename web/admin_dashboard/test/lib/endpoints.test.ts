import { expect, test } from 'vitest';
import {
  isSameOriginEndpoint,
  normalizeEndpointUrl,
  toWebSocketUrl,
} from '@/lib/endpoints';

test('endpoints__normalize_endpoint_url__empty__throws', () => {
  expect(() => normalizeEndpointUrl('  ')).toThrow('Endpoint URL is required');
});

test('endpoints__normalize_endpoint_url__no_scheme__http', () => {
  expect(normalizeEndpointUrl('localhost:8080')).toBe('http://localhost:8080');
});

test('endpoints__normalize_endpoint_url__https__https', () => {
  expect(normalizeEndpointUrl('https://example.com')).toBe('https://example.com');
});

test('endpoints__normalize_endpoint_url__other_scheme__throws', () => {
  expect(() => normalizeEndpointUrl('ftp://example.com')).toThrow('Endpoint must use HTTP or HTTPS');
});

test('endpoints__normalize_endpoint_url__trailing_slashes_query_fragment__removed', () => {
  expect(normalizeEndpointUrl(' http://example.com/base//?a=1#b ')).toBe('http://example.com/base');
});

test('endpoints__to_web_socket_url__http_path__ws_root', () => {
  expect(toWebSocketUrl('http://example.com:8080/base')).toBe('ws://example.com:8080/');
});

test('endpoints__to_web_socket_url__https__wss_root', () => {
  expect(toWebSocketUrl('https://example.com')).toBe('wss://example.com/');
});

test('endpoints__is_same_origin_endpoint__same_origin__true', () => {
  expect(isSameOriginEndpoint('localhost:8080/base', 'http://localhost:8080')).toBe(true);
});

test('endpoints__is_same_origin_endpoint__other_port__false', () => {
  expect(isSameOriginEndpoint('localhost:8180', 'http://localhost:8080')).toBe(false);
});
