import { type ReactNode, useEffect, useMemo, useState } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import {
  BlockchainClientProvider,
  ProviderSwitcherProvider,
  TurboProvider,
  createBlockchainClient,
  getAvailableProviders,
} from "@/lib/api/client";
import { BlockchainEventCacheUpdater } from "@/lib/hooks/useBlockchainData";
import { ScriptExplainerPreferenceProvider } from "./script-explainer-preference";
import { SavedInvestigationsProvider } from "./saved-investigations";

function readLocalStorage(key: string): string | null {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  return window_?.localStorage.getItem(key) ?? null;
}

function writeLocalStorage(key: string, value: string) {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  window_?.localStorage.setItem(key, value);
}

export function AppProviders({ children }: { children: ReactNode }) {
  const defaultProvider =
    import.meta.env.VITE_BLOCKCHAIN_PROVIDER ?? "libbitcoin";
  const availableProviders = getAvailableProviders();
  const [turbo, setTurbo] = useState(
    () => readLocalStorage("turboEnabled") === "true",
  );
  const [provider, setProvider] = useState(() => {
    const storedProvider = readLocalStorage("blockchainProvider");
    if (storedProvider && availableProviders.includes(storedProvider)) {
      return storedProvider;
    }
    return defaultProvider;
  });
  const [selectorVisible, setSelectorVisible] = useState(
    () => readLocalStorage("providerSelectorVisible") === "true",
  );
  const [queryClient] = useState(
    () =>
      new QueryClient({
        defaultOptions: {
          queries: {
            staleTime: 30_000,
            refetchOnWindowFocus: false,
            retry: 1,
          },
        },
      }),
  );
  const blockchainClient = useMemo(
    () => createBlockchainClient(provider, { turboEnabled: turbo }),
    [provider, turbo],
  );

  useEffect(() => {
    writeLocalStorage("turboEnabled", turbo ? "true" : "false");
  }, [turbo]);

  useEffect(() => {
    writeLocalStorage("blockchainProvider", provider);
  }, [provider]);

  useEffect(() => {
    writeLocalStorage(
      "providerSelectorVisible",
      selectorVisible ? "true" : "false",
    );
  }, [selectorVisible]);

  return (
    <ProviderSwitcherProvider
      provider={provider}
      setProvider={setProvider}
      availableProviders={availableProviders}
      selectorVisible={selectorVisible}
      setSelectorVisible={setSelectorVisible}
    >
      <TurboProvider turbo={turbo} setTurbo={setTurbo}>
        <BlockchainClientProvider client={blockchainClient}>
          <QueryClientProvider client={queryClient}>
            <BlockchainEventCacheUpdater />
            <SavedInvestigationsProvider>
              <ScriptExplainerPreferenceProvider>
                {children}
              </ScriptExplainerPreferenceProvider>
            </SavedInvestigationsProvider>
          </QueryClientProvider>
        </BlockchainClientProvider>
      </TurboProvider>
    </ProviderSwitcherProvider>
  );
}
