import { expect, test } from 'vitest';
import { serviceSocketUrl } from '@/lib/utils/services';

const service = { advertise: ['https://pool.example.com:8843', 'http://pool.example.com:8880', 'http://other.example.com:80'] };

test('services__service_socket_url__undefined__undefined', () => {
  expect(serviceSocketUrl(undefined, 'http:')).toBeUndefined();
});

test('services__service_socket_url__http_page__first_http_as_ws', () => {
  expect(serviceSocketUrl(service, 'http:')).toBe('ws://pool.example.com:8880');
});

test('services__service_socket_url__https_page__https_as_wss', () => {
  expect(serviceSocketUrl(service, 'https:')).toBe('wss://pool.example.com:8843');
});

test('services__service_socket_url__no_matching_scheme__undefined', () => {
  expect(serviceSocketUrl({ advertise: ['http://pool.example.com:8880'] }, 'https:')).toBeUndefined();
});
