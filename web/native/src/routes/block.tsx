import { type ReactNode, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import {
  useBlockHeader,
  useBlockValidation,
  useBranches,
  useBlockStats,
  useBlockTransactionDetails,
  useBlockTransactionHashes,
  useBlockTransactions,
} from '@/lib/hooks/useBlockchainData';
import {
  useBlockchainClient,
  type BlockHeaderDetail,
  type ChainBranch,
} from '@/lib/api/client';
import { useNow } from '@/lib/hooks/useNow';
import { useSwipeNavigation } from '@/lib/hooks/useSwipeNavigation';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import {
  blockByHashPath,
  blockValidationPath,
  sectionId,
  sectionSuffix,
  templatePath,
} from '@/lib/utils/routes';
import {
  formatBytes,
  formatHex,
  formatInteger,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
} from '@/lib/utils/format';
import { HEADER_ACTION } from '@/lib/utils/styles';
import { BlockSizeBar } from '@/components/block-size-bar';
import { Spinner } from '@/components/spinner';
import { CopyButton } from '@/components/copy-button';
import { StatusPill } from '@/components/status-pill';
import { BlockViewer } from '@/components/block-viewer';
import { CoinbaseTextValue, MinerValue } from '@/components/miner-value';
import { TransactionTable } from '@/components/transaction-table';
import { BlockValidationInspector } from '@/components/block-validation-inspector';
import { SaveBookmarkButton } from '@/components/save-bookmark';

// A held arrow key pages no faster than this, in milliseconds.
const REPEAT = 100;

export function BlockPage() {
  const { hash, height } = useParams<{ hash?: string; height?: string }>();
  const identifier = hash ?? height ?? 'latest';
  const navigate = useNavigate();
  const location = useLocation();
  const inspectValidation = location.pathname.endsWith('/header/context');
  const suffix = sectionSuffix(location.pathname);
  const client = useBlockchainClient();
  const now = useNow();
  const {
    data: block,
    isError: blockIsError,
    error: blockError,
  } = useBlockHeader(identifier);
  usePageTitle(block ? `Block ${formatInteger(block.height)}` : 'Block');

  const { data: validation } = useBlockValidation(block?.hash ?? '', {
    enabled: Boolean(block?.hash),
  });
  const statsTarget = block?.hash ?? '';
  const { data: stats, isLoading: statsLoading } = useBlockStats(statsTarget, {
    enabled: Boolean(block?.hash),
  });
  const { data: initialTransactions } = useBlockTransactions(identifier, 0);
  const {
    data: details,
    isLoading: detailsLoading,
    isError: detailsError,
  } = useBlockTransactionDetails(block?.hash ?? '', {
    enabled: Boolean(block?.hash),
  });
  const { data: hashes } = useBlockTransactionHashes(block?.hash ?? '', {
    enabled: Boolean(block?.hash),
  });
  const previousPathRef = useRef<string | null>(null);
  const nextPathRef = useRef<string | null>(null);
  // A block off the confirmed chain steps within its own branch, to its top.
  const detached = validation?.state?.confirmed === false;
  const settled = validation?.state?.confirmed === true;
  const status = settled
    ? 'confirmed'
    : validation?.state?.candidate === true
      ? 'candidate'
      : detached
        ? 'unconfirmed'
        : undefined;
  const { data: branches } = useBranches({ enabled: detached });
  const nextPath = nextBlockPath(block, validation?.state?.confirmed, branches, suffix);
  const previousPath =
    block?.previousBlockHash && block.height > 0
      ? `${blockByHashPath(block.previousBlockHash)}${suffix}`
      : null;
  const [focus, setFocus] = useState<{ position: number; key: number }>();
  const showPosition = (position: number) => {
    setFocus((current) => ({ position, key: (current?.key ?? 0) + 1 }));
  };
  useSwipeNavigation(previousPath, nextPath);
  const rows = useMemo(
    () =>
      details?.map((tx, position) => ({
        position,
        hash: hashes?.at(position),
        virtual: tx.virtual,
        fee: tx.fee,
        cluster: tx.cluster,
      })),
    [details, hashes]
  );

  useLayoutEffect(() => {
    previousPathRef.current = previousPath;
    nextPathRef.current = nextPath;
  }, [previousPath, nextPath]);

  useEffect(() => {
    let last = { path: '', time: 0 };
    const page = (path: string, event: KeyboardEvent) => {
      event.preventDefault();
      if (event.repeat && (path === last.path || event.timeStamp - last.time < REPEAT)) return;
      last = { path, time: event.timeStamp };
      void navigate(path);
    };
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.defaultPrevented) return;
      if (event.metaKey || event.ctrlKey || event.altKey || event.shiftKey) return;

      const target = event.target;
      if (target instanceof HTMLElement) {
        const tag = target.tagName;
        if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
        if (target.isContentEditable) return;
      }

      if (event.key === 'ArrowLeft' && previousPathRef.current) {
        page(previousPathRef.current, event);
        return;
      }

      if (event.key === 'ArrowRight' && nextPathRef.current) {
        page(nextPathRef.current, event);
      }
    };

    globalThis.window.addEventListener('keydown', handleKeyDown);
    return () => {
      globalThis.window.removeEventListener('keydown', handleKeyDown);
    };
  }, [navigate]);

  useEffect(() => {
    if (!block?.hash) return;
    if (hash && hash !== block.hash) {
      void navigate(
        inspectValidation
          ? blockValidationPath(block.hash)
          : `${blockByHashPath(block.hash)}${suffix}`,
        { replace: true }
      );
    }
  }, [block?.hash, hash, inspectValidation, navigate, suffix]);

  // A path ending in a section opens the page at it, once it is drawn.
  const section = sectionId(suffix);
  const sectionReady = section === 'details' ? Boolean(details) : Boolean(block);
  useEffect(() => {
    if (section === undefined || !sectionReady) return;
    document.getElementById(section)?.scrollIntoView({ block: 'start' });
  }, [section, sectionReady, location.key]);

  const timestamp = block?.timestamp;
  const feesSats = stats?.feesSats ?? block?.feesSats;
  const subsidySats = stats?.subsidySats ?? block?.subsidySats;
  const weight = stats?.weight ?? block?.weight;
  const size = stats?.size ?? block?.size;
  const showStatsPlaceholder = Boolean(block) && statsLoading;

  return (
    <div className="space-y-8">
      {blockIsError ? (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Could not load block</p>
          <p className="text-textMuted mt-1 break-words">
            {blockError instanceof Error ? blockError.message : 'Unknown error'}
          </p>
        </div>
      ) : null}
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Block</p>
          <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
            {block ? `${formatInteger(block.height)}` : '—'}
          </h1>
          <p className="text-textMuted mt-2 text-sm">
            {formatRelativeTime(timestamp, now)} · {formatTimestamp(timestamp)}
          </p>
          <div className="mt-2">
            <StatusPill status={status} />
          </div>
        </div>
        <div className="flex min-w-0 flex-col items-end gap-3">
          <div className="flex items-center gap-2">
            <p className="text-textPrimary font-mono text-xs break-all">{block?.hash ?? '—'}</p>
            {block && <CopyButton value={block.hash} label="Copy block hash" />}
          </div>
          <div className="flex flex-wrap items-center justify-end gap-3">
          {block && (
            <SaveBookmarkButton
              target={{ kind: 'block', hash: block.hash, height: block.height }}
            />
          )}
          {previousPath ? (
            <Link
              to={previousPath}
              className={HEADER_ACTION}
            >
              ← Previous
            </Link>
          ) : block ? (
            <span
              aria-disabled="true"
              className="border-accent/20 text-textMuted cursor-not-allowed rounded-md border px-3 py-2 text-sm opacity-50"
            >
              ← Previous
            </span>
          ) : null}
          {nextPath ? (
            <Link
              to={nextPath}
              className={HEADER_ACTION}
            >
              Next →
            </Link>
          ) : block ? (
            <span
              aria-disabled="true"
              className="border-accent/20 text-textMuted cursor-not-allowed rounded-md border px-3 py-2 text-sm opacity-50"
            >
              Next →
            </span>
          ) : null}
          </div>
        </div>
      </header>

      <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-3">
        <InfoItem
          label="Transactions"
          value={formatNumber(block?.txCount, { maximumFractionDigits: 0 })}
        />
        <InfoItem label="Size" value={formatBytes(size)} />
        <InfoItem
          label="Weight"
          value={
            typeof weight === 'number'
              ? formatNumber(weight, { maximumFractionDigits: 0 })
              : showStatsPlaceholder
                ? '…'
                : '—'
          }
        />
        <InfoItem
          label="Fees"
          value={
            typeof feesSats === 'number' ? formatSats(feesSats) : showStatsPlaceholder ? '…' : '—'
          }
        />
        <InfoItem
          label="Subsidy"
          value={
            typeof subsidySats === 'number'
              ? formatSats(subsidySats)
              : showStatsPlaceholder
                ? '…'
                : '—'
          }
        />
        <InfoItem label="Difficulty" value={formatNumber(block?.difficulty ?? undefined)} />
        <InfoItem label="Nonce" value={formatHex(block?.nonce)} />
        <InfoItem label="Bits" value={formatHex(block?.bits)} />
        <InfoItem label="Version" value={formatHex(block?.version)} />
        <InfoItem
          label="Merkle root"
          value={
            <span className="text-textPrimary font-mono text-xs break-all">
              {block?.merkleRoot ?? '—'}
            </span>
          }
        />
        <InfoItem label="Miner" value={<MinerValue coinbase={initialTransactions?.[0]} />} />
        <InfoItem
          label="Coinbase"
          value={<CoinbaseTextValue coinbase={initialTransactions?.[0]} hash={block?.hash} />}
        />
      </section>

      {(typeof size === 'number' ||
        (typeof stats?.nominal === 'number' && typeof weight === 'number')) && (
        <BlockSizeBar weight={weight} nominal={stats?.nominal} size={size} />
      )}

      {block && !details && detailsLoading && (
        <section className="border-accent/30 bg-surface/80 rounded-xl border px-6 py-4">
          <Spinner label="Loading block details…" />
        </section>
      )}

      {block && details && (
        <BlockViewer
          key={block.hash}
          elements={details}
          height={block.height}
          coinbase
          positionPath={(position) => `${blockByHashPath(block.hash)}/tx/${String(position)}`}
          forceExpanded={suffix === '/txs/details'}
          focus={focus}
        />
      )}

      {block && (
        <BlockValidationInspector
          key={JSON.stringify([client.metadata.endpoint, block.hash, inspectValidation])}
          blockHash={block.hash}
          initiallyExpanded={inspectValidation}
        />
      )}

      <TransactionTable
        key={`txs-${block?.hash ?? ''}`}
        rows={rows}
        isLoading={detailsLoading}
        isError={detailsError}
        onPosition={showPosition}
      />
    </div>
  );
}

// The next block, or the template above a block without one.
// The block above on the confirmed chain, else the template above its top.
// Off the confirmed chain it is the block above in its branch, if any.
function nextBlockPath(
  block: BlockHeaderDetail | undefined,
  confirmed: boolean | undefined,
  branches: ChainBranch[] | undefined,
  suffix: string
): string | null {
  if (!block) return null;
  if (block.nextBlockHash) return `${blockByHashPath(block.nextBlockHash)}${suffix}`;
  if (confirmed === true) return `${templatePath(block.height + 1)}${suffix}`;
  if (confirmed === undefined) return null;

  for (const branch of branches ?? []) {
    const index = branch.headers.findIndex((header) => header.hash === block.hash);
    if (index > 0) return `${blockByHashPath(branch.headers[index - 1].hash)}${suffix}`;
  }

  return null;
}

function InfoItem({ label, value }: { label: string; value: ReactNode }) {
  return (
    <div className="space-y-1">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary text-sm">{value}</p>
    </div>
  );
}
