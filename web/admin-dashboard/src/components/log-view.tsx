import { memo, useEffect, useRef, useState, useSyncExternalStore } from 'react';
import {
  ALL_LOG_FILTER,
  OPERATIONAL_LOG_FILTER,
  LOG_LEVELS,
  getLogLevel,
  includesLogLevel,
} from '@/lib/admin-contract';
import { formatIsoTimestamp, formatTimestamp } from '@/lib/format';
import type { MetricsSnapshot, RecentLog } from '@/lib/live-metrics';
import type { ConnectionStatus } from '@/lib/reconnecting-websocket';
import type { WorkerMetricsStore } from '@/lib/worker-metrics-store';

const ROW_HEIGHT = 32;

interface LogViewProps {
  metrics: Pick<WorkerMetricsStore, 'getLogWindow' | 'subscribeLogs' | 'setLogQuery'>;
  delivery: Pick<MetricsSnapshot, 'totalLogs' | 'evictedLogs' | 'clearedLogs' | 'invalidFrames'>;
  status: ConnectionStatus;
  logFilter: number;
  paused: boolean;
  onLogFilterChange: (filter: number) => void;
  onPausedChange: (paused: boolean) => void;
  onClear: () => void;
}

const levelStyles: Record<number, string> = {
  0: 'bg-sky-500/15 text-sky-300',
  1: 'bg-emerald-500/15 text-emerald-300',
  2: 'bg-violet-500/15 text-violet-300',
  3: 'bg-cyan-500/15 text-cyan-300',
  4: 'bg-indigo-500/15 text-indigo-300',
  5: 'bg-blue-500/15 text-blue-300',
  6: 'bg-red-500/20 text-red-300',
  7: 'bg-orange-500/20 text-orange-300',
  8: 'bg-slate-500/20 text-slate-300',
  9: 'bg-zinc-500/20 text-zinc-300',
};

export function LogView({
  metrics,
  delivery,
  status,
  logFilter,
  paused,
  onLogFilterChange,
  onPausedChange,
  onClear,
}: LogViewProps) {
  const [search, setSearch] = useState('');
  const [scroll, setScroll] = useState({ offset: 0, search, filter: logFilter });
  const offset = scroll.search === search && scroll.filter === logFilter ? scroll.offset : 0;
  const [selected, setSelected] = useState<RecentLog | null>(null);
  const viewport = useRef<HTMLDivElement>(null);
  const window = useSyncExternalStore(
    metrics.subscribeLogs,
    metrics.getLogWindow,
    metrics.getLogWindow
  );

  useEffect(() => {
    // eslint-disable-next-line react-you-might-not-need-an-effect/no-pass-live-state-to-parent, react-you-might-not-need-an-effect/no-pass-data-to-parent -- Synchronize a worker query, not React parent state.
    metrics.setLogQuery({ search, filter: logFilter, offset });
  }, [metrics, search, logFilter, offset]);

  useEffect(() => {
    if (viewport.current) viewport.current.scrollTop = 0;
  }, [search, logFilter]);

  return (
    <section className="space-y-4">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-xs tracking-wide uppercase">Diagnostic stream</p>
          <h2 className="text-accent mt-1 text-2xl font-semibold">Node logs</h2>
          <p className="text-textMuted mt-1 text-sm">
            The newest 1,000 messages are retained in this browser session.
          </p>
        </div>
        <div className="flex gap-2">
          <button
            type="button"
            onClick={() => {
              if (paused) {
                if (viewport.current) viewport.current.scrollTop = 0;
                setScroll({ offset: 0, search, filter: logFilter });
              }
              onPausedChange(!paused);
            }}
            className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
          >
            {paused ? 'Resume display' : 'Pause display'}
          </button>
          <button
            type="button"
            onClick={onClear}
            className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
          >
            Clear
          </button>
        </div>
      </header>

      <fieldset className="border-accent/30 bg-surface/80 space-y-3 rounded-2xl border p-4">
        <legend className="text-textPrimary px-1 text-sm font-medium">Requested log levels</legend>
        <p className="text-textMuted text-xs">
          Only checked levels are sent by the server. Pause freezes this view; collection and charts
          continue. Skipped log levels are not replayed.
        </p>
        <div className="flex flex-wrap gap-x-5 gap-y-3">
          {LOG_LEVELS.map((item) => (
            <label key={item.id} className="text-textPrimary flex items-center gap-2 text-sm">
              <input
                type="checkbox"
                checked={includesLogLevel(logFilter, item.id)}
                onChange={() => {
                  onLogFilterChange(logFilter ^ (1 << item.id));
                }}
                className="border-accent/35 bg-background text-accent focus:ring-accent rounded"
              />
              {item.label}
            </label>
          ))}
        </div>
        <div className="flex flex-wrap items-center gap-3 text-xs">
          <button
            type="button"
            onClick={() => {
              onLogFilterChange(OPERATIONAL_LOG_FILTER);
            }}
            className="text-accent hover:underline"
          >
            Operational levels
          </button>
          <button
            type="button"
            onClick={() => {
              onLogFilterChange(ALL_LOG_FILTER);
            }}
            className="text-accent hover:underline"
          >
            All levels
          </button>
          <button
            type="button"
            onClick={() => {
              onLogFilterChange(0);
            }}
            className="text-accent hover:underline"
          >
            No levels
          </button>
          <span className="text-textMuted">Selection is saved in this browser.</span>
        </div>
      </fieldset>

      <div className="border-accent/30 bg-surface/80 space-y-2 rounded-2xl border p-4 text-xs">
        <dl aria-label="Log delivery statistics" className="flex flex-wrap gap-x-6 gap-y-2">
          {[
            ['Received logs', delivery.totalLogs],
            ['Rolled out of history', delivery.evictedLogs],
            ['Cleared from history', delivery.clearedLogs],
            ['Invalid admin frames', delivery.invalidFrames],
            ['Reconnects', status.reconnects],
          ].map(([label, value]) => (
            <div key={label}>
              <dt className="text-textMuted">{label}</dt>
              <dd className="text-textPrimary mt-1 font-mono tabular-nums">
                {value.toLocaleString()}
              </dd>
            </div>
          ))}
        </dl>
        <p className="text-textMuted">
          Live rows refresh every 100 ms and can skip intermediate messages on screen. Every valid
          log at a requested level is counted; older messages leave the 1,000-message history as new
          ones arrive. These counters continue while display is paused. Reconnects can leave gaps
          that the server does not replay.
        </p>
        <p className="text-textMuted" role="status">
          Admin connection: {status.phase}
          {status.error ? ` · ${status.error}` : ''}
        </p>
      </div>

      <div className="border-accent/30 bg-surface/80 flex flex-wrap gap-3 rounded-2xl border p-4">
        <label className="min-w-56 flex-1">
          <span className="sr-only">Search logs</span>
          <input
            type="search"
            value={search}
            onChange={(event) => {
              setSearch(event.target.value);
            }}
            placeholder="Search received messages…"
            className="border-accent/40 bg-background text-textPrimary placeholder:text-textMuted/60 focus:border-accent w-full rounded-md border px-3 py-2 text-sm focus:ring-0"
          />
        </label>
        <p className="text-textMuted self-center text-xs">Search filters received messages only.</p>
      </div>

      <div className="border-accent/30 bg-surface/80 overflow-hidden rounded-xl border">
        <div className="border-accentSecondary/30 bg-accentSecondary/20 text-textMuted flex justify-between border-b px-4 py-3 text-xs">
          <span>
            {paused
              ? 'Display paused · collection continues'
              : logFilter === 0
                ? 'Log delivery disabled'
                : offset > 0
                  ? 'Viewing older rows · collection continues'
                  : 'Live stream · newest first'}
          </span>
          <span>
            {String(window.matched)} matching · {String(window.retained)} retained
          </span>
        </div>
        <div
          ref={viewport}
          role="region"
          aria-label="Scrollable log history"
          tabIndex={0}
          className="h-[min(60vh,32rem)] min-h-32 overflow-auto"
          style={{ overflowAnchor: 'none' }}
          onScroll={(event) => {
            setScroll({
              offset: Math.max(0, Math.floor(event.currentTarget.scrollTop / ROW_HEIGHT) - 8),
              search,
              filter: logFilter,
            });
          }}
        >
          {window.matched === 0 ? (
            <p className="text-textMuted px-4 py-12 text-center text-sm">
              No matching log messages.
            </p>
          ) : (
            <ol
              aria-label="Received logs"
              className="relative"
              style={{ height: window.matched * ROW_HEIGHT }}
            >
              {window.logs.map((log, index) => (
                <LogRow
                  key={log.id}
                  log={log}
                  index={window.offset + index}
                  total={window.matched}
                  onSelect={setSelected}
                />
              ))}
            </ol>
          )}
        </div>
      </div>
      <div className="flex items-center justify-between gap-3 text-xs">
        <p className="text-textMuted">Select a row to read its full message.</p>
        {!paused && (
          <button
            type="button"
            className="text-accent hover:underline"
            onClick={() => {
              if (viewport.current) viewport.current.scrollTop = 0;
              setScroll({ offset: 0, search, filter: logFilter });
            }}
          >
            Jump to latest
          </button>
        )}
      </div>
      {selected && (
        <LogDetail
          log={selected}
          onClose={() => {
            setSelected(null);
          }}
        />
      )}
    </section>
  );
}

const LogRow = memo(function LogRow({
  log,
  index,
  total,
  onSelect,
}: {
  log: RecentLog;
  index: number;
  total: number;
  onSelect: (log: RecentLog) => void;
}) {
  return (
    <li
      aria-setsize={total}
      aria-posinset={index + 1}
      className="border-accentSecondary/20 absolute inset-x-0 border-b"
      style={{ top: index * ROW_HEIGHT, height: ROW_HEIGHT }}
    >
      <button
        type="button"
        onClick={() => {
          onSelect(log);
        }}
        className="hover:bg-accentSecondary/10 focus-visible:bg-accentSecondary/20 grid h-full w-full grid-cols-[5.5rem_4.5rem_minmax(0,1fr)] items-center gap-2 px-4 text-left text-xs sm:grid-cols-[7.5rem_7.5rem_minmax(0,1fr)]"
      >
        <time
          dateTime={formatIsoTimestamp(log.time)}
          className="text-textMuted font-mono tabular-nums"
        >
          {formatTimestamp(log.time, false)}
        </time>
        <span
          className={`w-fit rounded px-2 py-0.5 font-medium ${levelStyles[log.level] ?? levelStyles[8]}`}
        >
          {getLogLevel(log.level).name}
        </span>
        <span className="text-textPrimary min-w-0 truncate font-mono">
          {log.message.replaceAll('\n', ' ↵ ')}
        </span>
      </button>
    </li>
  );
});

function LogDetail({ log, onClose }: { log: RecentLog; onClose: () => void }) {
  const dialog = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    const element = dialog.current;
    element?.showModal();
    return () => {
      element?.close();
    };
  }, []);
  return (
    <dialog
      ref={dialog}
      onClose={onClose}
      aria-labelledby="log-detail-title"
      className="border-accent/30 bg-surface/95 text-textPrimary fixed inset-0 m-auto max-h-[80vh] w-[min(60rem,90vw)] rounded-2xl border p-5 shadow-xl backdrop:bg-black/60"
    >
      <header className="mb-4 flex items-center justify-between gap-4">
        <h3 id="log-detail-title" className="text-accent text-lg font-semibold">
          {getLogLevel(log.level).label} · {formatIsoTimestamp(log.time) ?? 'Invalid time'}
        </h3>
        <button type="button" onClick={onClose} className="text-accent rounded px-3 py-2">
          Close
        </button>
      </header>
      <pre className="text-sm break-words whitespace-pre-wrap">{log.message}</pre>
    </dialog>
  );
}
