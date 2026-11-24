import { useEffect } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import { BlockViewer } from '@/components/block-viewer';
import { HashText } from '@/components/hash-text';
import { errorStatus } from '@/lib/api/errors';
import {
  useBlockHeader,
  useTemplate,
  useTemplateTransactionHashes,
} from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { formatInteger, formatSats } from '@/lib/utils/format';
import {
  blockByHeightPath,
  sectionId,
  sectionSuffix,
  templatePath,
  transactionPath,
} from '@/lib/utils/routes';

function unavailable(error: unknown): string {
  const status = errorStatus(error);
  if (status === 404) return 'There is no template at this height. Only the next block has one.';
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
  usePageTitle(`Template #${param}`);
  const { data: top } = useBlockHeader('latest');
  const { data: txs, isLoading, isError, error } = useTemplate(height);
  const { data: hashes } = useTemplateTransactionHashes(height);
  const previous = previousPath(height, top?.height, suffix);
  const next = `${templatePath(height + 1)}${suffix}`;
  const mined = top !== undefined && height <= top.height;
  const clusters = new Set(txs?.map((tx) => tx.cluster)).size;
  const size = txs?.reduce((sum, tx) => sum + tx.virtual, 0) ?? 0;
  const fees = txs?.reduce((sum, tx) => sum + tx.fee, 0) ?? 0;

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.defaultPrevented || event.repeat) return;
      if (event.metaKey || event.ctrlKey || event.altKey || event.shiftKey) return;
      const target = event.target;
      if (target instanceof HTMLElement) {
        const tag = target.tagName;
        if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
        if (target.isContentEditable) return;
      }

      if (event.key === 'ArrowLeft') {
        event.preventDefault();
        void navigate(previous);
      }

      if (event.key === 'ArrowRight') {
        event.preventDefault();
        void navigate(next);
      }
    };

    globalThis.window.addEventListener('keydown', handleKeyDown);
    return () => {
      globalThis.window.removeEventListener('keydown', handleKeyDown);
    };
  }, [navigate, next, previous]);

  // A template that has since been mined opens as its block.
  useEffect(() => {
    if (mined) void navigate(`${blockByHeightPath(height)}${suffix}`, { replace: true });
  }, [height, mined, navigate, suffix]);

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
            #{param}
          </h1>
          <p className="text-textMuted text-sm">The block as the pool would fill it, if mined next.</p>
        </div>
        <div className="flex flex-wrap items-center gap-3">
          <Link
            to={previous}
            className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm"
          >
            ← Previous
          </Link>
          <Link
            to={next}
            className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm"
          >
            Next →
          </Link>
        </div>
      </header>

      {isError && (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Template unavailable</p>
          <p className="text-textMuted mt-1 break-words">{unavailable(error)}</p>
        </div>
      )}
      {isLoading && <div className="text-textMuted py-10 text-sm">Loading template...</div>}

      {txs && (
        <>
          <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-4">
            <Item label="Transactions" value={formatInteger(txs.length)} />
            <Item label="Clusters" value={formatInteger(clusters)} />
            <Item label="Virtual size" value={`${formatInteger(size)} vB`} />
            <Item label="Fees" value={formatSats(fees)} />
          </section>

          <BlockViewer
            key={height}
            elements={txs}
            height={height}
            coinbase={false}
            forceExpanded={suffix === '/txs/details'}
          />

          <section id="transactions" className="scroll-mt-24">
            <header className="mb-4 flex items-center justify-between">
              <h2 className="text-accent text-lg font-semibold">Transactions</h2>
              <p className="text-textMuted text-sm">
                Showing {formatInteger(txs.length)} transactions
              </p>
            </header>
            <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
              <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
                <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
                  <tr>
                    <th className="px-4 py-3 text-left">Position</th>
                    <th className="px-4 py-3 text-left">TxID</th>
                    <th className="px-4 py-3 text-right">Virtual size</th>
                    <th className="px-4 py-3 text-right">Fee</th>
                    <th className="px-4 py-3 text-right">Rate</th>
                    <th className="px-4 py-3 text-right">Cluster</th>
                  </tr>
                </thead>
                <tbody className="divide-accentSecondary/20 divide-y tabular-nums">
                  {txs.map((tx, index) => (
                    <TransactionRow
                      key={index}
                      position={index + 1}
                      hash={hashes?.at(index)}
                      virtual={tx.virtual}
                      fee={tx.fee}
                      cluster={tx.cluster}
                    />
                  ))}
                </tbody>
              </table>
            </div>
          </section>
        </>
      )}
    </div>
  );
}

function TransactionRow({
  position,
  hash,
  virtual,
  fee,
  cluster,
}: {
  position: number;
  hash?: string;
  virtual: number;
  fee: number;
  cluster: number;
}) {
  const rate = virtual === 0 ? 0 : fee / virtual;
  return (
    <tr>
      <td className="px-4 py-2">{formatInteger(position)}</td>
      <td className="px-4 py-2">
        {hash ? (
          <Link to={transactionPath(hash)} className="text-accent hover:text-accentSecondary">
            <HashText value={hash} />
          </Link>
        ) : (
          '—'
        )}
      </td>
      <td className="px-4 py-2 text-right">{formatInteger(virtual)} vB</td>
      <td className="px-4 py-2 text-right">{formatInteger(fee)} sat</td>
      <td className="px-4 py-2 text-right">{rate.toFixed(2)} sat/vB</td>
      <td className="px-4 py-2 text-right">{formatInteger(cluster)}</td>
    </tr>
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
