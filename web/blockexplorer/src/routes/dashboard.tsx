import { type ReactNode } from 'react';
import { Link } from 'react-router-dom';
import {
  type BlockchainEventTransport,
  type BlockSummary,
  useProviderInfo,
} from '@/lib/api/client';
import {
  useBlockStats,
  useBlockchainEventStatus,
  useLiveLatestBlocks,
  useProviderConfiguration,
} from '@/lib/hooks/useBlockchainData';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import {
  formatBytes,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
} from '@/lib/utils/format';
import { HashText } from '@/components/hash-text';
import { blockByHashPath, blockByHeightPath, blocksPath } from '@/lib/utils/routes';

const DASHBOARD_BLOCK_SAMPLE_SIZE = 24;
const LATEST_BLOCK_TABLE_LIMIT = 8;

export function DashboardPage() {
  usePageTitle();
  const info = useProviderInfo();
  const now = useNow();
  const eventStatus = useBlockchainEventStatus();
  const { data: configuration } = useProviderConfiguration();
  const {
    data: blocks,
    isLoading: blocksLoading,
    isError: blocksError,
    error: blocksErrorValue,
  } = useLiveLatestBlocks(DASHBOARD_BLOCK_SAMPLE_SIZE);
  const latestBlock = blocks?.[0];
  const visibleBlocks = blocks?.slice(0, LATEST_BLOCK_TABLE_LIMIT);
  const statsTarget = info.id === 'libbitcoin' && latestBlock ? latestBlock.hash : '';
  const { data: latestStats, isLoading: latestStatsLoading } = useBlockStats(statsTarget, {
    enabled: info.id === 'libbitcoin' && latestBlock !== undefined,
  });
  const summary = summarizeBlocks(blocks ?? []);
  const sampleSize = blocks?.length ?? 0;
  const showStatsPlaceholder =
    info.id === 'libbitcoin' && latestBlock !== undefined && latestStatsLoading;
  const showNetworkCapabilities = configuration?.retarget !== configuration?.difficult;

  return (
    <div className="space-y-8">
      {blocksError ? (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Could not load dashboard</p>
          <p className="text-textMuted mt-1 break-words">
            {blocksErrorValue instanceof Error ? blocksErrorValue.message : 'Unknown error'}
          </p>
        </div>
      ) : null}

      <header className="space-y-2">
        <p className="text-accentSecondary text-sm tracking-wide uppercase">Dashboard</p>
        <div className="flex flex-wrap items-end justify-between gap-4">
          <div className="space-y-2">
            <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
              Overview
            </h1>
          </div>
          <Link
            to={blocksPath()}
            className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
          >
            View All Blocks
          </Link>
        </div>
      </header>

      <section className="grid gap-4 sm:grid-cols-2 xl:grid-cols-5">
        <OverviewCard
          label="Top"
          value={
            latestBlock ? (
              <Link
                to={blockByHashPath(latestBlock.hash)}
                className="text-accent hover:text-accentSecondary"
              >
                #{formatNumber(latestBlock.height, { maximumFractionDigits: 0 })}
              </Link>
            ) : blocksLoading ? (
              'Loading…'
            ) : (
              '—'
            )
          }
          detail={latestBlock ? <HashText value={latestBlock.hash} truncate={10} /> : '—'}
        />
        <OverviewCard
          label="Latest Age"
          value={
            latestBlock
              ? formatRelativeTime(latestBlock.timestamp, now)
              : blocksLoading
                ? 'Loading…'
                : '—'
          }
          detail={latestBlock ? formatTimestamp(latestBlock.timestamp) : '—'}
        />
        <OverviewCard
          label="Avg Interval"
          value={formatInterval(summary.averageIntervalSeconds)}
          detail={formatIntervalSampleDetail(summary.intervalCount)}
        />
        <OverviewCard
          label="Avg Block Size"
          value={formatBytes(summary.averageSizeBytes)}
          detail={summary.largestBlock ? `Peak ${formatBytes(summary.largestBlock.size)}` : '—'}
        />
        <OverviewCard
          label="Avg Tx / Block"
          value={formatNumber(summary.averageTxCount, { maximumFractionDigits: 0 })}
          detail={
            typeof latestBlock?.txCount === 'number'
              ? `Top ${formatNumber(latestBlock.txCount, { maximumFractionDigits: 0 })}`
              : '—'
          }
        />
      </section>

      <section className="grid gap-6 xl:grid-cols-[minmax(0,1.5fr)_minmax(20rem,1fr)]">
        <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-6">
          <div className="flex flex-wrap items-start justify-between gap-4">
            <div className="space-y-2">
              <p className="text-accentSecondary text-xs tracking-wide uppercase">Latest Block</p>
              <h2 className="text-accent text-2xl font-semibold">
                {latestBlock
                  ? `#${formatNumber(latestBlock.height, { maximumFractionDigits: 0 })}`
                  : blocksLoading
                    ? 'Loading…'
                    : '—'}
              </h2>
              <p className="text-textPrimary font-mono text-xs break-all">
                {latestBlock ? <HashText value={latestBlock.hash} /> : '—'}
              </p>
              <p className="text-textMuted text-sm">
                {latestBlock
                  ? `${formatRelativeTime(latestBlock.timestamp, now)} · ${formatTimestamp(latestBlock.timestamp)}`
                  : 'Waiting for block data'}
              </p>
            </div>
            {latestBlock ? (
              <Link
                to={blockByHashPath(latestBlock.hash)}
                className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
              >
                View Block
              </Link>
            ) : null}
          </div>

          <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
            <PanelStat
              label="Transactions"
              value={formatNumber(latestBlock?.txCount, { maximumFractionDigits: 0 })}
            />
            <PanelStat label="Size" value={formatBytes(latestBlock?.size)} />
            <PanelStat
              label="Weight"
              value={
                typeof latestStats?.weight === 'number'
                  ? formatNumber(latestStats.weight, { maximumFractionDigits: 0 })
                  : showStatsPlaceholder
                    ? '…'
                    : '—'
              }
            />
            <PanelStat
              label="Fees"
              value={
                typeof latestStats?.feesSats === 'number'
                  ? formatSats(latestStats.feesSats)
                  : showStatsPlaceholder
                    ? '…'
                    : '—'
              }
            />
            <PanelStat
              label="Subsidy"
              value={
                typeof latestStats?.subsidySats === 'number'
                  ? formatSats(latestStats.subsidySats)
                  : showStatsPlaceholder
                    ? '…'
                    : '—'
              }
            />
          </div>
        </div>

        <div className="grid gap-6">
          <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-6">
            <div className="space-y-2">
              <p className="text-accentSecondary text-xs tracking-wide uppercase">Provider</p>
              <h2 className="text-accent text-xl font-semibold">Provider Status</h2>
              <p className="text-textMuted text-sm">
                {info.label}
                {info.endpoint ? ` · ${formatEndpointLabel(info.endpoint)}` : ''}
              </p>
            </div>

            <div className="grid gap-4 sm:grid-cols-2">
              <PanelStat label="Connection" value={formatLabelValue(info.connectionState)} />
              <PanelStat label="Top Updates" value={formatTopUpdateMode(eventStatus.transport)} />
              <PanelStat label="Transport" value={formatEventTransport(eventStatus.transport)} />
              <PanelStat label="Provider ID" value={info.id} />
            </div>

            <div className="space-y-3">
              <p className="text-accentSecondary text-xs tracking-wide uppercase">Capabilities</p>
              <div className="flex flex-wrap gap-2">
                <CapabilityPill label="Address Index" enabled={configuration?.address} />
                <CapabilityPill label="Witness Node" enabled={configuration?.witness} />
                <CapabilityPill label="Client Filters" enabled={configuration?.filter} />
                <CapabilityPill label="Turbo API" enabled={configuration?.turbo} />
                {showNetworkCapabilities && (
                  <>
                    <CapabilityPill label="Retarget" enabled={configuration?.retarget} />
                    <CapabilityPill label="Difficulty" enabled={configuration?.difficult} />
                  </>
                )}
              </div>
            </div>
          </div>
        </div>
      </section>

      <section>
        <header className="mb-4 flex items-center justify-between gap-4">
          <div>
            <h2 className="text-accent text-lg font-semibold">Latest Blocks</h2>
          </div>
        </header>
        <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
          <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
            <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
              <tr>
                <th className="px-4 py-3 text-left">Height</th>
                <th className="px-4 py-3 text-left">Hash</th>
                <th className="px-4 py-3 text-left">Age</th>
                <th className="px-4 py-3 text-right">Tx</th>
                <th className="px-4 py-3 text-right">Size</th>
              </tr>
            </thead>
            <tbody>
              {blocksLoading && (
                <tr>
                  <td colSpan={5} className="text-textMuted px-4 py-6 text-center">
                    Loading latest blocks…
                  </td>
                </tr>
              )}
              {!blocksLoading && sampleSize === 0 && (
                <tr>
                  <td colSpan={5} className="text-textMuted px-4 py-6 text-center">
                    No blocks available.
                  </td>
                </tr>
              )}
              {!blocksLoading &&
                visibleBlocks?.map((block) => (
                  <tr key={block.hash} className="hover:bg-accentSecondary/10">
                    <td className="px-4 py-3">
                      <Link
                        to={blockByHeightPath(block.height)}
                        className="text-accent hover:text-accentSecondary font-medium"
                      >
                        {formatNumber(block.height, { maximumFractionDigits: 0 })}
                      </Link>
                    </td>
                    <td className="text-textPrimary px-4 py-3">
                      <Link to={blockByHashPath(block.hash)} className="text-textPrimary">
                        <HashText value={block.hash} className="font-mono" />
                      </Link>
                    </td>
                    <td className="text-textMuted px-4 py-3">
                      {formatRelativeTime(block.timestamp, now)}
                    </td>
                    <td className="text-textPrimary px-4 py-3 text-right">
                      {formatNumber(block.txCount, { maximumFractionDigits: 0 })}
                    </td>
                    <td className="text-textPrimary px-4 py-3 text-right">
                      {formatBytes(block.size)}
                    </td>
                  </tr>
                ))}
            </tbody>
          </table>
        </div>
      </section>
    </div>
  );
}

function summarizeBlocks(blocks: BlockSummary[]) {
  if (blocks.length === 0) {
    return {
      averageIntervalSeconds: undefined,
      averageSizeBytes: undefined,
      averageTxCount: undefined,
      largestBlock: undefined,
      intervalCount: 0,
    };
  }

  const intervals: number[] = [];
  const sizes: number[] = [];
  const txCounts: number[] = [];
  let largestBlock: BlockSummary | undefined;

  for (const [index, block] of blocks.entries()) {
    if (typeof block.size === 'number') {
      sizes.push(block.size);
      if (!largestBlock || block.size > (largestBlock.size ?? -1)) {
        largestBlock = block;
      }
    }
    if (typeof block.txCount === 'number') {
      txCounts.push(block.txCount);
    }
    if (index > 0 && blocks[index - 1].timestamp > block.timestamp) {
      const previous = blocks[index - 1];
      intervals.push(previous.timestamp - block.timestamp);
    }
  }

  return {
    averageIntervalSeconds: averageOf(intervals),
    averageSizeBytes: averageOf(sizes),
    averageTxCount: averageOf(txCounts),
    largestBlock,
    intervalCount: intervals.length,
  };
}

function averageOf(values: number[]) {
  if (values.length === 0) return;
  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

function formatInterval(seconds?: number) {
  if (seconds === undefined) return '—';
  if (seconds < 60) {
    return `${formatNumber(seconds, { maximumFractionDigits: 0 })} sec`;
  }
  if (seconds < 3_600) {
    return `${formatNumber(seconds / 60, { maximumFractionDigits: 1 })} min`;
  }
  return `${formatNumber(seconds / 3_600, { maximumFractionDigits: 2 })} hr`;
}

function formatIntervalSampleDetail(intervalCount: number) {
  if (intervalCount === 0) return '—';
  return `Last ${formatNumber(intervalCount, { maximumFractionDigits: 0 })} ${
    intervalCount === 1 ? 'interval' : 'intervals'
  }`;
}

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

function formatLabelValue(value: string) {
  if (!value) return value;
  return `${value[0].toUpperCase()}${value.slice(1)}`;
}

function formatTopUpdateMode(transport: BlockchainEventTransport) {
  if (transport === 'websocket' || transport === 'connecting') return 'Push';
  if (transport === 'polling') return 'Polling';
  return 'Idle';
}

function formatEventTransport(transport: BlockchainEventTransport) {
  if (transport === 'websocket') return 'WebSocket';
  if (transport === 'connecting') return 'WebSocket connecting';
  if (transport === 'polling') return 'HTTP';
  return 'Idle';
}

function OverviewCard({
  label,
  value,
  detail,
}: {
  label: string;
  value: ReactNode;
  detail: ReactNode;
}) {
  return (
    <div className="border-accent/30 bg-surface/80 space-y-2 rounded-2xl border p-5">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <div className="text-textPrimary text-2xl font-semibold">{value}</div>
      <p className="text-textMuted text-sm">{detail}</p>
    </div>
  );
}

function PanelStat({ label, value }: { label: string; value: ReactNode }) {
  return (
    <div className="border-accentSecondary/20 bg-accent/5 rounded-xl border p-3">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary mt-2 text-sm">{value}</p>
    </div>
  );
}

function CapabilityPill({ label, enabled }: { label: string; enabled?: boolean }) {
  const active = enabled === true;
  const stateLabel = enabled === undefined ? 'Unknown' : active ? 'On' : 'Off';

  return (
    <span
      className={`rounded-full px-3 py-1 text-xs tracking-wide uppercase ${
        active
          ? 'bg-emerald-500/15 text-emerald-300'
          : enabled === undefined
            ? 'bg-slate-500/15 text-slate-300'
            : 'bg-rose-500/15 text-rose-300'
      }`}
    >
      {label}: {stateLabel}
    </span>
  );
}
