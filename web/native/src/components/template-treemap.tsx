import { memo, useMemo } from 'react';
import {
  formatTemplateRate,
  rateMix,
  templateTiles,
  transactionRate,
  type PoolTemplates,
  type TemplateTile,
} from '@/lib/utils/pool-template';
import { formatInteger } from '@/lib/utils/format';

interface TemplateTreemapProps {
  pool: PoolTemplates;
  template: number;
  selected: number | null;
  onSelect: (transaction: number) => void;
}

// The plot is 16:10; tiles are laid out in that aspect and placed in percent.
const WIDTH = 1.6;
const HEIGHT = 1;

const percent = (value: number, extent: number) => `${((value / extent) * 100).toFixed(3)}%`;

function placement(tile: TemplateTile) {
  return {
    left: percent(tile.rect.x, WIDTH),
    top: percent(tile.rect.y, HEIGHT),
    width: percent(tile.rect.width, WIDTH),
    height: percent(tile.rect.height, HEIGHT),
  };
}

export function TemplateTreemap({ pool, template, selected, onSelect }: TemplateTreemapProps) {
  const tiles = useMemo(() => templateTiles(pool, template, WIDTH, HEIGHT), [pool, template]);
  const marks = useMemo(() => {
    const entry = selected === null ? undefined : pool.transactions[selected];
    if (entry?.template !== template) return [];
    const members = new Set(pool.chunks[entry.chunk].transactions);
    return tiles.filter((tile) => members.has(tile.transaction));
  }, [pool, template, selected, tiles]);

  return (
    <div className="bg-inset relative aspect-[16/10] w-full overflow-hidden rounded-xl">
      <Tiles pool={pool} tiles={tiles} onSelect={onSelect} />
      {marks.map((tile) => (
        <div
          key={tile.transaction}
          aria-hidden="true"
          className={`pointer-events-none absolute z-10 ${
            tile.transaction === selected
              ? 'ring-accent ring-offset-surface ring-2 ring-offset-1'
              : 'ring-textPrimary ring-1 ring-inset'
          }`}
          style={placement(tile)}
        />
      ))}
    </div>
  );
}

// Tiles depend only on the template, so a selection change does not
// re-render them. They are left out of the tab order: a template holds
// thousands, and the chunk list and template cards reach the same details.
const Tiles = memo(function Tiles({
  pool,
  tiles,
  onSelect,
}: {
  pool: PoolTemplates;
  tiles: TemplateTile[];
  onSelect: (transaction: number) => void;
}) {
  return tiles.map((tile) => {
    const transaction = pool.transactions[tile.transaction];
    const rate = transactionRate(transaction);
    return (
      <button
        key={tile.transaction}
        type="button"
        tabIndex={-1}
        aria-label={`${formatTemplateRate(rate)} sat/vB, ${formatInteger(transaction.size)} vB`}
        title={`${formatTemplateRate(rate)} sat/vB · ${formatInteger(transaction.size)} vB`}
        onClick={() => {
          onSelect(tile.transaction);
        }}
        className="absolute cursor-pointer border-0 p-0 shadow-[inset_0_0_0_0.5px_var(--theme-surface)] hover:brightness-125"
        style={{ ...placement(tile), background: rateMix(rate) }}
      />
    );
  });
});
