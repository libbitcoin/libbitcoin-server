import { useCallback, useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { BlockMinerValue } from '@/components/miner-value';
import { Spinner } from '@/components/spinner';
import { Toggle } from '@/components/toggle';
import {
  useBlockchainClient,
  type BlockSummary,
  type BranchHeader,
  type ChainBranch,
} from '@/lib/api/client';
import { useBranches, useLatestBlocks } from '@/lib/hooks/useBlockchainData';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { useWindowScrollToBottom } from '@/lib/hooks/useWindowScrollToBottom';
import { formatBytes, formatInteger, formatShortRelativeTime } from '@/lib/utils/format';
import { loadUnconfirmedOnly, saveUnconfirmedOnly } from '@/lib/utils/template-settings';
import { blockByHashPath, blockByHeightPath, templatePath } from '@/lib/utils/routes';

const PAGE_SIZE = 30;
const ROW = 44;
const LANE = 26;
const NODE = 5;

// A header of a weak branch at a row, in the lane of its branch.
interface BranchNode {
  lane: number;
  header: BranchHeader;
  top: boolean;
  base: boolean;
}

interface Row {
  height: number;
  strong?: BlockSummary;
  pending?: BranchHeader;
  nodes: BranchNode[];
}

const laneX = (lane: number) => LANE / 2 + lane * LANE;

const filters: Array<{ id: 'all' | 'unconfirmed'; label: string }> = [
  { id: 'all', label: 'All' },
  { id: 'unconfirmed', label: 'Unconfirmed' },
];

// The chain as a tree, the top first and genesis last: the confirmed chain in
// the first lane, the candidate headers above it, each weak branch in a lane
// of its own from the block it builds on.
export function BlocksPage() {
  usePageTitle('Blocks');
  const client = useBlockchainClient();
  const now = useNow();
  const { data: latest, isLoading, isError } = useLatestBlocks(PAGE_SIZE);
  const { data: branches } = useBranches();
  const [older, setOlder] = useState<BlockSummary[]>([]);
  const [loading, setLoading] = useState(false);
  const [unconfirmed, setUnconfirmed] = useState(loadUnconfirmedOnly);
  const updateFilter = useCallback((next: 'all' | 'unconfirmed') => {
    setUnconfirmed(next === 'unconfirmed');
    saveUnconfirmedOnly(next === 'unconfirmed');
  }, []);

  useEffect(() => {
    setOlder([]);
  }, [client]);

  const strong = useMemo(() => {
    const recent = (latest ?? []).filter((block) => !block.weak);
    const lowest = recent.at(-1)?.height;
    return lowest === undefined ? [] : [...recent, ...older.filter((block) => block.height < lowest)];
  }, [latest, older]);

  const lowest = strong.at(-1)?.height;
  const done = lowest === 0;
  const loadMore = useCallback(() => {
    if (lowest === undefined || lowest === 0) return;
    setLoading(true);
    void client
      .getBlocksBefore(lowest - 1, PAGE_SIZE)
      .then((blocks) => {
        setOlder((current) => [...current, ...blocks]);
      })
      .finally(() => {
        setLoading(false);
      });
  }, [client, lowest]);

  useWindowScrollToBottom({
    enabled: !unconfirmed && !loading && !done && strong.length > 0,
    onBottomReached: loadMore,
  });

  const weak = useMemo(
    () =>
      (branches ?? [])
        .filter((branch) => branch.top.chain === undefined)
        .sort((left, right) => right.top.height - left.top.height),
    [branches]
  );
  const lead = branches?.find((branch) => branch.top.chain === 'candidate');
  const top = strong.at(0)?.height;
  const laned = useMemo(() => toLanes(weak), [weak]);
  const rows = useMemo(
    () =>
      unconfirmed
        ? toWeakRows(weak, laned)
        : lowest === undefined
          ? []
          : toRows(strong, lead, weak, laned, lowest),
    [unconfirmed, strong, lead, weak, laned, lowest]
  );
  const lanes = Math.max(0, ...laned) + 1;

  return (
    <div className="space-y-8">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
            Blocks
          </h1>
          <p className="text-textMuted text-sm">
            {unconfirmed
              ? 'The unconfirmed blocks this node has seen.'
              : 'The confirmed blocks and any unconfirmed blocks this node has seen.'}
          </p>
        </div>
        <div className="text-sm">
          <Toggle
            label="Blocks shown"
            options={filters}
            value={unconfirmed ? 'unconfirmed' : 'all'}
            onChange={updateFilter}
          />
        </div>
      </header>

      {isError && <p className="text-accentSecondary text-sm">Could not load blocks.</p>}
      {isLoading && <Spinner label="Loading blocks…" />}
      {unconfirmed && branches !== undefined && rows.length === 0 && (
        <p className="text-textMuted text-sm">No unconfirmed blocks.</p>
      )}

      {rows.length > 0 && (
        <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
          <ul className="min-w-[44rem]">
            {!unconfirmed && top !== undefined && (
              <li className="border-accentSecondary/20 flex items-center border-b" style={{ height: ROW }}>
                <Lanes lanes={lanes} template={templatePath(top + 1)} />
                <Link
                  to={templatePath(top + 1)}
                  className="text-accent hover:text-accentSecondary px-3 text-sm"
                >
                  Template {formatInteger(top + 1)}
                </Link>
                <span className="text-textMuted truncate text-xs">
                  the next block, as the pool would fill it
                </span>
              </li>
            )}
            {rows.map((row) => (
              <li
                key={row.height}
                className="hover:bg-accentSecondary/10 flex items-center"
                style={{ height: ROW }}
              >
                <Lanes lanes={lanes} row={row} />
                <RowDetail row={row} now={now} />
              </li>
            ))}
          </ul>
        </div>
      )}

      {!unconfirmed && !done && strong.length > 0 && (
        <div className="flex justify-center">
          <button
            type="button"
            onClick={loadMore}
            disabled={loading}
            className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary rounded-md border px-4 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-60"
          >
            {loading ? <Spinner label="Loading more…" /> : 'Load more blocks'}
          </button>
        </div>
      )}
    </div>
  );
}

// Rows of the weak branches alone, from the highest top down, so heights at
// which no branch has a block are absent.
function toWeakRows(weak: ChainBranch[], laned: number[]): Row[] {
  const rows = new Map<number, Row>();
  weak.forEach((branch, index) => {
    const base = branch.headers.at(-1)?.height;
    for (const header of branch.headers) {
      let row = rows.get(header.height);
      if (!row) {
        row = { height: header.height, nodes: [] };
        rows.set(header.height, row);
      }

      row.nodes.push({
        lane: laned[index],
        header,
        top: header.height === branch.top.height,
        base: header.height === base && branch.fork !== undefined,
      });
    }
  });

  return [...rows.values()].sort((left, right) => right.height - left.height);
}

// The lane of each branch, top first: the first lane free below the branch
// last placed in it, so branches of distinct heights share lanes.
function toLanes(weak: ChainBranch[]): number[] {
  const bases: number[] = [];
  return weak.map((branch) => {
    const base = branch.headers.at(-1)?.height ?? branch.top.height;
    let lane = bases.findIndex((occupied) => occupied > branch.top.height + 1);
    if (lane === -1) lane = bases.length;
    bases[lane] = base;
    return lane + 1;
  });
}

// Rows from the highest top down to the lowest confirmed block loaded.
function toRows(
  strong: BlockSummary[],
  lead: ChainBranch | undefined,
  weak: ChainBranch[],
  laned: number[],
  lowest: number
): Row[] {
  const rows = new Map<number, Row>();
  const row = (height: number) => {
    let entry = rows.get(height);
    if (!entry) {
      entry = { height, nodes: [] };
      rows.set(height, entry);
    }
    return entry;
  };

  for (const block of strong) row(block.height).strong = block;
  for (const header of lead?.headers ?? []) row(header.height).pending = header;
  weak.forEach((branch, index) => {
    const base = branch.headers.at(-1)?.height;
    for (const header of branch.headers) {
      if (header.height < lowest) continue;
      row(header.height).nodes.push({
        lane: laned[index],
        header,
        top: header.height === branch.top.height,
        base: header.height === base && branch.fork !== undefined,
      });
    }
  });

  return [...rows.values()].sort((left, right) => right.height - left.height);
}

// The lanes of a row: the strong chain's line and node, and each branch's.
function Lanes({ lanes, row, template }: { lanes: number; row?: Row; template?: string }) {
  const first = laneX(0);
  const middle = ROW / 2;
  const occupied = row?.strong !== undefined || row?.pending !== undefined;
  return (
    <div className="relative shrink-0" style={{ width: lanes * LANE, height: ROW }}>
      {template && <NodeLink lane={0} to={template} label="Template" />}
      {row?.strong && <BlockLink lane={0} hash={row.strong.hash} label="Confirmed block" />}
      {!row?.strong && row?.pending && (
        <BlockLink lane={0} hash={row.pending.hash} label="Candidate block" />
      )}
      {row?.nodes.map((node) => (
        <BlockLink
          key={node.header.hash}
          lane={node.lane}
          hash={node.header.hash}
          label="Unconfirmed block"
        />
      ))}
      <svg width={lanes * LANE} height={ROW} aria-hidden="true" fill="none" strokeWidth={2}>
      {template && (
        <>
          <line x1={first} y1={middle} x2={first} y2={ROW} className="stroke-accent/50" strokeDasharray="3 3" />
          <rect x={first - NODE} y={middle - NODE} width={NODE * 2} height={NODE * 2} className="stroke-accent" strokeDasharray="3 2" />
        </>
      )}
      {occupied && (
        <>
          <line
            x1={first}
            y1={0}
            x2={first}
            y2={row?.height === 0 ? middle : ROW}
            className={row?.strong ? 'stroke-accent' : 'stroke-accent/50'}
            strokeDasharray={row?.strong ? undefined : '3 3'}
          />
          <circle
            cx={first}
            cy={middle}
            r={NODE}
            className={row?.strong ? 'fill-accent stroke-accent' : 'fill-surface stroke-accent'}
            strokeDasharray={row?.strong ? undefined : '3 2'}
          />
        </>
      )}
      {row?.nodes.map((node) => {
        const x = laneX(node.lane);
        return (
          <g key={node.header.hash} className="stroke-accentSecondary">
            {!node.top && <line x1={x} y1={0} x2={x} y2={middle} />}
            {node.base ? (
              <path d={`M${String(x)} ${String(middle)} Q${String(x)} ${String(ROW)} ${String(first)} ${String(ROW)}`} />
            ) : (
              <line x1={x} y1={middle} x2={x} y2={ROW} />
            )}
            <circle cx={x} cy={middle} r={NODE} className="fill-surface" />
          </g>
        );
      })}
      </svg>
    </div>
  );
}

// A node of the tree, as a link.
function NodeLink({ lane, to, label }: { lane: number; to: string; label: string }) {
  const size = NODE * 4;
  return (
    <Link
      to={to}
      title={label}
      aria-label={label}
      className="hover:bg-accentSecondary/30 absolute z-10 rounded-full"
      style={{ left: laneX(lane) - size / 2, top: ROW / 2 - size / 2, width: size, height: size }}
    />
  );
}

function BlockLink({ lane, hash, label }: { lane: number; hash: string; label: string }) {
  return <NodeLink lane={lane} to={blockByHashPath(hash)} label={`${label} ${hash}`} />;
}

function RowDetail({ row, now }: { row: Row; now: number }) {
  const { strong } = row;
  if (!strong) {
    return (
      <span className="text-textMuted px-3 text-sm tabular-nums">{formatInteger(row.height)}</span>
    );
  }

  return (
    <div className="grid min-w-0 flex-1 grid-cols-[5.5rem_minmax(8rem,1fr)_5.5rem_6.5rem_4.5rem_5rem] items-center gap-x-4 px-3 text-sm">
      <Link
        to={blockByHeightPath(row.height)}
        className="text-accent hover:text-accentSecondary font-medium tabular-nums"
      >
        {formatInteger(row.height)}
      </Link>
      <Link to={blockByHashPath(strong.hash)} className="text-accent hover:text-accentSecondary truncate font-mono">
        {strong.hash}
      </Link>
      <span className="text-textMuted truncate">{formatShortRelativeTime(strong.timestamp, now)}</span>
      <span className="text-textPrimary truncate">
        <BlockMinerValue hash={strong.hash} />
      </span>
      <span className="text-textMuted text-right tabular-nums">
        {typeof strong.txCount === 'number' ? `${formatInteger(strong.txCount)} txs` : '—'}
      </span>
      <span className="text-textMuted text-right tabular-nums">
        {formatBytes(strong.size)}
      </span>
    </div>
  );
}
