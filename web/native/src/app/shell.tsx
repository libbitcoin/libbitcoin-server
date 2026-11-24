import { useLayoutEffect } from 'react';
import { NavLink, useLocation, useNavigate } from 'react-router-dom';
import { SearchBar } from '@/components/search-bar';
import { ThemeSelect } from '@/components/theme-select';
import { CommandPalette } from '@/components/command-palette';
import { RouteContent } from './route-navigation';
import { useTurbo } from '@/lib/api/client';
import { useServerConfiguration } from '@/lib/hooks/useBlockchainData';
import {
  blocksPath,
  dashboardPath,
  investigationsPath,
  isAddressPath,
  poolPath,
} from '@/lib/utils/routes';

export function AppShell() {
  const { turbo, setTurbo } = useTurbo();
  const { data: configuration } = useServerConfiguration();
  const location = useLocation();
  const navigate = useNavigate();
  const turboAvailable = configuration?.turbo === true;
  const turboActive = turbo && turboAvailable;
  const network = configuration
    ? `${configuration.coin} ${configuration.network ?? '[unknown]'}`
    : ' ';

  useLayoutEffect(() => {
    if (!configuration) return;
    const parameters = new URLSearchParams(location.search);
    const turboUrl = turboActive && isAddressPath(location.pathname);
    if ((parameters.get('turbo') === 'true') === turboUrl) return;
    if (turboUrl) parameters.set('turbo', 'true');
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
          <div className="flex flex-wrap items-center gap-x-2 gap-y-4">
            <div className="order-1 flex flex-col">
              <NavLink
                to={dashboardPath()}
                className="text-accent text-lg font-semibold tracking-tight"
              >
                Libbitcoin Server
              </NavLink>
              <span className="text-textMuted text-sm">{network}</span>
            </div>
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
                to={poolPath()}
                className={({ isActive }) =>
                  `text-textMuted hover:text-accent rounded-md px-3 py-1 transition ${
                    isActive ? 'bg-accentSecondary/20 text-accent' : ''
                  }`
                }
              >
                Pool
              </NavLink>
              <NavLink
                to={investigationsPath()}
                className={({ isActive }) =>
                  `text-textMuted hover:text-accent rounded-md px-3 py-1 transition ${isActive ? 'bg-accentSecondary/20 text-accent' : ''}`
                }
              >
                Saved
              </NavLink>
            </nav>
            <div className="order-2 min-w-[12rem] flex-1 md:order-4 md:flex-none">
              <SearchBar />
            </div>
            <div className="order-3 flex w-full flex-wrap items-center gap-3 md:order-3 md:ml-auto md:w-auto md:flex-nowrap">
              <ThemeSelect />
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
                  className="border-accent/50 bg-surface/80 checked:bg-accent text-accent focus:ring-accent h-4 w-4 rounded disabled:cursor-not-allowed"
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
