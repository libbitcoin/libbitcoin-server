import { useMemo, useState, useSyncExternalStore } from 'react';
import { EndpointSettings } from '@/components/endpoint-settings';
import { EventView } from '@/components/event-view';
import { LogView } from '@/components/log-view';
import { Overview } from '@/components/overview';
import { SyncStatusNotifier } from '@/components/sync-status-notifier';
import {
  loadBrowserNotificationsEnabled,
  saveBrowserNotificationsEnabled,
} from '@/lib/browser-notifications';
import {
  clearEndpointConfiguration,
  defaultEndpointConfiguration,
  loadEndpointConfiguration,
  saveEndpointConfiguration,
  type EndpointConfiguration,
} from '@/lib/endpoints';
import { WorkerMetricsStore } from '@/lib/worker-metrics-store';
import { loadLogFilter, saveLogFilter } from '@/lib/log-settings';
import { estimateSyncProgress } from '@/lib/sync-status';
import { useAdminTelemetry, useNativeTelemetry } from '@/lib/use-telemetry';
import { useCurrentTime } from '@/lib/use-current-time';

type DashboardView = 'overview' | 'logs' | 'events';

const navigation: Array<{ id: DashboardView; label: string }> = [
  { id: 'overview', label: 'Overview' },
  { id: 'logs', label: 'Logs' },
  { id: 'events', label: 'Events' },
];

export function App() {
  const metrics = useMemo(() => new WorkerMetricsStore(), []);
  const now = useCurrentTime();
  const [endpoints, setEndpoints] = useState(loadEndpointConfiguration);
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
  const adminStatus = useAdminTelemetry(endpoints.adminUrl, metrics, logFilter);
  const native = useNativeTelemetry(endpoints.nativeUrl, metrics, now);
  const syncEstimate = estimateSyncProgress(native.top, native.topHeader, now);

  function saveEndpoints(configuration: EndpointConfiguration) {
    saveEndpointConfiguration(configuration);
    if (
      configuration.adminUrl !== endpoints.adminUrl ||
      configuration.nativeUrl !== endpoints.nativeUrl
    ) {
      metrics.reset();
    }
    setEndpoints(configuration);
    setSettingsOpen(false);
  }

  function resetEndpoints() {
    clearEndpointConfiguration();
    const defaults = defaultEndpointConfiguration();
    if (defaults.adminUrl !== endpoints.adminUrl || defaults.nativeUrl !== endpoints.nativeUrl) {
      metrics.reset();
    }
    setEndpoints(defaults);
    setSettingsOpen(false);
  }

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
          <div className="flex flex-wrap items-center gap-4">
            <button
              type="button"
              onClick={() => {
                setView('overview');
              }}
              className="text-accent order-1 text-lg font-semibold tracking-tight"
            >
              Libbitcoin Server
            </button>

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
              <div className="hidden items-center gap-2 lg:flex" aria-label="Connection summary">
                <StatusDot connected={adminStatus.phase === 'connected'} label="Admin" />
                <StatusDot connected={native.transport !== 'none'} label="Native" />
              </div>
              <button
                type="button"
                onClick={() => {
                  setSettingsOpen(true);
                }}
                className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
              >
                Endpoints
              </button>
            </div>
          </div>
        </div>
      </header>

      <main className="mx-auto min-h-[calc(100vh-4rem)] max-w-7xl min-w-0 space-y-6 px-4 py-8">
        <header>
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Administration</p>
          <h1 className="text-accent text-3xl font-semibold">Admin Dashboard</h1>
        </header>
        {view === 'overview' && (
          <Overview
            adminStatus={adminStatus}
            endpoints={endpoints}
            metrics={snapshot}
            native={native}
            syncEstimate={syncEstimate}
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
      </main>

      <SyncStatusNotifier
        key={endpoints.nativeUrl}
        estimate={syncEstimate}
        browserNotificationsEnabled={browserNotificationsEnabled}
      />

      {settingsOpen && (
        <EndpointSettings
          key={`${endpoints.adminUrl}|${endpoints.nativeUrl}`}
          configuration={endpoints}
          browserNotificationsEnabled={browserNotificationsEnabled}
          onClose={() => {
            setSettingsOpen(false);
          }}
          onReset={resetEndpoints}
          onSave={saveEndpoints}
          onBrowserNotificationsEnabledChange={updateBrowserNotifications}
        />
      )}
    </div>
  );
}

function StatusDot({ connected, label }: { connected: boolean; label: string }) {
  return (
    <span className="text-textMuted inline-flex items-center gap-1.5 text-xs">
      <span
        className={`h-2 w-2 rounded-full ${connected ? 'bg-emerald-400' : 'bg-amber-400'}`}
        aria-hidden="true"
      />
      {label}
    </span>
  );
}
