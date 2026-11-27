import { MinusIcon, PlusIcon } from '@heroicons/react/24/outline';
import { CopyButton } from '@/components/copy-button';
import { copyTextToClipboard } from '@/lib/utils/clipboard';
import { StatusPill } from '@/components/status-pill';
import { type ReactNode, useEffect, useMemo, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import {
  useLatestBlocks,
  useNetwork,
  useTransaction,
  useTransactionHeader,
  useTransactionInputScriptHex,
  useTransactionPrunedInputs,
  useTransactionOutputScriptHex,
  useTransactionOutputSpends,
} from '@/lib/hooks/useBlockchainData';
import { errorStatus } from '@/lib/api/errors';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { scriptHashFromAddress } from '@/lib/api/providers/libbitcoin-mapper';
import {
  formatBtc,
  formatHash,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
  satsToBtc,
} from '@/lib/utils/format';
import { isNullData } from '@/lib/utils/script';
import { FitHash } from '@/components/fit-hash';
import { MinerValue } from '@/components/miner-value';
import { SaveBookmarkButton } from '@/components/save-bookmark';
import {
  type NetworkParams,
  type TransactionInput,
  type TransactionOutput,
  type TransactionOutputSpend,
} from '@/lib/api/client';
import {
  addressPath,
  dashboardPath,
  transactionPath,
  transactionInputPath,
  transactionOutputPath,
  blockByHashPath,
} from '@/lib/utils/routes';
import { rememberLibbitcoinAddressAlias } from '@/lib/utils/libbitcoin-address-alias';

function canonicalizeAddress(address: string, params: NetworkParams | undefined) {
  if (!params) return address;
  return scriptHashFromAddress(address, params)?.hash ?? address;
}

export function TransactionPage() {
  const { txid, index } = useParams();
  const identifier = txid?.toLowerCase() ?? '';
  const location = useLocation();
  const navigate = useNavigate();
  const { network, params: networkParams } = useNetwork();
  const now = useNow();
  const { data: latestBlocks } = useLatestBlocks(1);
  const {
    data: headerTx,
    isLoading: headerLoading,
    isError: headerIsError,
    error: headerError,
  } = useTransactionHeader(identifier);
  const {
    data: detailTx,
    isLoading: detailLoading,
    isError: detailIsError,
    error: detailError,
  } = useTransaction(identifier);
  const tx = detailTx ?? headerTx;
  const pruned = detailIsError && errorStatus(detailError) === 404;
  const {
    data: prunedInputs,
    isLoading: prunedInputsLoading,
    isError: prunedInputsIsError,
  } = useTransactionPrunedInputs(identifier, { enabled: pruned });
  const inputs = detailTx?.inputs ?? prunedInputs;
  const outputIndices = useMemo(
    () => tx?.outputs.map((output) => output.index) ?? [],
    [tx?.outputs]
  );
  const spendsQuery = useTransactionOutputSpends(identifier, outputIndices);
  const spendsByIndex = new Map(spendsQuery.data?.map((spend) => [spend.outputIndex, spend]));
  const targetMatch =
    index !== undefined && /^\d+$/u.test(index) ? /^\/v1\/(input|output)\//u.exec(location.pathname) : null;
  const targetKind = targetMatch?.[1];
  const targetIndex = targetMatch ? Number(index) : undefined;
  const targetAvailable = Boolean(
    tx &&
    targetKind &&
    (targetKind === 'input' ? tx.inputs : tx.outputs).some((item) => item.index === targetIndex)
  );
  const hasError = headerIsError || (detailIsError && !pruned);
  const error = (headerIsError ? headerError : detailError) ?? null;
  const [expandedInputs, setExpandedInputs] = useState<Record<string, boolean | undefined>>({});
  const [expandedOutputs, setExpandedOutputs] = useState<Record<string, boolean | undefined>>({});
  usePageTitle(identifier ? `Tx ${formatHash(identifier, 8)}` : 'Transaction');

  useEffect(() => {
    if (!targetAvailable || !targetKind || targetIndex === undefined) return;
    // eslint-disable-next-line unicorn/prefer-query-selector -- Row ids are not CSS selectors.
    const target = document.getElementById(`${targetKind}-${String(targetIndex)}`);
    target?.scrollIntoView({ block: 'nearest' });
    target?.focus({ preventScroll: true });
  }, [identifier, targetKind, targetIndex, location.key, targetAvailable]);

  useEffect(() => {
    setExpandedInputs({});
    setExpandedOutputs({});
  }, [identifier]);

  useEffect(() => {
    if (!tx || !network || !networkParams) return;
    const addresses = new Set<string>();
    for (const input of tx.inputs) {
      if (input.address) addresses.add(input.address);
    }
    for (const output of tx.outputs) {
      if (output.address) addresses.add(output.address);
    }
    for (const address of addresses) {
      const scriptInfo = scriptHashFromAddress(address, networkParams);
      if (!scriptInfo) continue;
      rememberLibbitcoinAddressAlias(network, scriptInfo.hash, address);
    }
  }, [network, networkParams, tx]);

  useEffect(() => {
    if (!txid) return;
    const normalized = txid.toLowerCase();
    if (normalized !== txid) {
      void navigate(`${location.pathname.replace(txid, normalized)}${location.search}`, {
        replace: true,
      });
    }
  }, [location.pathname, location.search, navigate, txid]);

  if (!txid) {
    return (
      <div className="text-textPrimary py-10">
        <p className="text-textMuted">Transaction ID missing.</p>
        <Link to={dashboardPath()} className="text-accent hover:text-accentSecondary">
          Go back
        </Link>
      </div>
    );
  }

  const totalOutputBtc = formatBtc(satsToBtc(tx?.totalOutput));
  const totalInputBtc =
    typeof tx?.totalInput === 'number' ? formatBtc(satsToBtc(tx.totalInput)) : '—';
  const feeRate =
    typeof tx?.feeRate === 'number'
      ? `${formatNumber(tx.feeRate, { maximumFractionDigits: 2 })} sat/vB`
      : '—';
  const status = !tx
    ? undefined
    : tx.status === 'confirmed'
      ? 'confirmed'
      : tx.confirmable
        ? 'confirmable'
        : tx.coinbase
          ? 'unconfirmed'
          : 'unconfirmable';
  const receivedTime = tx?.receivedTime;
  const receivedLine = tx
    ? receivedTime
      ? `${formatRelativeTime(receivedTime, now)} · ${formatTimestamp(receivedTime)}`
      : '—'
    : '—';
  const latestHeight = latestBlocks?.[0]?.height;
  const confirmations =
    typeof latestHeight === 'number' && typeof tx?.blockHeight === 'number'
      ? Math.max(0, latestHeight - tx.blockHeight + 1)
      : tx?.confirmations;

  return (
    <div className="space-y-8">
      {hasError ? (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Could not load transaction</p>
          <p className="text-textMuted mt-1 break-words">
            {error instanceof Error ? error.message : 'Unknown error'}
          </p>
          <Link
            to={dashboardPath()}
            className="text-accent hover:text-accentSecondary mt-2 inline-block"
          >
            Return to dashboard
          </Link>
        </div>
      ) : null}
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div className="min-w-0">
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Transaction</p>
          <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
            {identifier.length > 8 ? `${identifier.slice(0, 8)}…` : identifier || '—'}
          </h1>
          <p className="text-textMuted mt-2 text-sm">{receivedLine}</p>
          <div className="mt-2">
            <StatusPill status={status} />
          </div>
        </div>
        <div className="flex min-w-0 flex-col items-end gap-3">
          <div className="flex items-center gap-2">
            <p className="text-textPrimary font-mono text-xs break-all">{identifier || '—'}</p>
            {identifier && <CopyButton value={identifier} label="Copy txid" />}
          </div>
          <div className="flex min-h-[2.375rem] items-center">
            {tx && <SaveBookmarkButton target={{ kind: 'transaction', txid: identifier }} />}
          </div>
        </div>
      </header>

      <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-3">
        <InfoItem label="Confirmations" value={confirmations ?? '—'} />
        <InfoItem
          label="Block height"
          value={
            tx?.blockHash && typeof tx.blockHeight === 'number' ? (
              <Link
                to={blockByHashPath(tx.blockHash)}
                className="text-accent hover:text-accentSecondary"
              >
                {formatNumber(tx.blockHeight, { maximumFractionDigits: 0 })}
              </Link>
            ) : (
              '—'
            )
          }
        />
        <InfoItem
          label="Block hash"
          value={
            tx?.blockHash ? (
              <span className="flex items-center gap-2">
                <span data-fit className="min-w-0 flex-1 overflow-hidden whitespace-nowrap">
                  <Link
                    to={blockByHashPath(tx.blockHash)}
                    className="text-accent hover:text-accentSecondary"
                  >
                    <FitHash value={tx.blockHash} />
                  </Link>
                </span>
                <CopyButton value={tx.blockHash} label="Copy block hash" />
              </span>
            ) : (
              '—'
            )
          }
        />
        <InfoItem label="Total output" value={totalOutputBtc} />
        <InfoItem label="Total input" value={totalInputBtc} />
        <InfoItem label="Fee" value={formatSats(tx?.feeSats)} />
        <InfoItem label="Fee rate" value={feeRate} />
        <InfoItem label="Size" value={tx?.size ? `${formatNumber(tx.size)} bytes` : '—'} />
        <InfoItem
          label="Weight"
          value={tx?.weight ? formatNumber(tx.weight, { maximumFractionDigits: 0 }) : '—'}
        />
        <InfoItem label="Version" value={tx?.version ?? '—'} />
        <InfoItem label="Locktime" value={tx?.locktime ?? '—'} />
        {tx?.inputs[0]?.isCoinbase && (
          <InfoItem label="Miner" value={<MinerValue coinbase={tx} />} />
        )}
      </section>

      {tx && targetKind && !targetAvailable && (
        <p role="alert" className="text-accentSecondary text-sm">
          {targetKind === 'input' ? 'Input' : 'Output'} {targetIndex} does not exist in this
          transaction.
        </p>
      )}

      <section className="grid gap-6 lg:grid-cols-2">
        <div className="border-accent/30 min-w-0 rounded-xl border">
          <header className="border-accentSecondary/30 border-b px-4 py-3">
            <h2 className="text-accent text-sm font-semibold tracking-wide uppercase">Inputs</h2>
          </header>
          <div className="divide-accentSecondary/30 divide-y">
            {!detailTx && detailLoading && (
              <p className="text-textMuted px-4 py-4 text-sm">Loading inputs…</p>
            )}
            {pruned && prunedInputsLoading && (
              <p className="text-textMuted px-4 py-4 text-sm">Loading inputs…</p>
            )}
            {pruned && prunedInputsIsError && (
              <p className="text-textMuted px-4 py-4 text-sm">Inputs are pruned at this height.</p>
            )}
            {detailTx && detailTx.inputs.length === 0 && (
              <p className="text-textMuted px-4 py-4 text-sm">No inputs (coinbase transaction)</p>
            )}
            {inputs?.map((input) => {
              const key = inputKey(input);
              return (
                <TransactionInputRow
                  key={key}
                  txid={identifier}
                  input={input}
                  isHighlighted={targetKind === 'input' && targetIndex === input.index}
                  isExpanded={Boolean(expandedInputs[key])}
                  onToggle={() => {
                    setExpandedInputs((prev) => ({
                      ...prev,
                      [key]: !prev[key],
                    }));
                  }}
                />
              );
            })}
          </div>
        </div>

        <div className="border-accent/30 min-w-0 rounded-xl border">
          <header className="border-accentSecondary/30 flex flex-wrap items-center justify-between gap-2 border-b px-4 py-3">
            <h2 className="text-accent text-sm font-semibold tracking-wide uppercase">Outputs</h2>
            {outputIndices.length > 0 && (
              <button
                type="button"
                onClick={() => {
                  void spendsQuery.refetch();
                }}
                disabled={spendsQuery.isFetching}
                className="text-accent hover:text-accentSecondary text-xs disabled:cursor-wait disabled:opacity-60"
              >
                {spendsQuery.isFetching ? 'Checking spend status…' : 'Refresh spend status'}
              </button>
            )}
          </header>
          <div className="divide-accentSecondary/30 divide-y">
            {!tx && (headerLoading || detailLoading) && (
              <p className="text-textMuted px-4 py-4 text-sm">Loading outputs…</p>
            )}
            {tx?.outputs.map((output) => {
              const key = outputKey(output);
              return (
                <TransactionOutputRow
                  key={key}
                  txid={identifier}
                  output={output}
                  isHighlighted={targetKind === 'output' && targetIndex === output.index}
                  spend={spendsQuery.isError ? undefined : spendsByIndex.get(output.index)}
                  spendLoading={spendsQuery.isPending}
                  isExpanded={Boolean(expandedOutputs[key])}
                  onToggle={() => {
                    setExpandedOutputs((prev) => ({
                      ...prev,
                      [key]: !prev[key],
                    }));
                  }}
                />
              );
            })}
          </div>
        </div>
      </section>
    </div>
  );
}

function InfoItem({ label, value }: { label: string; value: ReactNode }) {
  return (
    <div className="min-w-0 space-y-1">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <div className="text-textPrimary text-sm">{value}</div>
    </div>
  );
}

function TransactionInputRow({
  txid,
  input,
  isHighlighted,
  isExpanded,
  onToggle,
}: {
  txid: string;
  input: TransactionInput;
  isHighlighted: boolean;
  isExpanded: boolean;
  onToggle: () => void;
}) {
  const key = inputKey(input);
  const { params: networkParams } = useNetwork();
  const icons = (
    <span className="ml-auto flex items-center gap-2 text-xs">
      <SaveBookmarkButton target={{ kind: 'input', txid, index: input.index }} compact />
      <button
        type="button"
        onClick={onToggle}
        aria-expanded={isExpanded}
        aria-label={isExpanded ? 'Less' : 'More'}
        title={isExpanded ? 'Less' : 'More'}
        className="text-textMuted hover:text-accent shrink-0"
      >
        {isExpanded ? (
          <MinusIcon className="h-4 w-4" aria-hidden="true" />
        ) : (
          <PlusIcon className="h-4 w-4" aria-hidden="true" />
        )}
      </button>
    </span>
  );

  return (
    <div
      id={`input-${String(input.index)}`}
      role="group"
      aria-label={`Input ${String(input.index)}`}
      tabIndex={-1}
      className={`scroll-mt-24 px-4 py-3 text-sm ${isHighlighted ? 'bg-accent/10 ring-accent ring-2 ring-inset' : ''}`}
    >
      <div className="flex items-start gap-2">
        <PointIndex txid={txid} index={input.index} kind="input" />
        <p data-fit className="text-textMuted min-w-0 flex-1 overflow-hidden text-xs whitespace-nowrap">
          {input.address ? (
            <Link
              to={addressPath(canonicalizeAddress(input.address, networkParams))}
              className="text-accent hover:text-accentSecondary"
              state={{ displayAddress: input.address }}
              title="Spent output payment address"
            >
              <FitHash value={input.address} className="font-mono" />
            </Link>
          ) : input.isCoinbase ? (
            'Coinbase'
          ) : (
            'Unknown address'
          )}
        </p>
        {typeof input.valueSats === 'number' && (
          <p className="text-textPrimary shrink-0 font-medium">{formatSats(input.valueSats)}</p>
        )}
        {input.isCoinbase && icons}
      </div>
      {!input.isCoinbase && (
        <div className="text-textMuted mt-1 flex flex-wrap items-center gap-3 text-xs">
          <span data-fit className="min-w-0 flex-1 overflow-hidden whitespace-nowrap">
            {input.prevTxId && (
              <Link
                to={
                  typeof input.prevOutputIndex === 'number'
                    ? transactionOutputPath(input.prevTxId, input.prevOutputIndex)
                    : transactionPath(input.prevTxId)
                }
                className="text-accent hover:text-accentSecondary font-mono"
                title="Spends output"
              >
                <FitHash value={input.prevTxId} />
                {typeof input.prevOutputIndex === 'number' ? `:${String(input.prevOutputIndex)}` : ''}
              </Link>
            )}
          </span>
          {icons}
        </div>
      )}
      <>
        {isExpanded && (
          <div className="mt-3 space-y-3 text-xs">
            {input.scriptType && <DetailRow label="Script type" value={input.scriptType} />}
            {input.pruned && (
              <p className="text-textMuted">Script and witness are pruned at this height.</p>
            )}
            {input.script && (
              <ScriptDetail kind="input" txid={txid} index={input.index} script={input.script} />
            )}
            {input.witness && input.witness.length > 0 && (
              <div className="space-y-1">
                <p className="text-accentSecondary text-[10px] tracking-wide uppercase">
                  Witness stack ({input.witness.length})
                </p>
                <div className="space-y-1">
                  {input.witness.map((item, idx) => (
                    <code
                      key={`${key}-witness-${String(idx)}`}
                      className="text-textMuted bg-inset block rounded px-2 py-1 font-mono text-[11px] break-all"
                    >
                      {item || '(empty byte string)'}
                    </code>
                  ))}
                </div>
              </div>
            )}
            {typeof input.sequence === 'number' && (
              <DetailRow
                label="Sequence"
                value={`0x${input.sequence.toString(16).toUpperCase()}`}
              />
            )}
          </div>
        )}
      </>
    </div>
  );
}

function TransactionOutputRow({
  txid,
  output,
  isHighlighted,
  spend,
  spendLoading,
  isExpanded,
  onToggle,
}: {
  txid: string;
  output: TransactionOutput;
  isHighlighted: boolean;
  spend?: TransactionOutputSpend;
  spendLoading: boolean;
  isExpanded: boolean;
  onToggle: () => void;
}) {
  const { params: networkParams } = useNetwork();
  const unspendable = isNullData(output.script?.mnemonic);

  return (
    <div
      id={`output-${String(output.index)}`}
      role="group"
      aria-label={`Output ${String(output.index)}`}
      tabIndex={-1}
      className={`scroll-mt-24 px-4 py-3 text-sm ${isHighlighted ? 'bg-accent/10 ring-accent ring-2 ring-inset' : ''}`}
    >
      <div className="flex items-start gap-2">
        <PointIndex txid={txid} index={output.index} kind="output" />
        <p data-fit className="text-textMuted min-w-0 flex-1 overflow-hidden text-xs whitespace-nowrap">
          {output.address ? (
            <Link
              to={addressPath(canonicalizeAddress(output.address, networkParams))}
              className="text-accent hover:text-accentSecondary"
              state={{ displayAddress: output.address }}
              title="Output payment address"
            >
              <FitHash value={output.address} className="font-mono" />
            </Link>
          ) : (
            'Unknown address'
          )}
        </p>
        <p className="text-textPrimary shrink-0 font-medium">{formatSats(output.valueSats)}</p>
      </div>
      <div className="text-textMuted mt-1 flex flex-wrap items-center gap-3 text-xs">
        <span data-fit aria-live="polite" className="min-w-0 flex-1 overflow-hidden whitespace-nowrap">
          {spendLoading ? (
            'Checking spend status…'
          ) : spend?.status === 'spent' ? (
            <Link
              to={transactionInputPath(spend.txid, spend.inputIndex)}
              className="text-accent hover:text-accentSecondary font-mono"
              title="Spent by input"
            >
              <FitHash value={spend.txid} />:{spend.inputIndex}
            </Link>
          ) : unspendable ? (
            <span className="text-amber-300">Unspendable</span>
          ) : spend?.status === 'unspent' ? (
            <span className="text-emerald-300">Unspent</span>
          ) : (
            'Spend status unavailable'
          )}
        </span>
        <span className="ml-auto flex items-center gap-2">
          <SaveBookmarkButton target={{ kind: 'output', txid, index: output.index }} compact />
          <button
            type="button"
            onClick={onToggle}
            aria-expanded={isExpanded}
            aria-label={isExpanded ? 'Less' : 'More'}
            title={isExpanded ? 'Less' : 'More'}
            className="text-textMuted hover:text-accent shrink-0"
          >
            {isExpanded ? (
              <MinusIcon className="h-4 w-4" aria-hidden="true" />
            ) : (
              <PlusIcon className="h-4 w-4" aria-hidden="true" />
            )}
          </button>
        </span>
      </div>
      <>
        {isExpanded && (
          <div className="mt-3 space-y-3 text-xs">
            {output.scriptType && <DetailRow label="Script type" value={output.scriptType} />}
            {output.script && (
              <ScriptDetail kind="output" txid={txid} index={output.index} script={output.script} />
            )}
          </div>
        )}
      </>
    </div>
  );
}

function inputKey(input: { prevTxId?: string; index: number }) {
  return `${input.prevTxId ?? 'coinbase'}-${String(input.index)}`;
}

function outputKey(output: { index: number }) {
  return `output-${String(output.index)}`;
}

// The index of an input or output, which copies its point when clicked.
function PointIndex({ txid, index, kind }: { txid: string; index: number; kind: 'input' | 'output' }) {
  const [copied, setCopied] = useState(false);

  useEffect(() => {
    if (!copied) return;
    const timer = setTimeout(() => {
      setCopied(false);
    }, 1500);
    return () => {
      clearTimeout(timer);
    };
  }, [copied]);

  const label = copied ? 'Copied' : `Copy ${kind} point`;
  return (
    <button
      type="button"
      onClick={() => {
        void copyTextToClipboard(`${txid}:${String(index)}`).then(setCopied);
      }}
      aria-label={label}
      title={label}
      className="text-textMuted bg-accent/10 hover:text-accent shrink-0 rounded px-1.5 font-mono text-xs tabular-nums"
    >
      {copied ? '✓' : `#${String(index)}`}
    </button>
  );
}

// A script as its mnemonic, or as its hex, fetched when first shown.
function ScriptDetail({
  kind,
  txid,
  index,
  script,
}: {
  kind: 'input' | 'output';
  txid: string;
  index: number;
  script: { mnemonic?: string; hex?: string };
}) {
  const [hex, setHex] = useState(false);
  const fetch = hex && script.hex === undefined;
  const input = useTransactionInputScriptHex(txid, index, { enabled: fetch && kind === 'input' });
  const output = useTransactionOutputScriptHex(txid, index, { enabled: fetch && kind === 'output' });
  const query = kind === 'input' ? input : output;
  const value = !hex
    ? (script.mnemonic ?? '')
    : script.hex ?? (query.isError ? 'Script unavailable' : query.data ?? 'Loading…');

  return (
    <div className="space-y-1">
      <p className="text-accentSecondary text-[10px] tracking-wide uppercase">
        Script ·{' '}
        <button
          type="button"
          onClick={() => {
            setHex(!hex);
          }}
          className="text-accent hover:text-accentSecondary uppercase"
          title={hex ? 'Show the script mnemonic' : 'Show the script hex'}
        >
          {hex ? 'hex' : 'mnemonic'}
        </button>
      </p>
      <code className="text-textMuted bg-inset block rounded px-2 py-1 font-mono text-[11px] break-all">
        {value || '(empty)'}
      </code>
    </div>
  );
}

function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="space-y-1">
      <p className="text-accentSecondary text-[10px] tracking-wide uppercase">{label}</p>
      <code className="text-textMuted bg-inset block rounded px-2 py-1 font-mono text-[11px] break-all">
        {value}
      </code>
    </div>
  );
}
