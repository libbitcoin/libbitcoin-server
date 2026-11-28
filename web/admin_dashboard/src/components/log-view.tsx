import { memo, useEffect, useRef, useState, useSyncExternalStore } from 'react';
import {
  ALL_LOG_FILTER,
  LOG_LEVELS,
  getLogLevel,
  includesLogLevel,
} from '@/lib/admin-contract';
import { formatIsoTimestamp, formatTimestamp } from '@/lib/format';
import type { RecentLog } from '@/lib/live-metrics';
import type { WorkerMetricsStore } from '@/lib/worker-metrics-store';
import { HEADER_ACTION } from '@/lib/styles';

const ROW_HEIGHT = 32;

interface LogViewProps {
  metrics: Pick<WorkerMetricsStore, 'getLogWindow' | 'subscribeLogs' | 'setLogQuery'>;
  logFilter: number;
  paused: boolean;
  onLogFilterChange: (filter: number) => void;
  onPausedChange: (paused: boolean) => void;
  onClear: () => void;
  faults: number;
  clearedFaultSequence: number;
  onClearFaults: () => void;
}

// The fault log level.
const FAULT_LEVEL = 6;

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
  logFilter,
  paused,
  onLogFilterChange,
  onPausedChange,
  onClear,
  faults,
  clearedFaultSequence,
  onClearFaults,
}: LogViewProps) {
  const [search, setSearch] = useState('');
  const [scroll, setScroll] = useState({ offset: 0, search, filter: logFilter });
  const offset = scroll.search === search && scroll.filter === logFilter ? scroll.offset : 0;
  const viewport = useRef<HTMLDivElement>(null);
  const window = useSyncExternalStore(
    metrics.subscribeLogs,
    metrics.getLogWindow,
    metrics.getLogWindow
  );

  useEffect(() => {
    // eslint-disable-next-line react-you-might-not-need-an-effect/no-pass-live-state-to-parent -- Synchronize a worker query, not React parent state.
    metrics.setLogQuery({ search, filter: logFilter, offset });
  }, [metrics, search, logFilter, offset]);

  useEffect(() => {
    if (viewport.current) viewport.current.scrollTop = 0;
  }, [search, logFilter]);

  return (
    <section className="space-y-8">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <h1 className="text-accent text-2xl font-semibold">Logs</h1>
          <p className="text-textMuted text-sm">The node&apos;s log messages at the levels selected.</p>
        </div>
        <div className="flex gap-2">
          {faults > 0 && (
            <button type="button" onClick={onClearFaults} className={HEADER_ACTION}>
              Clear faults
            </button>
          )}
          <button
            type="button"
            onClick={() => {
              if (paused) {
                if (viewport.current) viewport.current.scrollTop = 0;
                setScroll({ offset: 0, search, filter: logFilter });
              }
              onPausedChange(!paused);
            }}
            className={HEADER_ACTION}
          >
            {paused ? 'Resume' : 'Pause'}
          </button>
          <button
            type="button"
            onClick={onClear}
            className={HEADER_ACTION}
          >
            Clear
          </button>
        </div>
      </header>

      <fieldset>
        <legend className="sr-only">Log levels</legend>
        <div className="flex flex-wrap gap-x-5 gap-y-3">
          {LOG_LEVELS.map((item) => (
            <label
              key={item.id}
              className={`text-textPrimary flex items-center gap-2 text-sm ${item.disabled ? 'opacity-50' : ''}`}
            >
              <input
                type="checkbox"
                checked={includesLogLevel(logFilter, item.id)}
                disabled={item.disabled}
                onChange={() => {
                  onLogFilterChange((logFilter ^ (1 << item.id)) & ALL_LOG_FILTER);
                }}
                className="border-accent/35 bg-background text-accent focus:ring-accent rounded disabled:cursor-not-allowed"
              />
              {item.label}
            </label>
          ))}
        </div>
      </fieldset>

      <div className="flex flex-wrap gap-3">
        <label className="min-w-56 flex-1">
          <span className="sr-only">Search logs</span>
          <input
            type="search"
            value={search}
            onChange={(event) => {
              setSearch(event.target.value);
            }}
            placeholder="Search messages…"
            className="bg-surface/80 text-textPrimary placeholder:text-textMuted/60 w-full rounded-md border-0 px-3 py-2 text-sm focus:ring-0"
          />
        </label>
      </div>

      <div className="border-accent/30 bg-surface/80 overflow-hidden rounded-xl border">
        <div className="border-accentSecondary/30 bg-accentSecondary/20 text-textMuted flex justify-between border-b px-4 py-3 text-xs">
          <span>
            {paused
              ? 'Paused'
              : logFilter === 0
                ? 'No levels selected'
                : offset > 0
                  ? 'Older messages'
                  : 'Newest first'}
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
                  uncleared={log.level === FAULT_LEVEL && log.id > clearedFaultSequence}
                />
              ))}
            </ol>
          )}
        </div>
      </div>
      <div className="flex items-center justify-end gap-3 text-xs">
        {!paused && (
          <button
            type="button"
            className="text-accent hover:text-accentSecondary"
            onClick={() => {
              if (viewport.current) viewport.current.scrollTop = 0;
              setScroll({ offset: 0, search, filter: logFilter });
            }}
          >
            Jump to latest
          </button>
        )}
      </div>
    </section>
  );
}

const LogRow = memo(function LogRow({
  log,
  index,
  total,
  uncleared,
}: {
  log: RecentLog;
  index: number;
  total: number;
  uncleared: boolean;
}) {
  return (
    <li
      aria-setsize={total}
      aria-posinset={index + 1}
      className={`border-accentSecondary/20 absolute inset-x-0 border-b ${uncleared ? 'border-l-2 border-l-red-400 bg-red-500/10' : ''}`}
      style={{ top: index * ROW_HEIGHT, height: ROW_HEIGHT }}
    >
      <div className="grid h-full w-full grid-cols-[5.5rem_4.5rem_minmax(0,1fr)] items-center gap-2 px-4 text-left text-xs sm:grid-cols-[7.5rem_7.5rem_minmax(0,1fr)]">
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
          {log.message.trimEnd().replaceAll('\n', ' ↵ ')}
        </span>
      </div>
    </li>
  );
});

