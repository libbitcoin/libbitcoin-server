import { useEffect, useMemo, useRef, useState, useSyncExternalStore } from 'react';
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
import { HEADER_ACTION } from '@/lib/styles';
import { explorerUrl } from '@/lib/native-contract';
import { showBrowserNotification } from '@/lib/browser-notifications';

type DashboardView = 'overview' | 'logs' | 'events' | 'mining';

const navigation: Array<{ id: DashboardView; label: string }> = [
  { id: 'mining', label: 'Mining' },
  { id: 'logs', label: 'Logs' },
  { id: 'events', label: 'Events' },
];

const NAVIGATION_ITEM = 'hover:text-accent rounded-md px-3 py-1 text-sm transition';

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
  const explorer = explorerUrl(native.configuration);
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

  const faults = snapshot.unclearedFaults;

  // A block submitted since the mining page was last shown.
  const mining = useSyncExternalStore(metrics.subscribeMining, metrics.getMining, metrics.getMining);
  const latestBlock = mining?.blocks[0];
  const [seenBlock, setSeenBlock] = useState<string>();
  if (view === 'mining' && latestBlock && latestBlock.hash !== seenBlock) setSeenBlock(latestBlock.hash);
  const submitted = latestBlock !== undefined && latestBlock.hash !== seenBlock;

  // A new block is announced by the browser, where enabled.
  const notifiedBlock = useRef<string | null>(null);
  useEffect(() => {
    if (!mining) return;
    const hash = latestBlock?.hash ?? '';
    const previous = notifiedBlock.current;
    notifiedBlock.current = hash;
    if (previous === null || hash === previous || !latestBlock || !browserNotificationsEnabled) return;
    showBrowserNotification('Block submitted', {
      body: `Block ${String(latestBlock.height)} was submitted by a miner.`,
      tag: 'libbitcoin-block',
    });
  }, [mining, latestBlock, browserNotificationsEnabled]);

  // A new fault is announced by the browser, where enabled.
  const notifiedFaults = useRef(snapshot.totalFaults);
  useEffect(() => {
    const total = snapshot.totalFaults;
    const previous = notifiedFaults.current;
    notifiedFaults.current = total;
    if (total <= previous || !browserNotificationsEnabled) return;
    showBrowserNotification('Node fault', {
      body: `${String(faults)} fault${faults === 1 ? '' : 's'} not cleared.`,
      tag: 'libbitcoin-fault',
    });
  }, [snapshot.totalFaults, faults, browserNotificationsEnabled]);

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
              <a
                href="/"
                onClick={(event) => {
                  event.preventDefault();
                  setView('overview');
                }}
                className="text-accent text-lg font-semibold tracking-tight"
              >
                Libbitcoin Admin
              </a>
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
                  className={`${NAVIGATION_ITEM} ${view === item.id ? 'bg-accentSecondary/20' : ''} ${
                    item.id === 'logs' && faults > 0
                      ? 'text-red-300'
                      : item.id === 'mining' && submitted
                        ? 'text-emerald-300'
                        : view === item.id
                        ? 'text-accent'
                        : 'text-textMuted'
                  }`}
                  title={item.id === 'logs' && faults > 0 ? `${String(faults)} fault${faults === 1 ? '' : 's'} not cleared` : undefined}
                >
                  {item.label}
                </button>
              ))}
              {explorer ? (
                <a href={`${explorer}/v1`} target="_blank" rel="noopener noreferrer" className={`${NAVIGATION_ITEM} text-textMuted`}>
                  Explorer
                </a>
              ) : (
                <span className={`${NAVIGATION_ITEM} text-textMuted cursor-not-allowed opacity-50`} title="The native site is not advertised">
                  Explorer
                </span>
              )}
            </nav>

            <div className="order-2 ml-auto flex items-center gap-3 md:order-3">
              <button
                type="button"
                onClick={() => {
                  setSettingsOpen(true);
                }}
                className={HEADER_ACTION}
              >
                Settings
              </button>
            </div>
          </div>
        </div>
      </header>
      <div className="h-0.5" aria-hidden />

      <main className="mx-auto min-h-[calc(100vh-4rem)] max-w-7xl min-w-0 space-y-8 px-4 py-8">
        {view === 'overview' && (
          <Overview
            adminStatus={adminStatus}
            endpoints={endpoints}
            explorer={explorer}
            metrics={snapshot}
            native={native}
            sync={sync}
          />
        )}
        {view === 'logs' && (
          <LogView
            metrics={metrics}
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
            faults={faults}
            clearedFaultSequence={snapshot.clearedFaultSequence}
            onClearFaults={() => {
              metrics.clearFaults();
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
        {view === 'mining' && <Mining metrics={metrics} />}
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

