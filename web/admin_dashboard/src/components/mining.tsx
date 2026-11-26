import { useMemo, useState } from 'react';
import { HashrateChart } from '@/components/hashrate-chart';
import { useCurrentTime } from '@/lib/use-current-time';
import { formatInteger } from '@/lib/format';
import { SAMPLE_DEVICES, sampleHistory, type SampleDevice } from '@/lib/mining/sample';
import {
  formatDifficulty,
  formatDuration,
  formatHashrate,
  formatShortRelativeTime,
} from '@/lib/mining/format';

// The node's mining console: the devices connected to its mining services, by
// service, and the detail of the one chosen. Devices are samples until the node
// reports the sessions of its stratum services.
export function Mining() {
  const [chosen, setChosen] = useState(SAMPLE_DEVICES[0]?.id);
  const now = useCurrentTime();
  const device = SAMPLE_DEVICES.find((entry) => entry.id === chosen) ?? SAMPLE_DEVICES[0];
  const fleet = SAMPLE_DEVICES.reduce((sum, entry) => sum + entry.rate, 0);
  const services = [...new Set(SAMPLE_DEVICES.map((entry) => entry.service))];

  return (
    <div className="space-y-6">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div className="space-y-1">
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Mining</p>
          <h2 className="text-accent text-2xl font-semibold">Devices</h2>
        </div>
        <p className="text-textMuted text-sm">
          Fleet <strong className="text-textPrimary text-lg tabular-nums">{formatHashrate(fleet)}</strong> ·{' '}
          {formatInteger(SAMPLE_DEVICES.length)} active
        </p>
      </header>

      <div className="flex flex-wrap items-start gap-6">
        <nav
          aria-label="Devices"
          className="border-accent/30 bg-surface/80 flex flex-[1_1_260px] flex-col gap-1 rounded-2xl border p-2"
        >
          {services.map((service) => (
            <div key={service} className="flex flex-col gap-1">
              <p className="text-accentSecondary px-3 pt-2 pb-1 text-xs tracking-wide uppercase">{service}</p>
              {SAMPLE_DEVICES.filter((entry) => entry.service === service).map((entry) => (
                <DeviceButton
                  key={entry.id}
                  current={device?.id === entry.id}
                  onClick={() => {
                    setChosen(entry.id);
                  }}
                  dot="bg-emerald-300"
                  name={entry.worker}
                  detail={`${entry.client} · ${entry.transport}`}
                  rate={formatHashrate(entry.rate)}
                  sample
                />
              ))}
            </div>
          ))}
          <p className="text-textMuted px-3 pt-2 pb-1 text-xs">
            Sample devices are shown until the node reports the sessions of its stratum services.
          </p>
        </nav>

        <section className="min-w-0 flex-[999_1_560px] space-y-4">
          {device && <SampleDetail device={device} now={now} />}
        </section>
      </div>
    </div>
  );
}

function DeviceButton({
  current,
  onClick,
  dot,
  name,
  detail,
  rate,
  sample = false,
}: {
  current: boolean;
  onClick: () => void;
  dot: string;
  name: string;
  detail: string;
  rate: string;
  sample?: boolean;
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
        <span className="text-textPrimary block font-semibold">
          {name}
          {sample && (
            <span className="text-textMuted ml-2 text-[10px] font-normal tracking-wide uppercase">sample</span>
          )}
        </span>
        <span className="text-textMuted block truncate text-xs">{detail}</span>
      </span>
      <span className="text-textPrimary text-sm font-medium tabular-nums">{rate}</span>
    </button>
  );
}

function Pill({ label, style }: { label: string; style: string }) {
  return (
    <span
      className={`inline-flex w-fit items-center gap-2 rounded-full px-3 py-1 text-xs font-medium tracking-wide whitespace-nowrap uppercase ${style}`}
    >
      <span className="h-2 w-2 rounded-full bg-current" aria-hidden />
      {label}
    </span>
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

function SampleDetail({ device, now }: { device: SampleDevice; now: number }) {
  const minute = Math.floor(now / 60_000) * 60_000;
  const history = useMemo(() => sampleHistory(device, minute), [device, minute]);
  return (
    <>
      <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-5">
        <div className="flex flex-wrap items-center justify-between gap-3">
          <div className="min-w-0">
            <h3 className="text-accent text-xl font-semibold">
              {device.worker}
              <span className="text-textMuted ml-2 text-xs font-normal tracking-wide uppercase">sample data</span>
            </h3>
            <p className="text-textMuted text-sm">
              {device.service} · {device.client} · {device.transport} ·{' '}
              <span className="font-mono">{device.address}</span> · connected{' '}
              {formatDuration(device.connected)} ago
            </p>
          </div>
          <Pill label="Active" style="bg-emerald-500/20 text-emerald-300" />
        </div>
        <dl className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          <Stat label="Hashrate" value={formatHashrate(device.rate)} />
          <Stat label="Difficulty" value={formatDifficulty(device.difficulty)} />
          <Stat label="Accepted" value={formatInteger(device.accepted)} />
          <Stat label="Rejected" value={formatInteger(device.rejected)} />
          <Stat label="Last share" value={`${formatInteger(device.lastShare)} s ago`} />
        </dl>
        <HashrateChart history={history} span={1800} />
      </div>

      <div className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
        <h3 className="text-accent mb-3 text-lg font-semibold">Recent shares</h3>
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
              {device.shares.map((share) => (
                <tr key={share.ago} className="border-accent/10 border-t">
                  <td className="py-1 pr-4">{formatShortRelativeTime(Math.floor(now / 1000) - share.ago, now)}</td>
                  <td className="py-1 pr-4">{formatInteger(share.template)}</td>
                  <td className="py-1 pr-4 text-right">{formatDifficulty(share.difficulty)}</td>
                  <td className="py-1">
                    {share.rejected ? (
                      <span className="text-rose-300">Rejected: {share.rejected}</span>
                    ) : (
                      <span className="text-emerald-300">Accepted</span>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </>
  );
}
