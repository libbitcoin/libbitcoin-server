import { useLayoutEffect } from 'react';
import { NavLink, useLocation, useNavigate } from 'react-router-dom';
import { SearchBar } from '@/components/search-bar';
import { StatusPill } from '@/components/status-pill';
import { ThemeSelect } from '@/components/theme-select';
import { CommandPalette } from '@/components/command-palette';
import { RouteContent } from './route-navigation';
import { useServerInfo, useTurbo } from '@/lib/api/client';
import { useServerConfiguration } from '@/lib/hooks/useBlockchainData';
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
  const info = useServerInfo();
  const { turbo, setTurbo } = useTurbo();
  const { data: configuration } = useServerConfiguration();
  const location = useLocation();
  const navigate = useNavigate();
  const turboAvailable = configuration?.turbo === true;
  const turboActive = turbo && turboAvailable;
  const endpointLabel = info.endpoint ? formatEndpointLabel(info.endpoint) : info.label;
  const statusTitle =
    info.endpoint && info.description
      ? `${info.endpoint} — ${info.description}`
      : (info.endpoint ?? info.description);

  useLayoutEffect(() => {
    if (!configuration) return;
    const parameters = new URLSearchParams(location.search);
    if ((parameters.get('turbo') === 'true') === turboActive) return;
    if (turboActive) parameters.set('turbo', 'true');
    else parameters.delete('turbo');
    const search = parameters.toString();
    void navigate(
      { pathname: location.pathname, search: search ? `?${search}` : '', hash: location.hash },
      { replace: true, state: location.state as unknown }
    );
  }, [configuration, location, navigate, turboActive]);

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
              <ThemeSelect />
              <StatusPill
                label={endpointLabel}
                status={info.connectionState}
                title={statusTitle}
                labelClassName={info.endpoint ? 'font-mono normal-case' : undefined}
              />
              <label
                className={`text-textMuted flex items-center gap-2 text-xs tracking-wide uppercase ${
                  turboAvailable ? '' : 'opacity-50'
                }`}
                title={
                  configuration && !turboAvailable ? 'Turbo is disabled by the server' : undefined
                }
              >
                <input
                  type="checkbox"
                  className="border-accent/50 bg-surface/80 text-accent focus:ring-accent h-4 w-4 rounded disabled:cursor-not-allowed"
                  checked={turboActive}
                  disabled={!turboAvailable}
                  onChange={(event) => {
                    setTurbo(event.target.checked);
                  }}
                />
                Turbo
              </label>
            </div>
          </div>
        </div>
      </header>
      <RouteContent />
      <CommandPalette />
    </div>
  );
}
