/** The http status of a failed request, if any. */
export function errorStatus(error: unknown): number | undefined {
  if (!(error instanceof Error) || !('status' in error)) return undefined;
  return typeof error.status === 'number' ? error.status : undefined;
}

/** The server is configured without address queries (no address index). */
export class AddressQueriesDisabledError extends Error {
  constructor() {
    super('Address queries are disabled by server configuration.');
    this.name = 'AddressQueriesDisabledError';
  }
}
