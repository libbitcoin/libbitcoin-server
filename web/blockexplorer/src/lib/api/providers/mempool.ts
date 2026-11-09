import type { BlockchainClient } from '../client';
import { createBlockstreamClient } from './blockstream';

interface MempoolClientOptions {
  baseUrl: string;
}

export function createMempoolClient({ baseUrl }: MempoolClientOptions): BlockchainClient {
  const client = createBlockstreamClient({ baseUrl });
  return {
    ...client,
    metadata: {
      ...client.metadata,
      id: 'mempool',
      label: 'mempool.space',
      endpoint: baseUrl,
      description: 'Adapter backed by mempool.space REST API.',
    },
  };
}
