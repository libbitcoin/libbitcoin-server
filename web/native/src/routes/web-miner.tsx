import { useEffect, useState, useSyncExternalStore, type ReactNode } from 'react';
import { Link } from 'react-router-dom';
import { HashrateChart } from '@/components/hashrate-chart';
import { MiningSettings, type PageSettings } from '@/components/mining-settings';
import { AddressQRCode } from '@/components/qr-code';
import { useServerConfiguration } from '@/lib/hooks/useBlockchainData';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { miner, type MinerSettings, type MinerState, type Share } from '@/lib/mining/miner';
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
import { serviceSocketUrl } from '@/lib/utils/services';

const LINK = 'text-accent hover:underline';

const STORAGE_KEY = 'libbitcoin.miner';

// A share is expected slower than this, in seconds, before a warning shows.
const SLOW = 60 * 60;

function cores(): number {
  return Math.max(1, globalThis.navigator?.hardwareConcurrency || 2);
}

function defaults(): PageSettings {
  return {
    url: '',
    worker: 'browser',
    password: '',
    threads: Math.max(1, cores() - 1),
    address: '',
    tag: '',
  };
}

function load(): PageSettings {
  const base = defaults();
  try {
    const saved = JSON.parse(globalThis.localStorage.getItem(STORAGE_KEY) ?? '{}') as Partial<PageSettings>;
    return {
      url: typeof saved.url === 'string' ? saved.url : base.url,
      worker: typeof saved.worker === 'string' && saved.worker ? saved.worker : base.worker,
      password: typeof saved.password === 'string' ? saved.password : base.password,
      address: typeof saved.address === 'string' ? saved.address : base.address,
      tag: typeof saved.tag === 'string' ? saved.tag : base.tag,
      threads:
        typeof saved.threads === 'number' && saved.threads >= 1
          ? Math.min(saved.threads, cores())
          : base.threads,
    };
  } catch {
    return base;
  }
}

function save(settings: PageSettings) {
  try {
    globalThis.localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch {
    // Settings are a convenience, mining works without them.
  }
}

// Mines the node's block templates in this browser, as a client of its stratum
// v1 service over a websocket. Mining continues across page navigation.
export function WebMinerPage() {
  usePageTitle('Web Miner');
  const state = useSyncExternalStore(miner.subscribe, miner.getSnapshot, miner.getSnapshot);
  const [settings, setSettings] = useState<PageSettings>(load);
  const configuration = useServerConfiguration().data;
  const stratum = configuration?.server.stratum_v1;
  const advertised = serviceSocketUrl(stratum);
  const payout = stratum?.payout_address;
  const coinbaseTag = stratum?.coinbase_tag;
  const [editing, setEditing] = useState(false);
  // Events are newer than the last tick of the clock.
  const now = Math.max(useNow(1_000), Date.now());
  const running = state.status === 'connecting' || state.status === 'waiting' || state.status === 'mining';

  useEffect(() => {
    save(settings);
  }, [settings]);

  // The node's advertised service and payout take precedence over the page.
  const effective = (page: PageSettings): MinerSettings => ({
    url: advertised ?? page.url,
    worker: payout ? page.worker : page.address,
    password: page.password,
    threads: page.threads,
  });

  const toggle = () => {
    if (running) miner.stop();
    else miner.start(effective(settings));
  };

  // Saved settings apply to a running miner by restarting it.
  const onSave = (next: PageSettings) => {
    setSettings(next);
    setEditing(false);
    if (running) miner.start(effective(next));
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
        settings={effective(settings)}
        address={payout ?? settings.address}
        tag={coinbaseTag ?? settings.tag}
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
        require={{ url: !advertised, address: !payout, tag: !coinbaseTag }}
        wallet={configuration?.wallet}
        network={configuration?.network}
        cores={cores()}
        onSave={onSave}
        onClose={() => {
          setEditing(false);
        }}
      />
    </div>
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
  address,
  tag,
  running,
  now,
  onToggle,
  onEdit,
}: {
  state: MinerState;
  settings: MinerSettings;
  address: string;
  tag: string;
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
      <JobPanel state={state} address={address} tag={tag} now={now} />

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
              <span className="font-mono">{settings.url || '—'}</span> · {formatInteger(settings.threads)} threads
            </p>
          </div>
          <div className="flex flex-wrap items-center gap-3">
            <button
              type="button"
              onClick={onEdit}
              className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent inline-flex items-center gap-2 rounded-md border px-4 py-1.5 text-sm"
            >
              <CogIcon />
              Settings
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

      <div className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
        <h3 className="text-accent mb-3 text-lg font-semibold">Recent shares</h3>
        {state.shares.length === 0 ? (
          <p className="text-textMuted text-sm">
            {running ? 'No share found yet.' : 'Start mining to submit shares.'}
          </p>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full min-w-[44rem] table-fixed text-sm tabular-nums">
              <colgroup>
                <col className="w-24" />
                <col className="w-24" />
                <col className="w-28" />
                <col className="w-28" />
                <col />
                <col className="w-36" />
              </colgroup>
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

// The current job, and the payout address its coinbase pays, labeled by tag.
function JobPanel({ state, address, tag, now }: { state: MinerState; address: string; tag: string; now: number }) {
  const job = state.job;
  return (
    <div className="border-accent/30 bg-surface/80 rounded-2xl border p-5">
      <div className="flex flex-col gap-5 sm:flex-row sm:items-start">
        <div className="min-w-0 flex-1">
          <h3 className="text-accent mb-4 text-lg font-semibold">Current job</h3>
          {job ? (
            <dl className="grid gap-4 text-sm sm:grid-cols-2 lg:grid-cols-3">
              <Item label="Template">
                {job.height === undefined ? (
                  '—'
                ) : (
                  <Link to={templatePath(job.height)} className={LINK}>
                    {formatInteger(job.height)}
                  </Link>
                )}
              </Item>
              <Item label="Previous block">
                <Link to={blockByHashPath(job.previous)} className={`${LINK} font-mono`} title={job.previous}>
                  {formatHash(job.previous)}
                </Link>
              </Item>
              <Item label="Coinbase tag">{job.tag ?? '—'}</Item>
              <Item label="Network difficulty">{formatDifficulty(job.networkDifficulty)}</Item>
              <Item label="Job">
                <span className="font-mono">{job.id}</span>
                {job.clean && <span className="text-textMuted"> · clean</span>}
              </Item>
              <Item label="Received">{formatShortRelativeTime(Math.floor(job.received / 1000), now)}</Item>
            </dl>
          ) : (
            <p className="text-textMuted text-sm">Start mining to receive a job.</p>
          )}
        </div>
        {address && (
          <figure className="w-40 shrink-0 space-y-2 self-center sm:self-start">
            <AddressQRCode value={address} size={144} />
            <figcaption className="text-textPrimary text-center text-sm font-medium break-words">
              {tag || '—'}
            </figcaption>
            <p className="text-textMuted font-mono text-xs break-all" title={address}>
              {address}
            </p>
          </figure>
        )}
      </div>
    </div>
  );
}

function ShareRow({ share, now }: { share: Share; now: number }) {
  return (
    <tr className="border-accent/10 border-t">
      <td className="py-1 pr-4 whitespace-nowrap">{formatShortRelativeTime(Math.floor(share.time / 1000), now)}</td>
      <td className="py-1 pr-4">{share.height === undefined ? '—' : formatInteger(share.height)}</td>
      <td className="py-1 pr-4 font-mono">{share.nonce}</td>
      <td className="py-1 pr-4 text-right">{formatDifficulty(share.difficulty)}</td>
      <td className="truncate py-1 pr-4 font-mono whitespace-nowrap" title={share.hash}>
        {share.block ? (
          <Link to={blockByHashPath(share.hash)} className={LINK}>
            {formatHash(share.hash)}
          </Link>
        ) : (
          formatHash(share.hash)
        )}
      </td>
      <td className="truncate py-1 whitespace-nowrap">
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
