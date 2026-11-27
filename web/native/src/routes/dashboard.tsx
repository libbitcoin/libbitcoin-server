import { type ReactNode } from 'react';
import { Link } from 'react-router-dom';
import { type BlockSummary, useServerInfo } from '@/lib/api/client';
import {
  useBlockStats,
  useBlockchainEventStatus,
  useFees,
  useLatestBlocks,
  useRecentTransactions,
  useTransactionSizes,
  useServerConfiguration,
} from '@/lib/hooks/useBlockchainData';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import {
  formatBytes,
  formatNumber,
  formatRelativeTime,
  formatRate,
  formatSats,
  formatShortRelativeTime,
  formatTimestamp,
} from '@/lib/utils/format';
import { HashText } from '@/components/hash-text';
import { MinerShareCard } from '@/components/miner-share';
import { BlockMinerValue } from '@/components/miner-value';
import { WeakBranchBadge } from '@/components/branch-notice';
import { Spinner } from '@/components/spinner';
import {
  blockByHashPath,
  blockByHeightPath,
  blocksPath,
  configurationPath,
  feesPath,
  topPath,
  transactionPath,
  transactionsPath,
} from '@/lib/utils/routes';

const DASHBOARD_BLOCK_SAMPLE_SIZE = 144;
const LATEST_BLOCK_TABLE_LIMIT = 8;
const RECENT_TRANSACTION_LIMIT = 10;

export function DashboardPage() {
  usePageTitle();
  const info = useServerInfo();
  const now = useNow();
  const { data: fees } = useFees();
  const estimate = fees?.estimates?.economical?.['1'];
  const nextBlockRate =
    typeof estimate === 'number'
      ? estimate
      : fees && fees.virtual > 0
        ? fees.fee / fees.virtual
        : undefined;
  const recentTransactions = useRecentTransactions(RECENT_TRANSACTION_LIMIT);
  const eventStatus = useBlockchainEventStatus();
  const websocket = eventStatus.transport === 'websocket';
  const connectionUrl = websocket ? info.endpoint?.replace(/^http/u, 'ws') : info.endpoint;
  const connectionLabel =
    info.connectionState === 'offline'
      ? '[disconnected]'
      : info.connectionState === 'unknown'
        ? '[connecting]'
        : `${connectionUrl ?? info.label}${
            websocket
              ? info.connectionState === 'degraded'
                ? ' [HTTP unavailable]'
                : ''
              : ' [HTTP polling]'
          }`;
  const {
    data: configuration,
    isError: configurationError,
    isFetching: configurationFetching,
    refetch: retryConfiguration,
  } = useServerConfiguration();
  const {
    data: blocks,
    isLoading: blocksLoading,
    isError: blocksError,
    error: blocksErrorValue,
  } = useLatestBlocks(DASHBOARD_BLOCK_SAMPLE_SIZE);
  const latestBlock = blocks?.[0];
  const visibleBlocks = blocks?.slice(0, LATEST_BLOCK_TABLE_LIMIT);
  const statsTarget = latestBlock?.hash ?? '';
  const { data: latestStats, isLoading: latestStatsLoading } = useBlockStats(statsTarget, {
    enabled: latestBlock !== undefined,
  });
  const summary = summarizeBlocks(blocks ?? []);
  const sampleSize = blocks?.length ?? 0;
  const showStatsPlaceholder = latestBlock !== undefined && latestStatsLoading;

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

      <header>
        <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
          Overview
        </h1>
        <p className="text-textMuted text-sm">The chain, pool and services of this node.</p>
      </header>

      <section className="grid gap-4 sm:grid-cols-2 xl:grid-cols-6">
        <OverviewCard
          label="Top Block"
          value={
            latestBlock ? (
              <Link to={topPath()} className="text-accent hover:text-accentSecondary">
                {formatNumber(latestBlock.height, { maximumFractionDigits: 0 })}
              </Link>
            ) : blocksLoading ? (
              'Loading…'
            ) : (
              '—'
            )
          }
          detail={latestBlock ? formatRelativeTime(latestBlock.timestamp, now) : '—'}
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
        <OverviewCard
          label="Next Block Fee"
          value={
            nextBlockRate === undefined ? (
              '—'
            ) : (
              <Link to={feesPath()} className="text-accent hover:text-accentSecondary">
                {`${formatNumber(nextBlockRate, { maximumFractionDigits: 2 })} sat/vB`}
              </Link>
            )
          }
          detail={
            fees
              ? `${typeof estimate === 'number' ? 'estimate' : 'pool average'} · ${formatNumber(fees.count, { maximumFractionDigits: 0 })} txs`
              : '—'
          }
        />
        <MinerShareCard count={DASHBOARD_BLOCK_SAMPLE_SIZE} />
      </section>

      <section className="grid gap-6 xl:grid-cols-[minmax(0,1.5fr)_minmax(20rem,1fr)]">
        <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-6">
          <div className="flex flex-wrap items-start justify-between gap-4">
            <div className="space-y-2">
              <p className="text-accentSecondary text-xs tracking-wide uppercase">Latest Block</p>
              <h2 className="text-accent text-2xl font-semibold">
                {latestBlock ? (
                  <Link to={blockByHeightPath(latestBlock.height)} className="hover:text-accentSecondary">
                    {formatNumber(latestBlock.height, { maximumFractionDigits: 0 })}
                  </Link>
                ) : blocksLoading ? (
                  'Loading…'
                ) : (
                  '—'
                )}
              </h2>
              <p className="text-textPrimary font-mono text-xs break-all">
                {latestBlock ? (
                  <Link to={blockByHashPath(latestBlock.hash)} className="text-accent hover:text-accentSecondary">
                    <HashText value={latestBlock.hash} />
                  </Link>
                ) : (
                  '—'
                )}
              </p>
              <p className="text-textMuted text-sm">
                {latestBlock
                  ? `${formatRelativeTime(latestBlock.timestamp, now)} · ${formatTimestamp(latestBlock.timestamp)}`
                  : 'Waiting for block data'}
              </p>
            </div>
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
            <PanelStat label="Miner" value={<BlockMinerValue hash={latestBlock?.hash} />} />
          </div>
        </div>

        <div className="grid gap-6">
          <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-6">
            <div className="flex items-start justify-between gap-3">
              <div className="space-y-2">
                <p className="text-accentSecondary text-xs tracking-wide uppercase">Server</p>
                <h2 className="text-accent text-xl font-semibold">
                  <Link to={configurationPath()} className="hover:text-accentSecondary">
                    Server Status
                  </Link>
                </h2>
              </div>
            </div>

            <div className="space-y-3">
              <p className="text-accentSecondary text-xs tracking-wide uppercase">Connection</p>
              <p
                role="status"
                aria-label={`Server connection: ${info.connectionState}`}
                className="text-textPrimary font-mono text-sm break-all"
              >
                {connectionLabel}
              </p>
            </div>

            {configurationError && (
              <div role="status" className="text-textMuted space-y-2 text-sm">
                <p>Server configuration unavailable. Retrying automatically.</p>
                <button
                  type="button"
                  disabled={configurationFetching}
                  onClick={() => {
                    void retryConfiguration();
                  }}
                  className="text-accent hover:text-accentSecondary disabled:opacity-50"
                >
                  {configurationFetching ? 'Retrying…' : 'Retry configuration'}
                </button>
              </div>
            )}

            {configuration && (
              <>
                <ConfigGroup title="Store configuration">
                  <Flags
                    items={[
                      ['Address index', configuration.address],
                      ['Client filters index', configuration.filter],
                      ['Silent payments index', configuration.silent],
                      ['Turbo', configuration.turbo],
                    ]}
                  />
                </ConfigGroup>
                <ConfigGroup title="Node services">
                  <Flags
                    items={[
                      ['Blocks', configuration.services.blocks],
                      ['Limited', configuration.services.limited],
                      ['Witness', configuration.services.witness],
                      ['Filters', configuration.services.filters],
                      ['Privacy', configuration.services.privacy],
                    ]}
                  />
                </ConfigGroup>
                <ConfigGroup title="Peer protocol">
                  <Flags
                    items={[
                      ['Relay', configuration.protocol.relay],
                      ['Compact', configuration.protocol.compact],
                      ['Address', configuration.protocol.address],
                      ['Witness tx', configuration.protocol.witness_tx],
                      ['Memory pool', configuration.protocol.memory_pool],
                      ['Not found', configuration.protocol.not_found],
                      ['Reject', configuration.protocol.reject],
                      ['Alert', configuration.protocol.alert],
                    ]}
                  />
                </ConfigGroup>
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
              </>
            )}
          </div>
        </div>
      </section>

      <section>
        <header className="mb-4 flex items-center justify-between gap-4">
          <h2 className="text-accent text-lg font-semibold">Latest Blocks</h2>
          <Link to={blocksPath()} className="text-accent hover:text-accentSecondary text-sm">
            All blocks →
          </Link>
        </header>
        <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
          <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
            <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
              <tr>
                <th className="px-4 py-3 text-left">Height</th>
                <th className="px-4 py-3 text-left">Hash</th>
                <th className="px-4 py-3 text-left">Age</th>
                <th className="px-4 py-3 text-left">Miner</th>
                <th className="px-4 py-3 text-right">Txs</th>
                <th className="px-4 py-3 text-right">Size</th>
              </tr>
            </thead>
            <tbody>
              {blocksLoading && (
                <tr>
                  <td colSpan={6} className="text-textMuted px-4 py-6 text-center">
                    Loading latest blocks…
                  </td>
                </tr>
              )}
              {!blocksLoading && sampleSize === 0 && (
                <tr>
                  <td colSpan={6} className="text-textMuted px-4 py-6 text-center">
                    No blocks available.
                  </td>
                </tr>
              )}
              {!blocksLoading &&
                visibleBlocks?.map((block) => (
                  <tr key={block.hash} className="hover:bg-accentSecondary/10">
                    <td className="px-4 py-3">
                      <span className="flex items-center gap-2">
                        <Link
                          to={
                            block.weak ? blockByHashPath(block.hash) : blockByHeightPath(block.height)
                          }
                          className="text-accent hover:text-accentSecondary font-medium"
                        >
                          {formatNumber(block.height, { maximumFractionDigits: 0 })}
                        </Link>
                        {block.weak && <WeakBranchBadge />}
                      </span>
                    </td>
                    <td className="text-textPrimary px-4 py-3">
                      <Link to={blockByHashPath(block.hash)} className="text-accent hover:text-accentSecondary">
                        <HashText value={block.hash} className="font-mono" />
                      </Link>
                    </td>
                    <td className="text-textMuted px-4 py-3">
                      {formatRelativeTime(block.timestamp, now)}
                    </td>
                    <td className="text-textPrimary px-4 py-3">
                      <BlockMinerValue hash={block.hash} />
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

      <section>
        <header className="mb-4 flex items-center justify-between gap-4">
          <h2 className="text-accent text-lg font-semibold">Recent Transactions</h2>
          <Link to={transactionsPath()} className="text-accent hover:text-accentSecondary text-sm">
            All transactions →
          </Link>
        </header>
        <div className="border-accent/30 overflow-hidden rounded-xl border">
          <table className="divide-accentSecondary/30 min-w-full table-fixed divide-y text-sm">
            <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
              <tr>
                <th className="px-4 py-3 text-left">TxID</th>
                <th className="w-28 px-4 py-3 text-left">Received</th>
                <th className="hidden w-28 px-4 py-3 text-right sm:table-cell">Fee</th>
                <th className="hidden w-28 px-4 py-3 text-right sm:table-cell">Size</th>
                <th className="hidden w-28 px-4 py-3 text-right sm:table-cell">Rate</th>
              </tr>
            </thead>
            <tbody>
              {recentTransactions.length === 0 && (
                <tr>
                  <td colSpan={5} className="px-4 py-6 text-center">
                    <Spinner label="Waiting for transactions…" />
                  </td>
                </tr>
              )}
              {recentTransactions.map((tx) => (
                <RecentTransactionRow key={tx.txid} txid={tx.txid} received={tx.received} now={now} />
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

function ConfigGroup({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="space-y-2">
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
          title={`${label}: ${enabled ? 'On' : 'Off'}`}
          className={`rounded-full border px-2 py-0.5 text-xs ${enabled ? 'border-emerald-500/50 bg-emerald-500/15 text-emerald-300' : 'text-textMuted border-slate-500/40'}`}
        >
          {label}
        </span>
      ))}
    </div>
  );
}

function RecentTransactionRow({
  txid,
  received,
  now,
}: {
  txid: string;
  received: number;
  now: number;
}) {
  const { data: sizes } = useTransactionSizes(txid);
  const fee = sizes?.fee;
  const rate = sizes && fee !== undefined && sizes.virtual > 0 ? fee / sizes.virtual : undefined;
  return (
    <tr className="hover:bg-accentSecondary/10">
      <td className="truncate px-4 py-3">
        <Link to={transactionPath(txid)} className="text-accent hover:text-accentSecondary font-mono">
          {txid}
        </Link>
      </td>
      <td className="text-textMuted px-4 py-3 whitespace-nowrap">
        {received * 1000 >= now ? 'just now' : formatShortRelativeTime(received, now)}
      </td>
      <td className="text-textPrimary hidden px-4 py-3 text-right tabular-nums sm:table-cell">
        {fee === undefined ? '…' : `${formatNumber(fee, { maximumFractionDigits: 0 })} sat`}
      </td>
      <td className="text-textPrimary hidden px-4 py-3 text-right tabular-nums sm:table-cell">
        {sizes ? `${formatNumber(sizes.virtual, { maximumFractionDigits: 0 })} vB` : '…'}
      </td>
      <td className="text-textPrimary hidden px-4 py-3 text-right tabular-nums sm:table-cell">
        {rate === undefined ? '…' : formatRate(rate)}
      </td>
    </tr>
  );
}
