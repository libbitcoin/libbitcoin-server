import { useCallback, useId, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { ChevronDownIcon } from '@heroicons/react/24/outline';
import { formatInteger } from '@/lib/utils/format';
import {
  chunkRate,
  describePlacement,
  featuredTransaction,
  formatBitcoin,
  formatTemplateRate,
  RATE_CEILING,
  rateGradient,
  rateMix,
  ratePosition,
  shortHash,
  transactionRate,
  type PoolTemplates,
} from '@/lib/utils/pool-template';
import { toPoolTemplates, type TemplateElement } from '@/lib/utils/pool-templates';
import type { Camera, HeightMode } from '@/lib/utils/template-scene';
import {
  loadHeightMode,
  loadSectionExpanded,
  loadTemplateDisplay,
  saveHeightMode,
  saveSectionExpanded,
  saveTemplateDisplay,
  type TemplateDisplay,
} from '@/lib/utils/template-settings';
import { TemplateSceneView } from './template-scene-view';
import { TemplateTreemap } from './template-treemap';

const displays: Array<{ id: TemplateDisplay; label: string }> = [
  { id: '2d', label: '2D' },
  { id: '3d', label: '3D' },
];

const heights: Array<{ id: HeightMode; label: string; legend: string }> = [
  { id: 'chunk', label: 'Cluster rate', legend: 'cluster fee rate (log)' },
  { id: 'own', label: 'Own rate', legend: 'own fee rate (log)' },
  { id: 'fee', label: 'Fee', legend: 'absolute fee (log)' },
];

const ticks = [1, 2, 5, 10, 20, 50, 100, RATE_CEILING];

// The 3D camera, kept across the blocks and templates of the session.
const view: { current: Camera | null } = { current: null };

// The txs of a block or template, the coinbase (if any) left out of the view.
// A tx is identified by its block position, linked when a path is given.
export function BlockViewer({
  elements,
  height,
  coinbase,
  positionPath,
  forceExpanded = false,
}: {
  elements: readonly TemplateElement[];
  height: number;
  coinbase: boolean;
  positionPath?: (position: number) => string;
  forceExpanded?: boolean;
}) {
  const [expanded, setExpanded] = useState(() => forceExpanded || loadSectionExpanded('details'));
  const regionId = useId();
  const headingId = useId();
  const pool = useMemo(
    () => toPoolTemplates([coinbase ? elements.slice(1) : elements], height),
    [coinbase, elements, height]
  );
  const [display, setDisplay] = useState(loadTemplateDisplay);
  const [mode, setMode] = useState<HeightMode>(loadHeightMode);
  const [selection, setSelection] = useState<number | null>(null);
  const valid = selection !== null && selection < pool.transactions.length;
  const selected = valid ? selection : featuredTransaction(pool, 0);
  const current = pool.templates.at(0);

  const select = useCallback((transaction: number) => {
    setSelection(transaction);
  }, []);

  function updateDisplay(next: TemplateDisplay) {
    saveTemplateDisplay(next);
    setDisplay(next);
  }

  function updateMode(next: HeightMode) {
    saveHeightMode(next);
    setMode(next);
  }

  return (
    <section
      id="details"
      className="border-accent/30 bg-surface/80 scroll-mt-24 rounded-xl border"
      aria-labelledby={headingId}
    >
      <h2 id={headingId}>
        <button
          type="button"
          aria-expanded={expanded}
          aria-controls={regionId}
          onClick={() => {
            saveSectionExpanded('details', !expanded);
            setExpanded(!expanded);
          }}
          className="text-accent flex w-full items-center justify-between gap-4 rounded-xl px-6 py-4 text-left font-semibold"
        >
          Details
          <ChevronDownIcon
            aria-hidden="true"
            className={`h-5 w-5 shrink-0 transition-transform ${expanded ? 'rotate-180' : ''}`}
          />
        </button>
      </h2>
      <div id={regionId} hidden={!expanded}>
        {expanded && (
          <div className="border-accent/20 space-y-4 border-t p-6">
      <header className="flex flex-wrap items-end justify-end gap-4">
        <div className="flex flex-wrap gap-2 text-sm">
          {display === '3d' && (
            <Toggle label="Box height" options={heights} value={mode} onChange={updateMode} />
          )}
          <Toggle label="Display" options={displays} value={display} onChange={updateDisplay} />
        </div>
      </header>

      <div className="flex flex-wrap items-start gap-6">
        <div className="min-w-0 flex-[999_1_560px] space-y-3">
          {current && (
            <p className="text-textMuted text-xs tabular-nums">
              {formatInteger(current.count)} txs · {formatInteger(current.size)} of{' '}
              {formatInteger(pool.capacity)} vB · {formatBitcoin(current.fee)}
            </p>
          )}
          {display === '3d' ? (
            <TemplateSceneView
              pool={pool}
              template={0}
              selected={selected}
              mode={mode}
              onSelect={select}
              view={view}
            />
          ) : (
            <TemplateTreemap pool={pool} template={0} selected={selected} onSelect={select} />
          )}
          <Legend
            height={display === '3d' ? heights.find((entry) => entry.id === mode)?.legend : undefined}
          />
        </div>
        <TransactionPanel
          pool={pool}
          selected={selected}
          onSelect={select}
          positionPath={positionPath}
        />
      </div>
          </div>
        )}
      </div>
    </section>
  );
}

function Toggle<Value extends string>({
  label,
  options,
  value,
  onChange,
}: {
  label: string;
  options: ReadonlyArray<{ id: Value; label: string }>;
  value: Value;
  onChange: (value: Value) => void;
}) {
  return (
    <div role="group" aria-label={label} className="flex">
      {options.map((option, index) => (
        <button
          key={option.id}
          type="button"
          aria-pressed={option.id === value}
          onClick={() => {
            onChange(option.id);
          }}
          className={`border px-3 py-2 font-medium ${index > 0 ? '-ml-px' : 'rounded-l-md'} ${
            index === options.length - 1 ? 'rounded-r-md' : ''
          } ${
            option.id === value
              ? 'border-accent/40 bg-accentSecondary/20 text-accent'
              : 'border-accent/30 text-textMuted hover:text-accent hover:bg-accentSecondary/10'
          }`}
        >
          {option.label}
        </button>
      ))}
    </div>
  );
}

function Legend({ height }: { height?: string }) {
  return (
    <div className="flex max-w-sm flex-col gap-1">
      <div className="h-2.5 rounded" style={{ background: rateGradient() }} />
      <div className="text-textMuted relative h-4 text-[11px] tabular-nums">
        {ticks.map((tick) => (
          <span
            key={tick}
            className="absolute -translate-x-1/2"
            style={{ left: `${(ratePosition(tick) * 100).toFixed(1)}%` }}
          >
            {tick}
          </span>
        ))}
      </div>
      <span className="text-textMuted text-[11px]">
        Color: own fee rate, sat/vB (log){height ? ` · Height: ${height}` : ''}
      </span>
    </div>
  );
}

function TransactionPanel({
  pool,
  selected,
  onSelect,
  positionPath,
}: {
  pool: PoolTemplates;
  selected: number | null;
  onSelect: (transaction: number) => void;
  positionPath?: (position: number) => string;
}) {
  const transaction = selected === null ? undefined : pool.transactions[selected];
  if (!transaction) return null;
  const chunk = pool.chunks[transaction.chunk];
  const own = transactionRate(transaction);
  const rows: Array<[string, string]> = [
    ['Fee rate', `${formatTemplateRate(own)} sat/vB`],
    ['Cluster rate', `${formatTemplateRate(chunkRate(chunk))} sat/vB`],
    ['Fee', `${formatInteger(transaction.fee)} sat`],
    ['Virtual size', `${formatInteger(transaction.size)} vB`],
    ['Cluster', `${String(chunk.transactions.length)} tx · ${formatInteger(chunk.size)} vB`],
  ];

  return (
    <aside className="border-accent/30 bg-surface/80 min-w-0 flex-[1_1_300px] space-y-4 rounded-2xl border p-5">
      <div className="space-y-1">
        <p className="text-accentSecondary text-xs tracking-wide uppercase">Transaction</p>
        {positionPath ? (
          <Link
            to={positionPath(transaction.position)}
            className="text-accent hover:text-accentSecondary font-mono text-sm"
          >
            Position {formatInteger(transaction.position)}
          </Link>
        ) : (
          <p className="text-accent font-mono text-sm">
            Position {formatInteger(transaction.position)}
          </p>
        )}
      </div>
      <dl className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1.5 text-sm tabular-nums">
        {rows.map(([label, value]) => (
          <div key={label} className="contents">
            <dt className="text-textMuted">{label}</dt>
            <dd className="text-textPrimary text-right">{value}</dd>
          </div>
        ))}
      </dl>
      <p className="border-accentSecondary/20 text-textMuted border-t pt-3 text-sm">
        {describePlacement(chunk, own)}
      </p>
      <div className="space-y-1">
        <p className="text-accentSecondary text-xs tracking-wide uppercase">
          Cluster members, in mining order
        </p>
        <ul className="space-y-0.5">
          {chunk.transactions.map((member) => {
            const entry = pool.transactions[member];
            const rate = transactionRate(entry);
            return (
              <li key={member}>
                <button
                  type="button"
                  aria-current={member === selected}
                  onClick={() => {
                    onSelect(member);
                  }}
                  className={`hover:bg-accentSecondary/10 flex w-full items-center justify-between gap-3 rounded-md px-2 py-2 text-left text-xs ${
                    member === selected ? 'bg-accentSecondary/20' : ''
                  }`}
                >
                  <span className="flex min-w-0 items-center gap-2">
                    <span
                      aria-hidden="true"
                      className="size-2.5 shrink-0 rounded-sm"
                      style={{ background: rateMix(rate) }}
                    />
                    <span className="text-textPrimary truncate font-mono">{shortHash(entry.hash)}</span>
                  </span>
                  <span className="text-textMuted shrink-0 whitespace-nowrap tabular-nums">
                    {formatTemplateRate(rate)} sat/vB · {formatInteger(entry.size)} vB
                  </span>
                </button>
              </li>
            );
          })}
        </ul>
      </div>
    </aside>
  );
}
