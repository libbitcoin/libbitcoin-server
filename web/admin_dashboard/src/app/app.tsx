import { useMemo, useState, useSyncExternalStore } from 'react';
import { Settings } from '@/components/settings';
import { EventView } from '@/components/event-view';
import { LogView } from '@/components/log-view';
import { Mining } from '@/components/mining';
import { Overview } from '@/components/overview';
import { SyncStatusNotifier } from '@/components/sync-status-notifier';
import {
  loadBrowserNotificationsEnabled,
  saveBrowserNotificationsEnabled,
} from '@/lib/browser-notifications';
import { loadEndpointConfiguration } from '@/lib/endpoints';
import { WorkerMetricsStore } from '@/lib/worker-metrics-store';
import { loadLogFilter, saveLogFilter } from '@/lib/log-settings';
import { chainSync } from '@/lib/sync-status';
import { useAdminTelemetry, useNativeTelemetry } from '@/lib/use-telemetry';
import { useCurrentTime } from '@/lib/use-current-time';
import { coinOf } from '@/lib/wallet-prefixes';

type DashboardView = 'overview' | 'logs' | 'events' | 'mining';

const navigation: Array<{ id: DashboardView; label: string }> = [
  { id: 'overview', label: 'Overview' },
  { id: 'logs', label: 'Logs' },
  { id: 'events', label: 'Events' },
  { id: 'mining', label: 'Mining' },
];

export function App() {
  const metrics = useMemo(() => new WorkerMetricsStore(), []);
  const now = useCurrentTime();
  const [endpoints] = useState(loadEndpointConfiguration);
  const [view, setView] = useState<DashboardView>('overview');
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [logFilter, setLogFilter] = useState(loadLogFilter);
  const [logsPaused, setLogsPaused] = useState(false);
  const [eventsPaused, setEventsPaused] = useState(false);
  const [browserNotificationsEnabled, setBrowserNotificationsEnabled] = useState(
    loadBrowserNotificationsEnabled
  );
  const snapshot = useSyncExternalStore(
    metrics.subscribe,
    metrics.getSnapshot,
    metrics.getSnapshot
  );
  const adminStatus = useAdminTelemetry(
    endpoints.adminUrl,
    endpoints.nativeUrl,
    metrics,
    logFilter
  );
  const native = useNativeTelemetry(endpoints.nativeUrl, metrics, now);
  const network = native.configuration
    ? `${coinOf(native.configuration.forks)} ${native.configuration.node.network}`
    : ' ';
  const sync = chainSync(
    native.tops,
    native.branch,
    snapshot.syncFill,
    native.receivedAt,
    native.topHeader,
    now
  );

  function updateBrowserNotifications(enabled: boolean) {
    saveBrowserNotificationsEnabled(enabled);
    setBrowserNotificationsEnabled(enabled);
  }

  function updateLogFilter(filter: number) {
    saveLogFilter(filter);
    setLogFilter(filter);
  }

  return (
    <div className="bg-background text-textPrimary min-h-screen">
      <header className="border-accent/30 bg-surface/90 border-b backdrop-blur">
        <div className="mx-auto max-w-7xl px-4 py-4">
          <div className="flex flex-wrap items-center gap-x-2 gap-y-4">
            <div className="order-1 flex flex-col">
              <button
                type="button"
                onClick={() => {
                  setView('overview');
                }}
                className="text-accent text-left text-lg font-semibold tracking-tight"
              >
                Libbitcoin Server
              </button>
              <span className="text-textMuted text-sm">
                {network}
              </span>
            </div>

            <nav className="text-textMuted order-3 flex w-full items-center gap-3 text-sm md:order-2 md:w-auto">
              {navigation.map((item) => (
                <button
                  key={item.id}
                  type="button"
                  onClick={() => {
                    setView(item.id);
                  }}
                  className={`text-textMuted hover:text-accent rounded-md px-3 py-1 text-sm transition ${
                    view === item.id ? 'bg-accentSecondary/20 text-accent' : ''
                  }`}
                >
                  {item.label}
                  {item.id === 'logs' && snapshot.totalLogs > 0 && (
                    <span className="bg-background/60 ml-2 rounded px-1.5 py-0.5 text-[10px] tabular-nums">
                      {snapshot.totalLogs > 999 ? '999+' : String(snapshot.totalLogs)}
                    </span>
                  )}
                  {item.id === 'events' && snapshot.totalEvents > 0 && (
                    <span className="bg-background/60 ml-2 rounded px-1.5 py-0.5 text-[10px] tabular-nums">
                      {snapshot.totalEvents > 999 ? '999+' : String(snapshot.totalEvents)}
                    </span>
                  )}
                </button>
              ))}
            </nav>

            <div className="order-2 ml-auto flex items-center gap-3 md:order-3">
              <button
                type="button"
                onClick={() => {
                  setSettingsOpen(true);
                }}
                className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
              >
                Settings
              </button>
            </div>
          </div>
        </div>
      </header>

      <main className="mx-auto min-h-[calc(100vh-4rem)] max-w-7xl min-w-0 space-y-8 px-4 py-8">
        <header className="space-y-2">
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Administration</p>
          <div className="flex min-h-10 flex-wrap items-end justify-between gap-4">
            <h1 className="text-accent text-3xl font-semibold">Admin Dashboard</h1>
          </div>
        </header>
        {view === 'overview' && (
          <Overview
            adminStatus={adminStatus}
            endpoints={endpoints}
            metrics={snapshot}
            native={native}
            sync={sync}
          />
        )}
        {view === 'logs' && (
          <LogView
            metrics={metrics}
            delivery={snapshot}
            status={adminStatus}
            logFilter={logFilter}
            paused={logsPaused}
            onLogFilterChange={updateLogFilter}
            onPausedChange={(paused) => {
              metrics.setLogsPaused(paused);
              setLogsPaused(paused);
            }}
            onClear={() => {
              metrics.clearLogs();
            }}
          />
        )}
        {view === 'events' && (
          <EventView
            metrics={metrics}
            paused={eventsPaused}
            onPausedChange={(paused) => {
              metrics.setEventsPaused(paused);
              setEventsPaused(paused);
            }}
            onClear={() => {
              metrics.clearEvents();
            }}
          />
        )}
        {view === 'mining' && <Mining />}
      </main>

      <SyncStatusNotifier
        sync={sync}
        browserNotificationsEnabled={browserNotificationsEnabled}
      />

      {settingsOpen && (
        <Settings
          browserNotificationsEnabled={browserNotificationsEnabled}
          onClose={() => {
            setSettingsOpen(false);
          }}
          onBrowserNotificationsEnabledChange={updateBrowserNotifications}
        />
      )}
    </div>
  );
}

