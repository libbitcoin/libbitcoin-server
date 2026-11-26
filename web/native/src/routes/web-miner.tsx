import { useEffect, useState, useSyncExternalStore, type ReactNode } from 'react';
import { Link } from 'react-router-dom';
import { HashrateChart } from '@/components/hashrate-chart';
import { MiningSettings } from '@/components/mining-settings';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { miner, type MinerSettings, type MinerState, type MinerStatus, type Share } from '@/lib/mining/miner';
import {
  formatDifficulty,
  formatDuration,
  formatHash,
  formatHashrate,
  formatShortRelativeTime,
  shareSeconds,
} from '@/lib/mining/format';
import { formatInteger } from '@/lib/utils/format';
import { blockByHashPath, templatePath } from '@/lib/utils/routes';

const LINK = 'text-accent hover:underline';

const STORAGE_KEY = 'libbitcoin.miner';

// A share is expected slower than this, in seconds, before a warning shows.
const SLOW = 60 * 60;

function cores(): number {
  return Math.max(1, globalThis.navigator?.hardwareConcurrency || 2);
}

function defaults(): MinerSettings {
  const secure = globalThis.location?.protocol === 'https:';
  return {
    url: `${secure ? 'wss' : 'ws'}://${globalThis.location?.hostname || 'localhost'}:3333`,
    worker: 'browser',
    password: 'x',
    threads: Math.max(1, cores() - 1),
  };
}

function load(): MinerSettings {
  const base = defaults();
  try {
    const saved = JSON.parse(globalThis.localStorage.getItem(STORAGE_KEY) ?? '{}') as Partial<MinerSettings>;
    return {
      url: typeof saved.url === 'string' && saved.url ? saved.url : base.url,
      worker: typeof saved.worker === 'string' && saved.worker ? saved.worker : base.worker,
      password: typeof saved.password === 'string' ? saved.password : base.password,
      threads:
        typeof saved.threads === 'number' && saved.threads >= 1
          ? Math.min(saved.threads, cores())
          : base.threads,
    };
  } catch {
    return base;
  }
}

function save(settings: MinerSettings) {
  try {
    globalThis.localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch {
    // Settings are a convenience, mining works without them.
  }
}

const STATUS: Record<MinerStatus, { label: string; style: string }> = {
  stopped: { label: 'Stopped', style: 'bg-slate-500/20 text-slate-300' },
  connecting: { label: 'Connecting', style: 'bg-amber-500/20 text-amber-300' },
  waiting: { label: 'Waiting for work', style: 'bg-amber-500/20 text-amber-300' },
  mining: { label: 'Mining', style: 'bg-emerald-500/20 text-emerald-300' },
  failed: { label: 'Failed', style: 'bg-rose-500/20 text-rose-300' },
};

// Mines the node's block templates in this browser, as a client of its stratum
// v1 service over a websocket. Mining continues across page navigation.
export function WebMinerPage() {
  usePageTitle('Web Miner');
  const state = useSyncExternalStore(miner.subscribe, miner.getSnapshot, miner.getSnapshot);
  const [settings, setSettings] = useState<MinerSettings>(load);
  const [editing, setEditing] = useState(false);
  // Events are newer than the last tick of the clock.
  const now = Math.max(useNow(1_000), Date.now());
  const running = state.status === 'connecting' || state.status === 'waiting' || state.status === 'mining';

  useEffect(() => {
    save(settings);
  }, [settings]);

  const toggle = () => {
    if (running) miner.stop();
    else miner.start(settings);
  };

  // Saved settings apply to a running miner by restarting it.
  const onSave = (next: MinerSettings) => {
    setSettings(next);
    setEditing(false);
    if (running) miner.start(next);
  };

  return (
    <div className="space-y-6">
      <header className="space-y-2">
        <p className="text-accentSecondary text-sm tracking-wide uppercase">Mining</p>
        <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
          Web Miner
        </h1>
      </header>

      <MinerDetail
        state={state}
        settings={settings}
        running={running}
        now={now}
        onToggle={toggle}
        onEdit={() => {
          setEditing(true);
        }}
      />

      <MiningSettings
        open={editing}
        settings={settings}
        cores={cores()}
        onSave={onSave}
        onClose={() => {
          setEditing(false);
        }}
      />
    </div>
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

function MinerDetail({
  state,
  settings,
  running,
  now,
  onToggle,
  onEdit,
}: {
  state: MinerState;
  settings: MinerSettings;
  running: boolean;
  now: number;
  onToggle: () => void;
  onEdit: () => void;
}) {
  const perShare = shareSeconds(state.difficulty, state.hashrate);
  const insecure =
    globalThis.location?.protocol === 'https:' && settings.url.trim().toLowerCase().startsWith('ws://');
  return (
    <>
      {state.status === 'failed' && state.message && (
        <Notice title="Mining stopped">{state.message}</Notice>
      )}
      {state.status === 'connecting' && state.message && (
        <Notice title="Connection lost">{state.message}</Notice>
      )}
      {insecure && !running && (
        <Notice title="Secure websocket required">
          A page served over https can only reach a wss:// stratum service. Bind a safe stratum
          endpoint with a certificate and use its wss:// address.
        </Notice>
      )}
      {state.status === 'mining' && state.hashrate > 0 && perShare > SLOW && (
        <Notice title="Shares are out of reach">
          At {formatHashrate(state.hashrate)} and difficulty {formatDifficulty(state.difficulty)} this
          browser is expected to find a share every {formatDuration(perShare)}. The share difficulty
          never drops below the minimum difficulty of the node&apos;s stratum service.
        </Notice>
      )}

      <div className="border-accent/30 bg-surface/80 space-y-5 rounded-2xl border p-5">
        <div className="flex flex-wrap items-center justify-between gap-3">
          <div className="min-w-0">
            <h2 className="text-accent text-xl font-semibold">This browser</h2>
            <p className="text-textMuted text-sm break-all">
              web miner · {settings.worker} · <span className="font-mono">{settings.url}</span> ·{' '}
              {formatInteger(settings.threads)} threads
            </p>
          </div>
          <div className="flex flex-wrap items-center gap-3">
            <Pill label={STATUS[state.status].label} style={STATUS[state.status].style} />
            <button
              type="button"
              onClick={onEdit}
              className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent inline-flex items-center gap-2 rounded-md border px-4 py-1.5 text-sm"
            >
              <CogIcon />
              Miner settings
            </button>
            <button
              type="button"
              onClick={onToggle}
              disabled={!running && (!settings.url || !settings.worker)}
              className={`rounded-md border px-5 py-1.5 text-sm font-medium transition disabled:cursor-not-allowed disabled:opacity-50 ${
                running
                  ? 'border-accent/40 text-textPrimary hover:border-accent hover:text-accent'
                  : 'border-accent bg-accent/20 text-accent hover:bg-accent/30'
              }`}
            >
              {running ? 'Stop' : 'Start'}
            </button>
          </div>
        </div>
        <dl className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-5">
          <Stat label="Hashrate" value={running ? formatHashrate(state.hashrate) : '—'} />
          <Stat label="Difficulty" value={formatDifficulty(state.difficulty)} />
          <Stat
            label="Per share"
            value={state.status === 'mining' && state.hashrate > 0 ? formatDuration(perShare) : '—'}
          />
          <Stat label="Accepted" value={formatInteger(state.accepted)} />
          <Stat label="Rejected" value={formatInteger(state.rejected)} />
        </dl>
        <HashrateChart history={state.history} />
      </div>

      {state.job && (
        <div className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
          <h3 className="text-accent mb-4 text-lg font-semibold">Current job</h3>
          <dl className="grid gap-4 text-sm sm:grid-cols-2 lg:grid-cols-3">
            <Item label="Template">
              {state.job.height === undefined ? (
                '—'
              ) : (
                <Link to={templatePath(state.job.height)} className={LINK}>
                  {formatInteger(state.job.height)}
                </Link>
              )}
            </Item>
            <Item label="Previous block">
              <Link
                to={blockByHashPath(state.job.previous)}
                className={`${LINK} font-mono`}
                title={state.job.previous}
              >
                {formatHash(state.job.previous)}
              </Link>
            </Item>
            <Item label="Coinbase tag">{state.job.tag ?? '—'}</Item>
            <Item label="Network difficulty">{formatDifficulty(state.job.networkDifficulty)}</Item>
            <Item label="Job">
              <span className="font-mono">{state.job.id}</span>
              {state.job.clean && <span className="text-textMuted"> · clean</span>}
            </Item>
            <Item label="Received">{formatShortRelativeTime(Math.floor(state.job.received / 1000), now)}</Item>
          </dl>
        </div>
      )}

      <div className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
        <h3 className="text-accent mb-3 text-lg font-semibold">Recent shares</h3>
        {state.shares.length === 0 ? (
          <p className="text-textMuted text-sm">
            {running ? 'No share found yet.' : 'Start mining to submit shares.'}
          </p>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm tabular-nums">
              <thead className="text-textMuted text-left">
                <tr>
                  <th className="py-1 pr-4 font-normal">Found</th>
                  <th className="py-1 pr-4 font-normal">Template</th>
                  <th className="py-1 pr-4 font-normal">Nonce</th>
                  <th className="py-1 pr-4 text-right font-normal">Difficulty</th>
                  <th className="py-1 pr-4 font-normal">Hash</th>
                  <th className="py-1 font-normal">Result</th>
                </tr>
              </thead>
              <tbody>
                {state.shares.map((share) => (
                  <ShareRow key={share.key} share={share} now={now} />
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </>
  );
}

function ShareRow({ share, now }: { share: Share; now: number }) {
  return (
    <tr className="border-accent/10 border-t">
      <td className="py-1 pr-4 whitespace-nowrap">{formatShortRelativeTime(Math.floor(share.time / 1000), now)}</td>
      <td className="py-1 pr-4">{share.height === undefined ? '—' : formatInteger(share.height)}</td>
      <td className="py-1 pr-4 font-mono">{share.nonce}</td>
      <td className="py-1 pr-4 text-right">{formatDifficulty(share.difficulty)}</td>
      <td className="py-1 pr-4 font-mono whitespace-nowrap" title={share.hash}>
        {share.block ? (
          <Link to={blockByHashPath(share.hash)} className={LINK}>
            {formatHash(share.hash)}
          </Link>
        ) : (
          formatHash(share.hash)
        )}
      </td>
      <td className="py-1 whitespace-nowrap">
        <Outcome share={share} />
      </td>
    </tr>
  );
}

function Outcome({ share }: { share: Share }) {
  if (share.outcome === 'pending') return <span className="text-textMuted">Pending…</span>;
  if (share.outcome === 'unanswered') return <span className="text-amber-300">No answer</span>;
  if (share.outcome === 'rejected') {
    return <span className="text-rose-300">Rejected{share.reason ? `: ${share.reason}` : ''}</span>;
  }
  return (
    <span className="text-emerald-300">
      Accepted
      {share.block && (
        <span className="ml-2 rounded-full bg-emerald-500/20 px-2 py-0.5 text-xs uppercase">Block</span>
      )}
    </span>
  );
}

function Notice({ title, children }: { title: string; children: ReactNode }) {
  return (
    <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
      <p className="text-accent font-semibold">{title}</p>
      <p className="text-textMuted mt-1 break-words">{children}</p>
    </div>
  );
}

function Item({ label, children }: { label: string; children: ReactNode }) {
  return (
    <div>
      <dt className="text-textMuted text-xs tracking-wide uppercase">{label}</dt>
      <dd className="text-textPrimary mt-1 font-medium tabular-nums">{children}</dd>
    </div>
  );
}

function CogIcon() {
  return (
    <svg viewBox="0 0 24 24" className="h-4 w-4" fill="none" stroke="currentColor" strokeWidth={1.5} aria-hidden="true">
      <path
        strokeLinecap="round"
        strokeLinejoin="round"
        d="M10.343 3.94c.09-.542.56-.94 1.11-.94h1.093c.55 0 1.02.398 1.11.94l.149.894c.07.424.384.764.78.93.398.164.855.142 1.205-.108l.737-.527a1.125 1.125 0 0 1 1.45.12l.773.774c.39.389.44 1.002.12 1.45l-.527.737c-.25.35-.272.806-.107 1.204.165.397.505.71.93.78l.893.15c.543.09.94.559.94 1.109v1.094c0 .55-.397 1.02-.94 1.11l-.894.149c-.424.07-.764.383-.929.78-.165.398-.143.854.107 1.204l.527.738c.32.447.269 1.06-.12 1.45l-.774.773a1.125 1.125 0 0 1-1.449.12l-.738-.527c-.35-.25-.806-.272-1.203-.107-.398.165-.71.505-.781.929l-.149.894c-.09.542-.56.94-1.11.94h-1.094c-.55 0-1.019-.398-1.11-.94l-.148-.894c-.071-.424-.384-.764-.781-.93-.398-.164-.854-.142-1.204.108l-.738.527c-.447.32-1.06.269-1.45-.12l-.773-.774a1.125 1.125 0 0 1-.12-1.45l.527-.737c.25-.35.272-.806.108-1.204-.165-.397-.506-.71-.93-.78l-.894-.15c-.542-.09-.94-.56-.94-1.109v-1.094c0-.55.398-1.02.94-1.11l.894-.149c.424-.07.765-.383.93-.78.165-.398.143-.854-.108-1.204l-.526-.738a1.125 1.125 0 0 1 .12-1.45l.773-.773a1.125 1.125 0 0 1 1.45-.12l.737.527c.35.25.807.272 1.204.107.397-.165.71-.505.78-.929l.15-.894Z"
      />
      <path strokeLinecap="round" strokeLinejoin="round" d="M15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0Z" />
    </svg>
  );
}
