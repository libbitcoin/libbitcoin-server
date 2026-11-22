import { useCallback, useMemo, useState } from 'react';
import { formatInteger } from '@/lib/format';
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
  templateName,
  templateRates,
  transactionRate,
  type PoolTemplates,
} from '@/lib/pool-template';
import { stubPoolTemplates } from '@/lib/pool-template-stub';
import type { HeightMode } from '@/lib/template-scene';
import {
  loadTemplateDisplay,
  saveTemplateDisplay,
  type TemplateDisplay,
} from '@/lib/template-settings';
import { TemplateSceneView } from './template-scene-view';
import { TemplateTreemap } from './template-treemap';

const displays: Array<{ id: TemplateDisplay; label: string }> = [
  { id: '2d', label: '2D' },
  { id: '3d', label: '3D' },
];

const heights: Array<{ id: HeightMode; label: string; legend: string }> = [
  { id: 'chunk', label: 'Chunk rate', legend: 'chunk fee rate (log)' },
  { id: 'own', label: 'Own rate', legend: 'own fee rate (log)' },
  { id: 'fee', label: 'Fee', legend: 'absolute fee (log)' },
];

const ticks = [1, 2, 5, 10, 20, 50, 100, RATE_CEILING];

export function TemplateView() {
  const pool = useMemo(() => stubPoolTemplates(), []);
  const [display, setDisplay] = useState(loadTemplateDisplay);
  const [mode, setMode] = useState<HeightMode>('chunk');
  const [template, setTemplate] = useState(0);
  const [selection, setSelection] = useState<number | null>(null);
  const selected = selection ?? featuredTransaction(pool, template);
  const current = pool.templates.at(template);

  const select = useCallback(
    (transaction: number) => {
      setSelection(transaction);
      setTemplate(pool.transactions[transaction].template);
    },
    [pool]
  );

  function updateDisplay(next: TemplateDisplay) {
    saveTemplateDisplay(next);
    setDisplay(next);
  }

  return (
    <section className="space-y-4">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-xs tracking-wide uppercase">Transaction pool</p>
          <h2 className="text-accent mt-1 text-2xl font-semibold">Block templates</h2>
          <p className="text-textMuted mt-1 max-w-3xl text-sm">
            The next blocks as the pool would fill them. Area is virtual size, color is fee rate.
            Transactions of one chunk are mined together, at the chunk&apos;s combined rate.
          </p>
        </div>
        <div className="flex flex-wrap gap-2 text-sm">
          {display === '3d' && (
            <Toggle
              label="Box height"
              options={heights}
              value={mode}
              onChange={setMode}
            />
          )}
          <Toggle label="Display" options={displays} value={display} onChange={updateDisplay} />
        </div>
      </header>

      <p className="rounded-xl border border-amber-500/30 bg-amber-500/10 px-4 py-3 text-sm text-amber-300">
        Placeholder data: this pool is generated in the browser until the server publishes its
        templates.
      </p>

      <div className="grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
        {pool.templates.map((entry, index) => {
          const rates = templateRates(pool, entry);
          const active = index === template;
          return (
            <button
              key={index}
              type="button"
              aria-pressed={active}
              onClick={() => {
                setTemplate(index);
                setSelection(null);
              }}
              className={`bg-surface/80 min-w-0 space-y-1 rounded-2xl border p-4 text-left ${
                active ? 'border-accent' : 'border-accent/30 hover:border-accent/60'
              }`}
            >
              <span className="text-accentSecondary flex justify-between gap-2 text-xs tracking-wide uppercase">
                <span>{templateName(index)}</span>
                <span className="tabular-nums">{formatInteger(entry.count)} txs</span>
              </span>
              <span className="text-textPrimary block text-2xl font-semibold tabular-nums">
                ~{formatTemplateRate(rates.median)}{' '}
                <span className="text-textMuted text-sm font-normal">sat/vB</span>
              </span>
              <span className="text-textMuted block text-xs tabular-nums">
                {formatTemplateRate(rates.minimum)} – {formatTemplateRate(rates.maximum)} sat/vB ·{' '}
                {formatBitcoin(entry.fee)}
              </span>
              <span className="bg-inset mt-2 block h-1 overflow-hidden rounded">
                <span
                  className={`block h-1 ${active ? 'bg-accent' : 'bg-accentSecondary'}`}
                  style={{ width: `${Math.min(100, (entry.size / pool.capacity) * 100).toFixed(1)}%` }}
                />
              </span>
            </button>
          );
        })}
      </div>

      <div className="flex flex-wrap items-start gap-6">
        <div className="min-w-0 flex-[999_1_560px] space-y-3">
          {current && (
            <div className="text-textMuted flex flex-wrap items-baseline justify-between gap-2 text-xs tabular-nums">
              <h3 className="text-accent text-lg font-semibold">{templateName(template)} template</h3>
              <span>
                {formatInteger(current.count)} txs · {formatInteger(current.size)} of{' '}
                {formatInteger(pool.capacity)} vB · {formatBitcoin(current.fee)}
              </span>
            </div>
          )}
          {display === '3d' ? (
            <TemplateSceneView
              pool={pool}
              template={template}
              selected={selected}
              mode={mode}
              onSelect={select}
            />
          ) : (
            <TemplateTreemap pool={pool} template={template} selected={selected} onSelect={select} />
          )}
          <Legend
            height={display === '3d' ? heights.find((entry) => entry.id === mode)?.legend : undefined}
          />
        </div>
        <TransactionPanel pool={pool} selected={selected} onSelect={select} />
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
      <div
        className="h-2.5 rounded"
        style={{ background: rateGradient() }}
      />
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
}: {
  pool: PoolTemplates;
  selected: number | null;
  onSelect: (transaction: number) => void;
}) {
  const transaction = selected === null ? undefined : pool.transactions[selected];
  if (!transaction) return null;
  const chunk = pool.chunks[transaction.chunk];
  const cluster = pool.clusters[transaction.cluster];
  const own = transactionRate(transaction);
  const rows: Array<[string, string]> = [
    ['Fee rate', `${formatTemplateRate(own)} sat/vB`],
    ['Chunk rate', `${formatTemplateRate(chunkRate(chunk))} sat/vB`],
    ['Fee', `${formatInteger(transaction.fee)} sat`],
    ['Virtual size', `${formatInteger(transaction.size)} vB`],
    ['Chunk', `${String(chunk.transactions.length)} tx · ${formatInteger(chunk.size)} vB`],
    ['Cluster', `${String(cluster.transactions.length)} tx · ${String(cluster.chunks.length)} chunks`],
    ['Parents / children', `${String(transaction.parents.length)} / ${String(transaction.children.length)}`],
    [
      'Position',
      `${formatInteger(transaction.position)} of ${formatInteger(pool.templates[transaction.template].count)}`,
    ],
  ];

  return (
    <aside className="border-accent/30 bg-surface/80 min-w-0 flex-[1_1_300px] space-y-4 rounded-2xl border p-5">
      <div className="space-y-1">
        <p className="text-accentSecondary text-xs tracking-wide uppercase">
          Transaction · {templateName(transaction.template)}
        </p>
        <p className="text-accent font-mono text-sm" title={transaction.hash}>
          {shortHash(transaction.hash)}
        </p>
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
          Chunk members, in mining order
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
