import { Fragment, type ReactNode } from 'react';
import { coinOf, describePrefix } from '@/lib/wallet-prefixes';
import { getEventDefinition } from '@/lib/admin-contract';
import { toWebSocketUrl, type EndpointConfiguration } from '@/lib/endpoints';
import {
  formatElapsed,
  formatEventValue,
  formatInteger,
  formatRate,
  formatTimestamp,
} from '@/lib/format';
import { detectNodeActivity, type MetricsSnapshot } from '@/lib/live-metrics';
import type { ConnectionPhase, ConnectionStatus } from '@/lib/reconnecting-websocket';
import { eventSignalId } from '@/lib/signal-catalog';
import { type ChainSync, SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS } from '@/lib/sync-status';
import type { NativeTelemetry } from '@/lib/use-telemetry';
import { averageRate, nextBlockRate } from '@/lib/pool-rates';
import type { NativeFees } from '@/lib/native-contract';
import { MetricItem } from './metric-item';

// A page of the native site, opened apart from the admin site where it is
// advertised, else the text alone.
function ExplorerLink({
  explorer,
  path,
  children,
}: {
  explorer: string | undefined;
  path: string;
  children: ReactNode;
}) {
  if (!explorer) return <>{children}</>;
  return (
    <a
      href={`${explorer}${path}`}
      target="_blank"
      rel="noopener noreferrer"
      className="text-accent hover:text-accentSecondary"
    >
      {children}
    </a>
  );
}
import { NodeSyncProgress } from './node-sync-progress';
import { TelemetryDashboard } from './telemetry-dashboard';

interface OverviewProps {
  adminStatus: ConnectionStatus;
  endpoints: EndpointConfiguration;
  explorer: string | undefined;
  metrics: MetricsSnapshot;
  native: NativeTelemetry;
  sync: ChainSync | null;
}
const lifecycleEvents: ReadonlyArray<{ event: number; label: string }> = [
  { event: 0, label: 'Header archived' },
  { event: 3, label: 'Block archived' },
  { event: 5, label: 'Block validated' },
  { event: 6, label: 'Block confirmed' },
  { event: 13, label: 'Block organized' },
  { event: 14, label: 'Block reorganized' },
];

export function Overview({ adminStatus, endpoints, explorer, metrics, native, sync }: OverviewProps) {
  const configuration = native.configuration;
  const pooling = configuration?.network?.relay === true && sync?.blocksRemaining === 0;
  const activity = detectNodeActivity(metrics, 10, pooling);
  const pool = pooling ? metrics.poolSnapshot?.data : undefined;
  const coin = coinOf(configuration?.forks ?? []);
  // Admin and native share one socket when their endpoints match.
  const shared = endpoints.adminUrl === endpoints.nativeUrl;
  const connection = combineStatus(adminStatus, native.status, shared);
  const warnings = [
    adminStatus.error ? `${shared ? 'Connection' : 'Admin'}: ${adminStatus.error}` : null,
    native.status.error && !shared ? `Native: ${native.status.error}` : null,
    adminStatus.refused ? `${shared ? 'Server' : 'Admin'} refused: ${adminStatus.refused}` : null,
    native.status.refused && !shared ? `Native refused: ${native.status.refused}` : null,
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
    <div className="space-y-8">
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

      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <h1 className="text-accent text-2xl font-semibold">Overview</h1>
          <p className="text-textMuted text-sm">The chain, pool and telemetry of this node.</p>
        </div>
      </header>

      <NodeSyncProgress sync={sync} />

      <dl aria-label="Node summary" className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
        <MetricItem
          label="Current top"
          value={
            native.top === null ? '—' : <ExplorerLink explorer={explorer} path="/v1/top">{formatInteger(native.top)}</ExplorerLink>
          }
          detail={
            native.transport === 'none'
              ? 'Waiting for native API'
              : `${nativeSyncDescription(sync)} · via ${
                  native.transport === 'websocket' ? 'native WebSocket' : 'REST fallback'
                }`
          }
        />
        <MetricItem
          label="Current activity"
          value={activity.label}
          detail={
            activity.rate > 0
              ? `${formatRate(activity.rate, activity.unit)} over the last 10 seconds`
              : activity.label === 'Pooling'
                ? 'No pool admissions in the last 10 seconds'
                : 'No lifecycle events in the last 10 seconds'
          }
          numeric={false}
        />
        <MetricItem
          label="Collection uptime"
          value={formatElapsed(metrics.now - metrics.sessionStartedAt)}
          detail={`${formatInteger(metrics.totalEvents)} events · ${formatInteger(metrics.totalLogs)} logs`}
        />
        <MetricItem
          label="Connection"
          value={
            connection.phase === 'connected'
              ? toWebSocketUrl(endpoints.adminUrl).replace(/\/$/u, '')
              : '[disconnected]'
          }
          detail={`${
            connection.lastMessageAt
              ? `Last frame ${formatTimestamp(connection.lastMessageAt)}`
              : 'Waiting for the first frame'
          } · ${String(connection.reconnects)} reconnect${connection.reconnects === 1 ? '' : 's'}`}
          numeric={false}
        />
        <MetricItem
          label="Pool size"
          value={
            pool ? (
              <ExplorerLink explorer={explorer} path="/v1/txs/unconfirmed">{`${formatInteger(pool.count)} transactions`}</ExplorerLink>
            ) : (
              '—'
            )
          }
          detail={pool ? `${formatBtc(pool.fee)} btc fees · ${formatMvb(pool.virtual)} MvB` : 'Not pooling'}
        />
        <MetricItem
          label="Fee rate"
          value={
            pool ? (
              <ExplorerLink explorer={explorer} path="/v1/fees">{`${formatSatsPerVbyte(averageRate(pool))} sat/vB`}</ExplorerLink>
            ) : (
              '—'
            )
          }
          detail={pool ? nextBlockDetail(pool) : 'Not pooling'}
        />
      </dl>

      <TelemetryDashboard metrics={metrics} />

      <section className="grid gap-6 xl:grid-cols-2">
        <div className="border-accent/30 bg-surface/80 overflow-hidden rounded-2xl border">
          <header className="border-accentSecondary/20 border-b px-5 py-4">
            <h2 className="text-accent text-lg font-semibold">Lifecycle heights</h2>
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
            <h2 className="text-accent text-lg font-semibold">Native configuration</h2>
          </header>
          {configuration ? (
            <div className="space-y-4 p-5 text-sm">
              <ConfigGroup title="Store configuration">
                <Flags
                  items={[
                    ['Address index', configuration.store.address],
                    ['Client filters index', configuration.store.filter],
                    ['Silent payments index', configuration.store.silent !== null],
                    ['Turbo', configuration.store.turbo],
                  ]}
                />
                <Values
                  items={[
                    [
                      'Silent payments from',
                      configuration.store.silent === null
                        ? 'Not indexed'
                        : formatInteger(configuration.store.silent),
                    ],
                  ]}
                />
              </ConfigGroup>
              <ConfigGroup title="Node services">
                <Flags
                  items={[
                    ['Blocks', configuration.node.blocks],
                    ['Limited', configuration.node.limited],
                    ['Witness', configuration.node.witness],
                    ['Filters', configuration.node.filters],
                    ['Privacy', configuration.node.privacy],
                  ]}
                />
                <Values
                  items={[
                    [
                      'Inputs pruned below',
                      configuration.store.pruned > 0
                        ? formatInteger(configuration.store.pruned)
                        : 'Not pruned',
                    ],
                  ]}
                />
              </ConfigGroup>
              {configuration.network && (
                <ConfigGroup title="Peer protocol">
                  <Flags
                    items={[
                      ['Relay', configuration.network.relay],
                      ['Compact', configuration.network.compact],
                      ['Address', configuration.network.address],
                      ['Witness tx', configuration.network.witness_tx],
                      ['Memory pool', configuration.network.memory_pool],
                      ['Not found', configuration.network.not_found],
                      ['Reject', configuration.network.reject],
                      ['Alert', configuration.network.alert],
                    ]}
                  />
                  <Values
                    items={[
                      ['Protocol minimum', String(configuration.network.protocol_minimum)],
                      ['Protocol maximum', String(configuration.network.protocol_maximum)],
                    ]}
                  />
                </ConfigGroup>
              )}
              {configuration.wallet && (
                <ConfigGroup title="Wallet">
                  <Values
                    items={[
                      [
                        'P2PKH version',
                        String(configuration.wallet.p2kh),
                        describePrefix(coin, 'p2kh', configuration.wallet.p2kh),
                      ],
                      [
                        'P2SH version',
                        String(configuration.wallet.p2sh),
                        describePrefix(coin, 'p2sh', configuration.wallet.p2sh),
                      ],
                      [
                        'WIF version',
                        String(configuration.wallet.wif),
                        describePrefix(coin, 'wif', configuration.wallet.wif),
                      ],
                      [
                        'Witness prefix',
                        configuration.wallet.witness,
                        describePrefix(coin, 'witness', configuration.wallet.witness),
                      ],
                      [
                        'Silent prefix',
                        configuration.wallet.silent,
                        describePrefix(coin, 'silent', configuration.wallet.silent),
                      ],
                      [
                        'HD private',
                        toHex32(configuration.wallet.hd_private),
                        describePrefix(coin, 'hdPrivate', configuration.wallet.hd_private),
                      ],
                      [
                        'HD public',
                        toHex32(configuration.wallet.hd_public),
                        describePrefix(coin, 'hdPublic', configuration.wallet.hd_public),
                      ],
                    ]}
                  />
                </ConfigGroup>
              )}
              <ConfigGroup title="Configured forks">
                <div className="flex flex-wrap gap-1.5">
                  {configuration.forks.map((fork) => (
                    <span
                      key={fork}
                      className="text-textPrimary bg-accentSecondary/15 rounded-full px-2 py-0.5 font-mono text-xs"
                    >
                      {fork}
                    </span>
                  ))}
                </div>
              </ConfigGroup>
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

const PHASE_RANK: Record<ConnectionPhase, number> = {
  connected: 0,
  connecting: 1,
  reconnecting: 2,
  idle: 3,
};

function combineStatus(
  admin: ConnectionStatus,
  native: ConnectionStatus,
  shared: boolean
): ConnectionStatus {
  const worse = PHASE_RANK[native.phase] > PHASE_RANK[admin.phase] ? native : admin;
  const last = Math.max(admin.lastMessageAt ?? 0, native.lastMessageAt ?? 0);
  return {
    ...worse,
    reconnects: shared ? admin.reconnects : admin.reconnects + native.reconnects,
    lastMessageAt: last > 0 ? last : null,
  };
}

function ConfigGroup({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="border-accentSecondary/20 space-y-2 border-t pt-3 first:border-t-0 first:pt-0">
      <h3 className="text-accentSecondary text-xs tracking-wide uppercase">{title}</h3>
      {children}
    </section>
  );
}

function Flags({ items }: { items: ReadonlyArray<readonly [string, boolean | undefined]> }) {
  const reported = items.filter(([, enabled]) => enabled !== undefined);
  return (
    <div className="flex flex-wrap gap-1.5">
      {reported.map(([label, enabled]) => (
        <span
          key={label}
          className={`rounded-full border px-2 py-0.5 text-xs ${enabled ? 'border-emerald-500/50 bg-emerald-500/15 text-emerald-300' : 'text-textMuted border-slate-500/40'}`}
        >
          {label}
        </span>
      ))}
    </div>
  );
}

function Values({
  items,
}: {
  items: ReadonlyArray<readonly [string, string] | readonly [string, string, string]>;
}) {
  const described = items.some((item) => item.length > 2);
  const columns = described
    ? 'grid-cols-[10rem_max-content_auto]'
    : 'grid-cols-[10rem_max-content]';
  return (
    <dl className={`grid ${columns} items-baseline gap-x-4 gap-y-1`}>
      {items.map(([label, value, meaning]) => (
        <Fragment key={label}>
          <dt className="text-textMuted">{label}</dt>
          <dd className="text-textPrimary font-mono">{value}</dd>
          {described && <dd className="text-textMuted">{meaning}</dd>}
        </Fragment>
      ))}
    </dl>
  );
}

function toHex32(value: number) {
  return `0x${value.toString(16).padStart(8, '0')}`;
}

function EmptyRow({ text }: { text: string }) {
  return <p className="text-textMuted px-5 py-10 text-center text-sm">{text}</p>;
}

function nativeSyncDescription(sync: ChainSync | null): string {
  if (!sync) return 'Waiting for chain tops';
  if (sync.diverged) return 'Candidate branch diverges';
  if (sync.blocksRemaining === null) return 'Headers not current';
  if (sync.blocksRemaining === 0) return 'At top candidate header';
  return sync.blocksRemaining <= SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS
    ? `Near current, ${formatInteger(sync.blocksRemaining)} blocks remaining`
    : `${formatInteger(sync.blocksRemaining)} blocks remaining`;
}

const btcFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 8 });
const mvbFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 1 });
const rateFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 });

function formatBtc(sats: number): string {
  return btcFormatter.format(sats / 100_000_000);
}

function formatMvb(virtual: number): string {
  return mvbFormatter.format(virtual / 1_000_000);
}

function formatSatsPerVbyte(rate: number | null): string {
  return rate === null ? '—' : rateFormatter.format(rate);
}

function nextBlockDetail(pool: NativeFees): string {
  const rate = nextBlockRate(pool);
  if (rate === null) return 'Pool fits in the next block';
  return `next block ≥ ${formatSatsPerVbyte(rate)} sat/vB`;
}
