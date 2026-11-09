import { useEffect } from 'react';
import { NavLink } from 'react-router-dom';
import { SearchBar } from '@/components/search-bar';
import { StatusPill } from '@/components/status-pill';
import { CommandPalette } from '@/components/command-palette';
import { RouteContent } from './route-navigation';
import { useProviderInfo, useProviderSwitcher, useTurbo } from '@/lib/api/client';
import { useProviderConfiguration } from '@/lib/hooks/useBlockchainData';
import { blocksPath, dashboardPath, investigationsPath } from '@/lib/utils/routes';

function formatEndpointLabel(endpoint: string) {
  try {
    const url = new URL(endpoint);
    const path = url.pathname.replace(/\/$/u, '');
    if (path && path !== '/') return `${url.host}${path}`;
    return url.host;
  } catch {
    return endpoint;
  }
}

export function AppShell() {
  const info = useProviderInfo();
  const { provider, setProvider, availableProviders, selectorVisible } = useProviderSwitcher();
  const { turbo, setTurbo } = useTurbo();
  const { data: configuration } = useProviderConfiguration();
  const showTurbo =
    provider === 'libbitcoin' && (!configuration || (configuration.turbo && configuration.address));
  const endpointLabel = info.endpoint ? formatEndpointLabel(info.endpoint) : info.label;
  const statusTitle =
    info.endpoint && info.description
      ? `${info.endpoint} — ${info.description}`
      : (info.endpoint ?? info.description);

  useEffect(() => {
    if (provider !== 'libbitcoin') return;
    if (!configuration) return;
    if (!configuration.address || !configuration.turbo) {
      setTurbo(false);
    }
  }, [configuration, provider, setTurbo]);

  return (
    <div className="bg-background text-textPrimary min-h-screen">
      <header className="border-accent/30 bg-surface/90 border-b backdrop-blur">
        <div className="mx-auto max-w-7xl px-4 py-4">
          <div className="flex flex-wrap items-center gap-4">
            <NavLink
              to={dashboardPath()}
              className="text-accent order-1 text-lg font-semibold tracking-tight"
            >
              Libbitcoin Server
            </NavLink>
            <nav className="text-textMuted order-3 flex w-full items-center gap-3 text-sm md:order-2 md:w-auto">
              <NavLink
                to={dashboardPath()}
                className={({ isActive }) =>
                  `text-textMuted hover:text-accent rounded-md px-3 py-1 transition ${
                    isActive ? 'bg-accentSecondary/20 text-accent' : ''
                  }`
                }
                end
              >
                Dashboard
              </NavLink>
              <NavLink
                to={blocksPath()}
                className={({ isActive }) =>
                  `text-textMuted hover:text-accent rounded-md px-3 py-1 transition ${
                    isActive ? 'bg-accentSecondary/20 text-accent' : ''
                  }`
                }
              >
                Blocks
              </NavLink>
              <NavLink
                to={investigationsPath()}
                className={({ isActive }) =>
                  `text-textMuted hover:text-accent rounded-md px-3 py-1 transition ${isActive ? 'bg-accentSecondary/20 text-accent' : ''}`
                }
              >
                Investigations
              </NavLink>
            </nav>
            <div className="order-2 min-w-[12rem] flex-1 md:order-4 md:flex-none">
              <SearchBar />
            </div>
            <div className="order-3 flex w-full flex-wrap items-center gap-3 md:order-3 md:ml-auto md:w-auto md:flex-nowrap">
              <StatusPill
                label={endpointLabel}
                status={info.connectionState}
                title={statusTitle}
                labelClassName={info.endpoint ? 'font-mono normal-case' : undefined}
              />
              {showTurbo && (
                <label className="text-textMuted flex items-center gap-2 text-xs tracking-wide uppercase">
                  <input
                    type="checkbox"
                    className="border-accent/50 bg-surface/80 text-accent focus:ring-accent h-4 w-4 rounded"
                    checked={turbo}
                    onChange={(event) => {
                      setTurbo(event.target.checked);
                    }}
                  />
                  Turbo
                </label>
              )}
              {selectorVisible && (
                <select
                  value={provider}
                  onChange={(event) => {
                    setProvider(event.target.value);
                  }}
                  className="border-accent/40 bg-surface/80 text-textPrimary hover:border-accent focus:border-accent rounded-md border px-2 py-1 text-xs tracking-wide uppercase focus:outline-none"
                  title="Switch data provider"
                >
                  {availableProviders.map((p) => {
                    const label = p === 'libbitcoin' ? 'server' : p;
                    return (
                      <option key={p} value={p}>
                        {label}
                      </option>
                    );
                  })}
                </select>
              )}
            </div>
          </div>
        </div>
      </header>
      <RouteContent />
      <CommandPalette />
    </div>
  );
}
