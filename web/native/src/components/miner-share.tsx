import { useMemo } from 'react';
import { useRecentMiners } from '@/lib/hooks/useBlockchainData';
import { formatNumber } from '@/lib/utils/format';
import type { Miner } from '@/lib/utils/miner';

const SLICE_COLORS = ['var(--chart-series-1)', 'var(--chart-series-2)', 'var(--chart-series-3)'];
const LISTED_MINERS = SLICE_COLORS.length;
const OTHER_COLOR = 'var(--chart-series-other)';

interface Share {
  name: string;
  count: number;
  color: string;
}

function toShares(miners: Array<Miner | null>): Share[] {
  const counts = new Map<string, number>();
  for (const miner of miners) {
    const name = miner?.name ?? 'Unknown';
    counts.set(name, (counts.get(name) ?? 0) + 1);
  }

  const ranked = [...counts.entries()].sort((left, right) => right[1] - left[1]);
  const listed = ranked.length > LISTED_MINERS + 1 ? ranked.slice(0, LISTED_MINERS) : ranked;
  const shares = listed.map(([name, count], index) => ({
    name,
    count,
    color: index < LISTED_MINERS ? SLICE_COLORS[index] : OTHER_COLOR,
  }));

  const other = ranked.slice(listed.length).reduce((sum, [, count]) => sum + count, 0);
  if (other > 0) shares.push({ name: 'Other', count: other, color: OTHER_COLOR });
  return shares;
}

function slicePath(start: number, end: number) {
  const point = (fraction: number) => {
    const angle = 2 * Math.PI * fraction - Math.PI / 2;
    return `${String(50 + 50 * Math.cos(angle))} ${String(50 + 50 * Math.sin(angle))}`;
  };
  const large = end - start > 0.5 ? 1 : 0;
  return `M 50 50 L ${point(start)} A 50 50 0 ${String(large)} 1 ${point(end)} Z`;
}

export function MinerShareCard({ count }: { count: number }) {
  const { data, isError } = useRecentMiners(count);
  const shares = useMemo(() => (data ? toShares(data.miners) : []), [data]);
  const total = data?.miners.length ?? 0;
  const percent = (value: number) =>
    `${formatNumber((100 * value) / total, { maximumFractionDigits: 1 })}%`;

  const slices: Array<Share & { start: number; end: number }> = [];
  for (const share of shares) {
    const start = slices.at(-1)?.end ?? 0;
    slices.push({ ...share, start, end: start + share.count / total });
  }

  return (
    <div className="border-accent/30 bg-surface/80 space-y-2 rounded-2xl border p-5">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">Miners</p>
      {total > 0 ? (
        <div className="flex items-center gap-3">
          <svg viewBox="0 0 100 100" className="h-14 w-14 shrink-0" role="img">
            <title>Blocks by miner</title>
            {slices.map((slice) =>
              slice.end - slice.start >= 1 ? (
                <circle key={slice.name} cx="50" cy="50" r="50" fill={slice.color}>
                  <title>{`${slice.name}: ${percent(slice.count)}`}</title>
                </circle>
              ) : (
                <path
                  key={slice.name}
                  d={slicePath(slice.start, slice.end)}
                  fill={slice.color}
                  stroke="var(--theme-surface)"
                  strokeWidth="3"
                  strokeLinejoin="round"
                >
                  <title>{`${slice.name}: ${percent(slice.count)}`}</title>
                </path>
              )
            )}
          </svg>
          <ul className="min-w-0 flex-1 text-xs leading-tight">
            {shares.map((share) => (
              <li key={share.name} className="flex items-center gap-1.5">
                <span
                  className="h-2 w-2 shrink-0 rounded-sm"
                  style={{ backgroundColor: share.color }}
                />
                <span className="text-textPrimary truncate">{share.name}</span>
                <span className="text-textMuted ml-auto">{percent(share.count)}</span>
              </li>
            ))}
          </ul>
        </div>
      ) : (
        <>
          <div className="text-textPrimary text-2xl font-semibold">{isError ? '—' : '…'}</div>
          <p className="text-textMuted text-sm">—</p>
        </>
      )}
    </div>
  );
}
