/** The server is configured without address queries (no address index). */
export class AddressQueriesDisabledError extends Error {
  constructor() {
    super('Address queries are disabled by server configuration.');
    this.name = 'AddressQueriesDisabledError';
  }
}
