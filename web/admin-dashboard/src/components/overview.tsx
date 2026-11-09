import { getEventDefinition } from '@/lib/admin-contract';
import type { EndpointConfiguration } from '@/lib/endpoints';
import {
  compactEndpoint,
  formatElapsed,
  formatEventValue,
  formatInteger,
  formatRate,
  formatTimestamp,
} from '@/lib/format';
import { detectNodeActivity, type MetricsSnapshot } from '@/lib/live-metrics';
import type { ConnectionStatus } from '@/lib/reconnecting-websocket';
import { eventSignalId } from '@/lib/signal-catalog';
import { type SyncEstimate, SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS } from '@/lib/sync-status';
import type { NativeTelemetry } from '@/lib/use-telemetry';
import { ConnectionPill } from './connection-pill';
import { MetricItem } from './metric-item';
import { NodeSyncProgress } from './node-sync-progress';
import { TelemetryDashboard } from './telemetry-dashboard';

interface OverviewProps {
  adminStatus: ConnectionStatus;
  endpoints: EndpointConfiguration;
  metrics: MetricsSnapshot;
  native: NativeTelemetry;
  syncEstimate: SyncEstimate | null;
}
const lifecycleEvents: ReadonlyArray<{ event: number; label: string }> = [
  { event: 0, label: 'Header archived' },
  { event: 3, label: 'Block archived' },
  { event: 5, label: 'Block validated' },
  { event: 6, label: 'Block confirmed' },
  { event: 13, label: 'Block organized' },
  { event: 14, label: 'Block reorganized' },
];

export function Overview({ adminStatus, endpoints, metrics, native, syncEstimate }: OverviewProps) {
  const activity = detectNodeActivity(metrics, 10);
  const configuration = native.configuration;
  const confirmedSignal = eventSignalId(6);
  const warnings = [
    adminStatus.error ? `Admin: ${adminStatus.error}` : null,
    native.status.error ? `Native: ${native.status.error}` : null,
    native.transport === 'none' && native.restError ? `Native REST: ${native.restError}` : null,
    metrics.invalidFrames > 0
      ? `${String(metrics.invalidFrames)} malformed admin frame${metrics.invalidFrames === 1 ? '' : 's'} ignored`
      : null,
    native.live.invalidFrames > 0
      ? `${String(native.live.invalidFrames)} malformed native frame${native.live.invalidFrames === 1 ? '' : 's'} ignored`
      : null,
    native.status.phase === 'connected' && native.transport === 'none'
      ? 'Native: Connected, but no fresh valid state has been received'
      : null,
  ].filter((warning): warning is string => warning !== null);

  return (
    <div className="space-y-6">
      {warnings.length > 0 && (
        <section className="rounded-xl border border-amber-500/30 bg-amber-500/10 px-4 py-3">
          <p className="text-sm font-medium text-amber-300">Transport notice</p>
          <ul className="text-textMuted mt-1 space-y-1 text-xs">
            {warnings.map((warning) => (
              <li key={warning}>{warning}</li>
            ))}
          </ul>
        </section>
      )}

      <NodeSyncProgress confirmed={native.top} estimate={syncEstimate} />

      <dl aria-label="Node summary" className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
        <MetricItem
          label="Canonical top"
          value={native.top === null ? '—' : formatInteger(native.top)}
          detail={
            native.transport === 'none'
              ? 'Waiting for native API'
              : `${nativeSyncDescription(syncEstimate)} · via ${
                  native.transport === 'websocket' ? 'native WebSocket' : 'REST fallback'
                }`
          }
          emphasized
        />
        <MetricItem
          label="Current activity"
          value={activity.label}
          detail={
            activity.rate > 0
              ? `${formatRate(activity.rate)} over the last 10 seconds`
              : 'No lifecycle events in the last 10 seconds'
          }
          numeric={false}
        />
        <MetricItem
          label="Highest confirmed"
          value={
            metrics.signalMaximums[confirmedSignal] === undefined
              ? '—'
              : formatInteger(metrics.signalMaximums[confirmedSignal])
          }
          detail={`${formatInteger(metrics.signalTotals[confirmedSignal] ?? 0)} confirmations observed`}
        />
        <MetricItem
          label="Collection uptime"
          value={formatElapsed(metrics.now - metrics.sessionStartedAt)}
          detail={`${formatInteger(metrics.totalEvents)} events · ${formatInteger(metrics.totalLogs)} logs`}
        />
      </dl>

      <section className="grid gap-4 lg:grid-cols-2">
        <ConnectionCard
          title="Admin stream"
          endpoint={endpoints.adminUrl}
          status={adminStatus}
          detail={
            adminStatus.lastMessageAt
              ? `Last frame ${formatTimestamp(adminStatus.lastMessageAt)}`
              : 'Waiting for the first frame'
          }
        />
        <ConnectionCard
          title="Native interface"
          endpoint={endpoints.nativeUrl}
          status={native.status}
          detail={
            native.receivedAt
              ? `Last state ${formatTimestamp(native.receivedAt)} · ${native.transport}`
              : 'Waiting for node state'
          }
        />
      </section>

      <TelemetryDashboard metrics={metrics} />

      <section className="grid gap-4 xl:grid-cols-[1.1fr_0.9fr]">
        <div className="border-accent/30 bg-surface/80 overflow-hidden rounded-2xl border">
          <header className="border-accentSecondary/20 border-b px-5 py-4">
            <p className="text-accentSecondary text-xs tracking-wide uppercase">
              Lifecycle heights
            </p>
            <h2 className="text-accent mt-1 text-lg font-semibold">
              Highest observed event values
            </h2>
          </header>
          <div className="bg-accentSecondary/10 grid gap-px sm:grid-cols-2 lg:grid-cols-3">
            {lifecycleEvents.map(({ event, label }) => {
              const maximum = metrics.signalMaximums[eventSignalId(event)];
              return (
                <div key={event} className="bg-surface px-5 py-4">
                  <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
                  <p className="text-textPrimary mt-2 text-xl font-semibold tabular-nums">
                    {maximum === undefined ? '—' : formatInteger(maximum)}
                  </p>
                </div>
              );
            })}
          </div>
        </div>

        <div className="border-accent/30 bg-surface/80 rounded-2xl border">
          <header className="border-accentSecondary/20 border-b px-5 py-4">
            <p className="text-accentSecondary text-xs tracking-wide uppercase">Capabilities</p>
            <h2 className="text-accent mt-1 text-lg font-semibold">Native configuration</h2>
          </header>
          {configuration ? (
            <div className="grid grid-cols-2 gap-x-6 gap-y-4 p-5 text-sm">
              <Capability label="Address index" enabled={configuration.address} />
              <Capability label="Client filters" enabled={configuration.filter} />
              <Capability label="Turbo" enabled={configuration.turbo} />
              <Capability label="Witness" enabled={configuration.witness} />
              <Capability label="Retarget" enabled={configuration.retarget} />
              <Capability label="Difficulty" enabled={configuration.difficult} />
              <div className="border-accentSecondary/20 col-span-2 border-t pt-3">
                <span className="text-textMuted">Pruned below</span>
                <span className="text-textPrimary float-right font-mono">
                  {configuration.pruned > 0 ? formatInteger(configuration.pruned) : 'Not pruned'}
                </span>
              </div>
            </div>
          ) : (
            <p className="text-textMuted px-5 py-10 text-center text-sm">
              Waiting for native configuration.
            </p>
          )}
        </div>
      </section>

      <section>
        <div className="border-accent/30 bg-surface/80 rounded-2xl border">
          <header className="border-accentSecondary/20 border-b px-5 py-4">
            <p className="text-accentSecondary text-xs tracking-wide uppercase">Recent events</p>
          </header>
          {metrics.recentEvents.length === 0 ? (
            <EmptyRow text="No events observed yet." />
          ) : (
            <ul className="divide-accentSecondary/20 divide-y">
              {metrics.recentEvents.slice(0, 6).map((event) => (
                <li key={event.id} className="flex items-center gap-3 px-5 py-3 text-xs">
                  <time className="text-textMuted w-24 shrink-0 font-mono">
                    {formatTimestamp(event.time)}
                  </time>
                  <span className="text-textPrimary min-w-0 flex-1 truncate font-mono">
                    {getEventDefinition(event.event).name}
                  </span>
                  <span className="text-accent tabular-nums">
                    {formatEventValue(event.event, event.value)}
                  </span>
                </li>
              ))}
            </ul>
          )}
        </div>
      </section>
    </div>
  );
}

function ConnectionCard({
  title,
  endpoint,
  status,
  detail,
}: {
  title: string;
  endpoint: string;
  status: ConnectionStatus;
  detail: string;
}) {
  return (
    <article className="border-accent/30 bg-surface/80 min-w-0 rounded-2xl border p-5">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <p className="text-accentSecondary text-xs tracking-wide uppercase">{title}</p>
          <p className="text-textPrimary mt-2 font-mono text-sm break-all">
            {compactEndpoint(endpoint)}
          </p>
        </div>
        <ConnectionPill phase={status.phase} label={status.phase} />
      </div>
      <div className="text-textMuted border-accentSecondary/20 mt-4 flex flex-wrap justify-between gap-2 border-t pt-3 text-xs">
        <span>{detail}</span>
        <span>
          {String(status.reconnects)} reconnect{status.reconnects === 1 ? '' : 's'}
        </span>
      </div>
    </article>
  );
}

function Capability({ label, enabled }: { label: string; enabled: boolean }) {
  return (
    <div className="flex items-center justify-between gap-3">
      <span className="text-textMuted">{label}</span>
      <span className={enabled ? 'text-emerald-300' : 'text-textMuted'}>
        {enabled ? 'Enabled' : 'Disabled'}
      </span>
    </div>
  );
}

function EmptyRow({ text }: { text: string }) {
  return <p className="text-textMuted px-5 py-10 text-center text-sm">{text}</p>;
}

function nativeSyncDescription(estimate: SyncEstimate | null): string {
  if (!estimate) return 'Waiting for confirmed block timestamp';
  return estimate.blocksRemaining <= SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS
    ? 'Near current (estimated)'
    : `${formatInteger(estimate.blocksRemaining)} blocks remaining (estimated)`;
}
