import { type ReactNode, useEffect, useMemo, useState } from 'react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { BlockchainClientProvider, TurboProvider, createBlockchainClient } from '@/lib/api/client';
import { BlockchainEventCacheUpdater } from '@/lib/hooks/useBlockchainData';
import { SavedInvestigationsProvider } from './saved-investigations';
import { AddressQueriesDisabledError, errorStatus } from '@/lib/api/errors';

function readLocalStorage(key: string): string | null {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  return window_?.localStorage.getItem(key) ?? null;
}

function readTurboParameter(): boolean {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  const search = window_?.location.search ?? '';
  return new URLSearchParams(search).get('turbo') === 'true';
}

function writeLocalStorage(key: string, value: string) {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  window_?.localStorage.setItem(key, value);
}

export function AppProviders({ children }: { children: ReactNode }) {
  const [turbo, setTurbo] = useState(
    () => readTurboParameter() || readLocalStorage('turboEnabled') !== 'false'
  );
  const [queryClient] = useState(
    () =>
      new QueryClient({
        defaultOptions: {
          queries: {
            staleTime: 30_000,
            refetchOnWindowFocus: false,
            retry: (failures, error) =>
              failures < 1 &&
              !(error instanceof AddressQueriesDisabledError) &&
              errorStatus(error) !== 404,
          },
        },
      })
  );
  const blockchainClient = useMemo(() => createBlockchainClient({ turboEnabled: turbo }), [turbo]);

  useEffect(() => {
    writeLocalStorage('turboEnabled', turbo ? 'true' : 'false');
  }, [turbo]);

  return (
    <TurboProvider turbo={turbo} setTurbo={setTurbo}>
      <BlockchainClientProvider client={blockchainClient}>
        <QueryClientProvider client={queryClient}>
          <BlockchainEventCacheUpdater />
          <SavedInvestigationsProvider>
            {children}
          </SavedInvestigationsProvider>
        </QueryClientProvider>
      </BlockchainClientProvider>
    </TurboProvider>
  );
}
