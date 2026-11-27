import { useEffect, useMemo, useRef, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import { BlockViewer } from '@/components/block-viewer';
import { Spinner } from '@/components/spinner';
import { StatusPill } from '@/components/status-pill';
import { TransactionTable } from '@/components/transaction-table';
import { errorStatus } from '@/lib/api/errors';
import {
  useBlockHeader,
  useLatestTemplateHeight,
  usePrefetchTemplate,
  useTemplateNotifications,
  useTemplate,
  useTemplateTransactionHashes,
} from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { useSwipeNavigation } from '@/lib/hooks/useSwipeNavigation';
import { useNow } from '@/lib/hooks/useNow';
import {
  formatInteger,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
} from '@/lib/utils/format';
import { notifyBrowser } from '@/lib/utils/notify';
import {
  followHeight,
  followMove,
  followWhen,
  isOvertaken,
  type Opened,
} from '@/lib/utils/template-follow';
import { loadTemplateDisplay } from '@/lib/utils/template-settings';
import {
  blockByHeightPath,
  sectionId,
  sectionSuffix,
  templatePath,
} from '@/lib/utils/routes';
import { HEADER_ACTION } from '@/lib/utils/styles';

// A held arrow key pages no faster than this, in milliseconds.
const REPEAT = 100;

function reducedMotion(): boolean {
  return globalThis.matchMedia('(prefers-reduced-motion: reduce)').matches;
}

function unavailable(error: unknown): string {
  const status = errorStatus(error);
  if (status === 404) return 'There is no template at this height.';
  if (status === 501) return 'Relay is disabled, so the node keeps no pool.';
  if (status === 503) return 'The pool is not open yet.';
  return error instanceof Error ? error.message : 'Unknown error';
}

// The block below a template, confirmed if it is the top.
function previousPath(height: number, top: number | undefined, suffix: string): string {
  if (top !== undefined && height - 1 <= top) return `${blockByHeightPath(height - 1)}${suffix}`;
  return `${templatePath(height - 1)}${suffix}`;
}

// The template of a block above the confirmed top, as a block page.
export function TemplatePage() {
  const { height: param = '' } = useParams<{ height: string }>();
  const height = Number(param);
  const location = useLocation();
  const navigate = useNavigate();
  const suffix = sectionSuffix(location.pathname);
  usePageTitle(`Template ${formatInteger(height)}`);
  const { data: top } = useBlockHeader('latest');
  const { data: txs, isLoading, isError, error, dataUpdatedAt } = useTemplate(height);
  const now = useNow();
  const updated = txs ? Math.floor(dataUpdatedAt / 1000) : undefined;
  const { data: hashes } = useTemplateTransactionHashes(height);
  const previous = previousPath(height, top?.height, suffix);
  // Paging stops one past the last template, as notified or found.
  useTemplateNotifications();
  const latest = useLatestTemplateHeight();
  const absent = isError && errorStatus(error) === 404;
  const more = latest === undefined ? !absent : height <= latest;
  const next = more ? `${templatePath(height + 1)}${suffix}` : null;

  // The top when this height was opened. A template already mined then opens
  // as its block; one mined while open gives way to the next template.
  const [opened, setOpened] = useState<Opened>();
  const overtaken = isOvertaken(opened, height, top?.height);
  const [departing, setDeparting] = useState(false);
  // The next template may not be built yet, its notification then loads it.
  const awaited = absent && top !== undefined && height === top.height + 1;
  const [focus, setFocus] = useState<{ position: number; key: number }>();
  const showPosition = (position: number) => {
    setFocus((current) => ({ position, key: (current?.key ?? 0) + 1 }));
  };
  useSwipeNavigation(previous, next);
  const clusters = new Set(txs?.slice(1).map((tx) => tx.cluster)).size;
  const size = txs?.reduce((sum, tx) => sum + tx.virtual, 0) ?? 0;
  const fees = txs?.reduce((sum, tx) => sum + tx.fee, 0) ?? 0;
  const rows = useMemo(
    () =>
      (txs ?? []).map((tx, index) => ({
        position: index,
        hash: hashes?.at(index),
        virtual: tx.virtual,
        fee: tx.fee,
        cluster: tx.cluster,
      })),
    [txs, hashes]
  );

  const paged = useRef(0);
  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.defaultPrevented) return;
      if (event.repeat && event.timeStamp - paged.current < REPEAT) return;
      if (event.metaKey || event.ctrlKey || event.altKey || event.shiftKey) return;
      const target = event.target;
      if (target instanceof HTMLElement) {
        const tag = target.tagName;
        if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
        if (target.isContentEditable) return;
      }

      if (event.key === 'ArrowLeft') {
        event.preventDefault();
        paged.current = event.timeStamp;
        void navigate(previous);
      }

      if (event.key === 'ArrowRight' && next) {
        event.preventDefault();
        paged.current = event.timeStamp;
        void navigate(next);
      }
    };

    globalThis.window.addEventListener('keydown', handleKeyDown);
    return () => {
      globalThis.window.removeEventListener('keydown', handleKeyDown);
    };
  }, [navigate, next, previous]);

  useEffect(() => {
    if (top === undefined || opened?.height === height) return;
    if (height <= top.height) {
      void navigate(`${blockByHeightPath(height)}${suffix}`, { replace: true });
      return;
    }
    setOpened({ height, top: top.height });
  }, [height, navigate, opened?.height, suffix, top]);

  useEffect(() => {
    if (!overtaken) return;
    notifyBrowser(`Block ${formatInteger(height)} mined`, 'The template has been mined.', () => {
      void navigate(blockByHeightPath(height));
    });
  }, [height, navigate, overtaken]);

  // The page follows the template of the next block. In 3D the mined template
  // moves out of its scene as the next moves in; the 2D view, a hidden page or
  // reduced motion switches without moving.
  const following = followHeight(opened, height, top?.height);
  const prefetch = usePrefetchTemplate();
  useEffect(() => {
    if (following === undefined) return;
    const follow = () => {
      setDeparting(false);
      void navigate(`${templatePath(following)}${suffix}`, { replace: true });
    };

    const move = followMove(loadTemplateDisplay(), document.hidden, reducedMotion());
    if (move === null) {
      follow();
      return;
    }

    // The next template is loaded while the mined one moves out, so that its
    // page opens drawn rather than loading.
    let current = true;
    setDeparting(move > 0);
    void followWhen(move, prefetch(following)).then(() => {
      if (current) follow();
    });
    return () => {
      current = false;
      setDeparting(false);
    };
  }, [following, navigate, prefetch, suffix]);

  // A path ending in a section opens the page at it, once it is drawn.
  const section = sectionId(suffix);
  const sectionReady = Boolean(txs);
  useEffect(() => {
    if (section === undefined || !sectionReady) return;
    document.getElementById(section)?.scrollIntoView({ block: 'start' });
  }, [section, sectionReady, location.key]);

  return (
    <div className="space-y-8">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Template</p>
          <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
            {formatInteger(height)}
          </h1>
          <p className="text-textMuted mt-2 text-sm">
            {updated ? `${formatRelativeTime(updated, now)} · ${formatTimestamp(updated)}` : '—'}
          </p>
          <div className="mt-2">
            <StatusPill status="unconfirmed" />
          </div>
        </div>
        <div className="flex flex-wrap items-center gap-3">
          <Link
            to={previous}
            className={HEADER_ACTION}
          >
            ← Previous
          </Link>
          {next ? (
            <Link
              to={next}
              className={HEADER_ACTION}
            >
              Next →
            </Link>
          ) : (
            <span
              aria-disabled="true"
              className="border-accent/20 text-textMuted cursor-not-allowed rounded-md border px-3 py-2 text-sm opacity-50"
            >
              Next →
            </span>
          )}
        </div>
      </header>

      {awaited && (
        <div className="py-10">
          <Spinner label="Waiting for the next template…" />
        </div>
      )}
      {isError && !awaited && (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Template unavailable</p>
          <p className="text-textMuted mt-1 break-words">{unavailable(error)}</p>
        </div>
      )}
      {isLoading && (
        <div className="py-10">
          <Spinner label="Loading template…" />
        </div>
      )}

      {txs && (
        <>
          <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-4">
            <Item label="Transactions" value={formatInteger(txs.length)} />
            <Item label="Clusters" value={formatInteger(clusters)} />
            <Item label="Virtual size" value={`${formatInteger(size)} vB`} />
            <Item label="Fees" value={formatSats(fees)} />
          </section>

          <BlockViewer
            elements={txs}
            height={height}
            coinbase
            reservation
            forceExpanded={suffix === '/txs/details'}
            focus={focus}
            departing={departing}
          />

          <TransactionTable key={`txs-${String(height)}`} rows={rows} onPosition={showPosition} />
        </>
      )}
    </div>
  );
}

function Item({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <p className="text-textMuted text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary mt-1 font-medium tabular-nums">{value}</p>
    </div>
  );
}

// The pool opens at the template of the next block.
export function PoolRedirect() {
  usePageTitle('Pool');
  const navigate = useNavigate();
  const { data: top } = useBlockHeader('latest');

  useEffect(() => {
    if (top !== undefined) void navigate(templatePath(top.height + 1), { replace: true });
  }, [navigate, top]);

  return <div className="text-textMuted py-10 text-sm">Loading pool...</div>;
}
