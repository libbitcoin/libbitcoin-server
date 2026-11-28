import { useState, useSyncExternalStore } from 'react';
import { FitHash } from '@/components/fit-hash';
import { HashrateChart } from '@/components/hashrate-chart';
import type { AdminMiningFrame, MiningDevice } from '@/lib/admin-contract';
import { useCurrentTime } from '@/lib/use-current-time';
import { formatInteger } from '@/lib/format';
import type { WorkerMetricsStore } from '@/lib/worker-metrics-store';
import {
  formatDifficulty,
  formatDuration,
  formatHashrate,
  formatShortRelativeTime,
} from '@/lib/mining/format';

// The node's mining console: the channels of its stratum services, by
// service, the detail of the one chosen, and the blocks they found.
export function Mining({ metrics }: { metrics: WorkerMetricsStore }) {
  const mining = useSyncExternalStore(metrics.subscribeMining, metrics.getMining, metrics.getMining);
  const [chosen, setChosen] = useState<string>();
  const now = useCurrentTime();
  const devices = mining?.devices ?? [];
  const device = devices.find((entry) => entry.id === chosen) ?? devices[0];
  const fleet = devices.reduce((sum, entry) => sum + entry.hashrate, 0);
  const services = [...new Set(devices.map((entry) => entry.service))];

  return (
    <div className="space-y-8">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <h1 className="text-accent text-2xl font-semibold">Mining</h1>
          <p className="text-textMuted text-sm">The channels of the node&apos;s stratum services.</p>
        </div>
        <p className="text-textMuted text-sm">
          Fleet <strong className="text-textPrimary text-lg tabular-nums">{formatHashrate(fleet)}</strong> ·{' '}
          {formatInteger(devices.length)} active
        </p>
      </header>

      {devices.length === 0 ? (
        <p className="border-accent/30 bg-surface/80 text-textMuted rounded-2xl border p-5 text-sm">
          {mining ? 'No miners connected.' : 'Waiting for the mining feed…'}
        </p>
      ) : (
        <div className="flex flex-wrap items-start gap-6">
          <nav
            aria-label="Devices"
            className="border-accent/30 bg-surface/80 flex min-w-0 flex-[1_1_260px] flex-col gap-1 rounded-2xl border p-2"
          >
            {services.map((service) => (
              <div key={service} className="flex flex-col gap-1">
                <p className="text-accentSecondary px-3 pt-2 pb-1 text-xs tracking-wide uppercase">{service}</p>
                {devices
                  .filter((entry) => entry.service === service)
                  .map((entry) => (
                    <DeviceButton
                      key={entry.id}
                      dot={deviceDot(entry, mining)}
                      current={device?.id === entry.id}
                      onClick={() => {
                        setChosen(entry.id);
                      }}
                      name={deviceName(entry)}
                      detail={`${entry.client || 'unknown client'} · ${entry.transport}`}
                      rate={formatHashrate(entry.hashrate)}
                    />
                  ))}
              </div>
            ))}
          </nav>

          <section className="min-w-0 flex-[999_1_560px] space-y-4">
            {device && <DeviceDetail device={device} now={now} />}
          </section>
        </div>
      )}

      {mining && mining.blocks.length > 0 && <Blocks mining={mining} now={now} />}
    </div>
  );
}

// The color of a share result: accepted blue, submitted green, an invalid
// share (a fault) red, and any other rejection amber.
function resultColor(result: string): string {
  if (result === 'accepted') return 'text-sky-300';
  if (result === 'submitted') return 'text-emerald-300';
  return result === 'invalid' ? 'text-rose-300' : 'text-amber-300';
}

// The status of a device, green once it has submitted a block, else as its
// most recent share.
function deviceDot(device: MiningDevice, mining: AdminMiningFrame | null): string {
  if (mining?.blocks.some((block) => block.device === device.id)) return 'bg-emerald-300';
  const result = device.shares[0]?.result ?? 'accepted';
  if (result === 'accepted' || result === 'submitted') return 'bg-sky-300';
  return result === 'invalid' ? 'bg-rose-300' : 'bg-amber-300';
}

// The workers of a device, or its identifier without any.
function deviceName(device: MiningDevice): string {
  return device.workers.length > 0 ? device.workers.join(', ') : device.id;
}

function DeviceButton({
  dot,
  current,
  onClick,
  name,
  detail,
  rate,
}: {
  dot: string;
  current: boolean;
  onClick: () => void;
  name: string;
  detail: string;
  rate: string;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      aria-current={current ? 'true' : undefined}
      className={`flex w-full items-center gap-3 rounded-xl border px-3 py-2.5 text-left transition ${
        current
          ? 'border-accent/50 bg-accentSecondary/20'
          : 'hover:bg-accentSecondary/10 border-transparent'
      }`}
    >
      <span className={`h-2.5 w-2.5 shrink-0 rounded-full ${dot}`} aria-hidden />
      <span className="min-w-0 flex-1">
        <span className="text-textPrimary block truncate font-semibold">{name}</span>
        <span className="text-textMuted block truncate text-xs">{detail}</span>
      </span>
      <span className="text-textPrimary text-sm font-medium tabular-nums">{rate}</span>
    </button>
  );
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <dt className="text-accentSecondary text-xs tracking-wide uppercase">{label}</dt>
      <dd className="text-textPrimary mt-0.5 text-xl font-semibold tabular-nums">{value}</dd>
    </div>
  );
}

function DeviceDetail({ device, now }: { device: MiningDevice; now: number }) {
  const history = device.history.map(([time, rate]) => ({ time, rate }));
  const rejections = Object.entries(device.rejections);
  return (
    <>
      <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-5">
        <div className="min-w-0">
          <h3 className="text-accent truncate text-xl font-semibold">{deviceName(device)}</h3>
          <p className="text-textMuted text-sm">
            {device.service} · {device.client || 'unknown client'} · {device.transport} ·{' '}
            <span className="font-mono">{device.address}</span> · connected{' '}
            {formatDuration((now - device.connected) / 1000)} ago
          </p>
        </div>
        <dl className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          <Stat label="Hashrate" value={formatHashrate(device.hashrate)} />
          <Stat label="Difficulty" value={formatDifficulty(device.difficulty)} />
          <Stat label="Accepted" value={formatInteger(device.accepted)} />
          <Stat label="Rejected" value={formatInteger(device.rejected)} />
          <Stat
            label="Last share"
            value={device.last_share === null ? '—' : formatShortRelativeTime(device.last_share / 1000, now)}
          />
        </dl>
        {rejections.length > 0 && (
          <p className="text-textMuted text-xs">
            Rejected by reason:{' '}
            {rejections.map(([reason, count]) => `${reason} ${formatInteger(count)}`).join(' · ')}
          </p>
        )}
        <HashrateChart history={history} span={1800} />
      </div>

      <div className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
        <h3 className="text-accent mb-3 text-lg font-semibold">Recent shares</h3>
        {device.shares.length === 0 ? (
          <p className="text-textMuted text-sm">No share submitted yet.</p>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm tabular-nums">
              <thead className="text-textMuted text-left">
                <tr>
                  <th className="py-1 pr-4 font-normal">Found</th>
                  <th className="py-1 pr-4 font-normal">Template</th>
                  <th className="py-1 pr-4 text-right font-normal">Difficulty</th>
                  <th className="py-1 font-normal">Result</th>
                </tr>
              </thead>
              <tbody>
                {device.shares.map((share, index) => (
                  <tr key={`${String(share.time)}-${String(index)}`} className="border-accent/10 border-t">
                    <td className="py-1 pr-4 whitespace-nowrap">{formatShortRelativeTime(share.time / 1000, now)}</td>
                    <td className="py-1 pr-4">{share.height === 0 ? '—' : formatInteger(share.height)}</td>
                    <td className="py-1 pr-4 text-right">{formatDifficulty(share.difficulty)}</td>
                    <td className="py-1">
                      <span className={resultColor(share.result)}>
                        {share.result === 'accepted'
                          ? 'Accepted'
                          : share.result === 'submitted'
                            ? 'Submitted'
                            : `Rejected: ${share.result}`}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </>
  );
}

function Blocks({ mining, now }: { mining: AdminMiningFrame; now: number }) {
  const names = new Map(mining.devices.map((device) => [device.id, deviceName(device)]));
  return (
    <section className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
      <h3 className="text-accent mb-3 text-lg font-semibold">Blocks found</h3>
      <div className="overflow-x-auto">
        <table className="w-full table-fixed text-sm tabular-nums">
          <colgroup>
            <col className="w-24" />
            <col className="w-28" />
            <col />
            <col className="w-40" />
          </colgroup>
          <thead className="text-textMuted text-left">
            <tr>
              <th className="py-1 pr-4 font-normal">Height</th>
              <th className="py-1 pr-4 font-normal">Found</th>
              <th className="py-1 pr-4 font-normal">Hash</th>
              <th className="py-1 font-normal">Device</th>
            </tr>
          </thead>
          <tbody>
            {mining.blocks.map((block) => (
              <tr key={block.hash} className="border-accent/10 border-t">
                <td className="py-1 pr-4">{formatInteger(block.height)}</td>
                <td className="py-1 pr-4 whitespace-nowrap">{formatShortRelativeTime(block.time / 1000, now)}</td>
                <td data-fit className="overflow-hidden py-1 pr-4 font-mono whitespace-nowrap">
                  <FitHash value={block.hash} />
                </td>
                <td className="truncate py-1">{names.get(block.device) ?? block.device}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}
