import { memo, useEffect, useRef, useState, useSyncExternalStore } from 'react';
import { EVENT_DEFINITIONS, type EventCategory, getEventDefinition } from '@/lib/admin-contract';
import { formatEventValue, formatIsoTimestamp, formatTimestamp } from '@/lib/format';
import type { RecentEvent } from '@/lib/live-metrics';
import type { WorkerMetricsStore } from '@/lib/worker-metrics-store';

const ROW_HEIGHT = 32;

interface EventViewProps {
  metrics: Pick<WorkerMetricsStore, 'getEventWindow' | 'subscribeEvents' | 'setEventQuery'>;
  paused: boolean;
  onPausedChange: (paused: boolean) => void;
  onClear: () => void;
}

const categories: Array<{ value: EventCategory | 'all'; label: string }> = [
  { value: 'all', label: 'All categories' },
  { value: 'headers', label: 'Headers' },
  { value: 'blocks', label: 'Blocks' },
  { value: 'transactions', label: 'Transactions' },
  { value: 'chain', label: 'Confirmed chain' },
  { value: 'mining', label: 'Mining' },
  { value: 'timing', label: 'Timing' },
];

const categoryStyles: Record<EventCategory, string> = {
  headers: 'bg-violet-500/15 text-violet-300',
  blocks: 'bg-accent/15 text-accent',
  transactions: 'bg-sky-500/15 text-sky-300',
  chain: 'bg-emerald-500/15 text-emerald-300',
  mining: 'bg-amber-500/15 text-amber-300',
  timing: 'bg-cyan-500/15 text-cyan-300',
};

export function EventView({ metrics, paused, onPausedChange, onClear }: EventViewProps) {
  const [search, setSearch] = useState('');
  const [category, setCategory] = useState<EventCategory | 'all'>('all');
  const [scroll, setScroll] = useState({ offset: 0, search, category });
  const offset = scroll.search === search && scroll.category === category ? scroll.offset : 0;
  const viewport = useRef<HTMLDivElement>(null);
  const window = useSyncExternalStore(
    metrics.subscribeEvents,
    metrics.getEventWindow,
    metrics.getEventWindow
  );

  useEffect(() => {
    // eslint-disable-next-line react-you-might-not-need-an-effect/no-pass-live-state-to-parent, react-you-might-not-need-an-effect/no-pass-data-to-parent -- Synchronize a worker query, not React parent state.
    metrics.setEventQuery({ search, category, offset });
  }, [metrics, search, category, offset]);

  useEffect(() => {
    if (viewport.current) viewport.current.scrollTop = 0;
  }, [search, category]);

  return (
    <section className="space-y-4">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-xs tracking-wide uppercase">Node lifecycle</p>
          <h2 className="text-accent mt-1 text-2xl font-semibold">Live events</h2>
          <p className="text-textMuted mt-1 text-sm">
            Heights are event payloads; timing values retain their source units.
          </p>
          <p className="text-textMuted mt-1 text-xs">
            Pause freezes this view; collection and charts continue.
          </p>
        </div>
        <div className="flex gap-2">
          <button
            type="button"
            onClick={() => {
              if (paused) {
                if (viewport.current) viewport.current.scrollTop = 0;
                setScroll({ offset: 0, search, category });
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

      <div className="border-accent/30 bg-surface/80 flex flex-wrap gap-3 rounded-2xl border p-4">
        <label className="min-w-56 flex-1">
          <span className="sr-only">Search events</span>
          <input
            type="search"
            value={search}
            onChange={(event) => {
              setSearch(event.target.value);
            }}
            placeholder="Search event names or values…"
            className="border-accent/40 bg-background text-textPrimary placeholder:text-textMuted/60 focus:border-accent w-full rounded-md border px-3 py-2 text-sm focus:ring-0"
          />
        </label>
        <label>
          <span className="sr-only">Filter event category</span>
          <select
            value={category}
            onChange={(event) => {
              setCategory(event.target.value as EventCategory | 'all');
            }}
            className="border-accent/40 bg-background text-textPrimary focus:border-accent rounded-md border px-3 py-2 text-sm focus:ring-0"
          >
            {categories.map((item) => (
              <option key={item.value} value={item.value}>
                {item.label}
              </option>
            ))}
          </select>
        </label>
      </div>

      <div className="border-accent/30 bg-surface/80 overflow-hidden rounded-xl border">
        <div className="border-accentSecondary/30 bg-accentSecondary/20 text-textMuted flex justify-between border-b px-4 py-3 text-xs">
          <span>
            {paused
              ? 'Display paused · collection continues'
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
          aria-label="Scrollable event history"
          tabIndex={0}
          className="h-[min(60vh,32rem)] min-h-32 overflow-auto"
          style={{ overflowAnchor: 'none' }}
          onScroll={(event) => {
            setScroll({
              offset: Math.max(0, Math.floor(event.currentTarget.scrollTop / ROW_HEIGHT) - 8),
              search,
              category,
            });
          }}
        >
          {window.matched === 0 ? (
            <p className="text-textMuted px-4 py-12 text-center text-sm">No matching events.</p>
          ) : (
            <ol
              aria-label="Received events"
              className="relative min-w-[32rem]"
              style={{ height: window.matched * ROW_HEIGHT }}
            >
              {window.events.map((event, index) => (
                <EventRow
                  key={event.id}
                  event={event}
                  index={window.offset + index}
                  total={window.matched}
                />
              ))}
            </ol>
          )}
        </div>
      </div>

      {!paused && (
        <div className="flex justify-end text-xs">
          <button
            type="button"
            className="text-accent hover:underline"
            onClick={() => {
              if (viewport.current) viewport.current.scrollTop = 0;
              setScroll({ offset: 0, search, category });
            }}
          >
            Jump to latest
          </button>
        </div>
      )}

      <details className="border-accent/30 bg-surface/80 rounded-2xl border">
        <summary className="text-textMuted hover:text-accent cursor-pointer px-4 py-3 text-sm">
          Event contract ({String(EVENT_DEFINITIONS.length)} identifiers)
        </summary>
        <div className="border-accentSecondary/20 grid gap-2 border-t p-4 sm:grid-cols-2 lg:grid-cols-3">
          {EVENT_DEFINITIONS.map((definition) => (
            <div key={definition.id} className="text-xs">
              <span className="text-textMuted font-mono">
                {String(definition.id).padStart(2, '0')}
              </span>
              <span className="text-textPrimary ml-2">{definition.name}</span>
            </div>
          ))}
        </div>
      </details>
    </section>
  );
}

const EventRow = memo(function EventRow({
  event,
  index,
  total,
}: {
  event: RecentEvent;
  index: number;
  total: number;
}) {
  const definition = getEventDefinition(event.event);
  return (
    <li
      aria-setsize={total}
      aria-posinset={index + 1}
      className="border-accentSecondary/20 absolute inset-x-0 grid grid-cols-[8.5rem_10rem_minmax(0,1fr)] items-center gap-2 border-b px-4 text-xs"
      style={{ top: index * ROW_HEIGHT, height: ROW_HEIGHT }}
    >
      <time
        dateTime={formatIsoTimestamp(event.time)}
        className="text-textMuted font-mono tabular-nums"
      >
        {formatTimestamp(event.time)}
      </time>
      <span
        className={`w-fit rounded px-2 py-0.5 font-medium ${categoryStyles[definition.category]}`}
      >
        {definition.name}
      </span>
      <span className="text-textPrimary font-mono tabular-nums">
        {formatEventValue(event.event, event.value)}
      </span>
    </li>
  );
});
