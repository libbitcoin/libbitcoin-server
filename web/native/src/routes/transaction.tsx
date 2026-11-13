import { type ReactNode, useEffect, useMemo, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import { useScriptExplainerPreference } from '@/app/script-explainer-preference';
import {
  useLatestBlocks,
  useTransaction,
  useTransactionHeader,
  useTransactionInputScriptHex,
  useTransactionOutputScriptHex,
  useTransactionOutputSpends,
} from '@/lib/hooks/useBlockchainData';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import {
  getNetwork,
  getNetworkParams,
  scriptHashFromAddress,
} from '@/lib/api/providers/libbitcoin-mapper';
import {
  formatBtc,
  formatHash,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
  satsToBtc,
} from '@/lib/utils/format';
import { copyTextToClipboard } from '@/lib/utils/clipboard';
import { HashText } from '@/components/hash-text';
import { ScriptExplainer } from '@/components/script-explainer';
import { ScriptLookupStatus } from '@/components/script-lookup-status';
import { SaveBookmarkButton } from '@/components/save-bookmark';
import {
  type TransactionInput,
  type TransactionOutput,
  type TransactionOutputSpend,
  useProviderInfo,
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

const NETWORK = getNetwork();
const NETWORK_PARAMS = getNetworkParams(NETWORK);

function canonicalizeAddressForProvider(providerId: string, address: string) {
  if (providerId !== 'libbitcoin') return address;
  return scriptHashFromAddress(address, NETWORK_PARAMS)?.hash ?? address;
}

export function TransactionPage() {
  const { txid } = useParams();
  const identifier = txid?.toLowerCase() ?? '';
  const location = useLocation();
  const navigate = useNavigate();
  const provider = useProviderInfo();
  const now = useNow();
  const { data: latestBlocks } = useLatestBlocks(1);
  const {
    data: headerTx,
    isLoading: headerLoading,
    isError: headerIsError,
    error: headerError,
  } = useTransactionHeader(identifier);
  const enableDetail = provider.id === 'libbitcoin';
  const {
    data: detailTx,
    isLoading: detailLoading,
    isError: detailIsError,
    error: detailError,
  } = useTransaction(identifier, { enabled: enableDetail });
  const tx = detailTx ?? headerTx;
  const outputIndices = useMemo(
    () => tx?.outputs.map((output) => output.index) ?? [],
    [tx?.outputs]
  );
  const spendsQuery = useTransactionOutputSpends(identifier, outputIndices);
  const spendsByIndex = new Map(spendsQuery.data?.map((spend) => [spend.outputIndex, spend]));
  const targetMatch = /^#(input|output)-(\d+)$/u.exec(location.hash);
  const targetKind = targetMatch?.[1];
  const targetIndex = targetMatch ? Number(targetMatch[2]) : undefined;
  const targetAvailable = Boolean(
    tx &&
    targetKind &&
    (targetKind === 'input' ? tx.inputs : tx.outputs).some((item) => item.index === targetIndex)
  );
  const hasError = headerIsError || detailIsError;
  const error = (detailIsError ? detailError : headerError) ?? null;
  const [expandedInputs, setExpandedInputs] = useState<Record<string, boolean | undefined>>({});
  const [expandedOutputs, setExpandedOutputs] = useState<Record<string, boolean | undefined>>({});
  usePageTitle(identifier ? `Tx ${formatHash(identifier, 8)}` : 'Transaction');

  useEffect(() => {
    if (!targetAvailable) return;
    // eslint-disable-next-line unicorn/prefer-query-selector -- Fragment IDs are not CSS selectors.
    const target = document.getElementById(location.hash.slice(1));
    target?.scrollIntoView({ block: 'center' });
    target?.focus({ preventScroll: true });
  }, [identifier, location.hash, location.key, targetAvailable]);

  useEffect(() => {
    setExpandedInputs({});
    setExpandedOutputs({});
  }, [identifier, provider.id]);

  useEffect(() => {
    if (provider.id !== 'libbitcoin') return;
    if (!tx) return;
    const addresses = new Set<string>();
    for (const input of tx.inputs) {
      if (input.address) addresses.add(input.address);
    }
    for (const output of tx.outputs) {
      if (output.address) addresses.add(output.address);
    }
    for (const address of addresses) {
      const scriptInfo = scriptHashFromAddress(address, NETWORK_PARAMS);
      if (!scriptInfo) continue;
      rememberLibbitcoinAddressAlias(NETWORK, scriptInfo.hash, address);
    }
  }, [provider.id, tx]);

  useEffect(() => {
    if (!txid) return;
    const normalized = txid.toLowerCase();
    if (normalized !== txid) {
      void navigate(`${transactionPath(normalized)}${location.search}${location.hash}`, {
        replace: true,
      });
    }
  }, [location.hash, location.search, navigate, txid]);

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
  const status = tx?.status;
  const receivedTime = tx?.receivedTime;
  const receivedLine = tx
    ? receivedTime
      ? `${formatRelativeTime(receivedTime, now)} · ${formatTimestamp(receivedTime)}`
      : 'Pending'
    : '—';
  const latestHeight = latestBlocks?.[0]?.height;
  const confirmations =
    typeof latestHeight === 'number' && typeof tx?.blockHeight === 'number'
      ? Math.max(0, latestHeight - tx.blockHeight + 1)
      : tx?.confirmations;
  const statusStyles =
    status === 'confirmed'
      ? 'bg-emerald-500/20 text-emerald-300'
      : status === 'unconfirmed'
        ? 'bg-amber-500/20 text-amber-300'
        : 'bg-slate-500/20 text-slate-300';

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
      <header className="space-y-2">
        <p className="text-accentSecondary text-sm tracking-wide uppercase">Transaction</p>
        <div className="flex flex-wrap items-center gap-3">
          <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
            <HashText value={identifier} truncate={19} />
          </h1>
          <button
            type="button"
            onClick={() => {
              void copyTextToClipboard(identifier);
            }}
            disabled={!identifier.trim()}
            className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary inline-flex items-center gap-2 rounded-md border px-3 py-1 text-xs font-medium tracking-wide uppercase"
          >
            Copy TxID
          </button>
          {tx && <SaveBookmarkButton target={{ kind: 'transaction', txid: identifier }} />}
        </div>
        <p className="text-textMuted text-sm">{receivedLine}</p>
        <span
          className={`inline-flex w-fit items-center gap-2 rounded-full px-3 py-1 text-xs font-medium tracking-wide uppercase ${statusStyles}`}
        >
          <span className="h-2 w-2 rounded-full bg-current" aria-hidden />
          {status ?? '—'}
        </span>
      </header>

      <section className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-3">
        <InfoItem label="Confirmations" value={confirmations ?? '—'} />
        <InfoItem label="Block height" value={tx?.blockHeight ?? '—'} />
        <InfoItem
          label="Block hash"
          value={
            tx?.blockHash ? (
              <Link to={blockByHashPath(tx.blockHash)}>
                <HashText value={tx.blockHash} />
              </Link>
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
      </section>

      {tx && targetKind && !targetAvailable && (
        <p role="alert" className="text-accentSecondary text-sm">
          {targetKind === 'input' ? 'Input' : 'Output'} {targetIndex} does not exist in this
          transaction.
        </p>
      )}

      <section className="grid gap-6 lg:grid-cols-2">
        <div className="border-accent/30 rounded-xl border">
          <header className="border-accentSecondary/30 border-b px-4 py-3">
            <h2 className="text-accent text-sm font-semibold tracking-wide uppercase">Inputs</h2>
          </header>
          <div className="divide-accentSecondary/30 divide-y">
            {!tx && (headerLoading || detailLoading) && (
              <p className="text-textMuted px-4 py-4 text-sm">Loading inputs…</p>
            )}
            {tx && tx.inputs.length === 0 && (
              <p className="text-textMuted px-4 py-4 text-sm">No inputs (coinbase transaction)</p>
            )}
            {tx?.inputs.map((input) => {
              const key = inputKey(input);
              return (
                <TransactionInputRow
                  key={key}
                  txid={identifier}
                  input={input}
                  isHighlighted={targetKind === 'input' && targetIndex === input.index}
                  providerId={provider.id}
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

        <div className="border-accent/30 rounded-xl border">
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
                  providerId={provider.id}
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
    <div className="space-y-1">
      <p className="text-accentSecondary text-xs tracking-wide uppercase">{label}</p>
      <p className="text-textPrimary text-sm">{value}</p>
    </div>
  );
}

function TransactionInputRow({
  txid,
  input,
  isHighlighted,
  providerId,
  isExpanded,
  onToggle,
}: {
  txid: string;
  input: TransactionInput;
  isHighlighted: boolean;
  providerId: string;
  isExpanded: boolean;
  onToggle: () => void;
}) {
  const key = inputKey(input);
  const { enabled: scriptExplainerEnabled } = useScriptExplainerPreference();
  const shouldFetchHex = isExpanded && input.script?.hex === undefined;
  const {
    data: scriptHex,
    isError: scriptHexIsError,
    error: scriptHexError,
    isFetching: scriptHexLoading,
    refetch: refetchScriptHex,
  } = useTransactionInputScriptHex(txid, input.index, { enabled: shouldFetchHex });

  const resolvedScriptHex = input.script?.hex ?? (scriptHexIsError ? undefined : scriptHex);
  const scriptHexErrorMessage = scriptHexIsError
    ? scriptHexError instanceof Error
      ? scriptHexError.message
      : 'Failed to load script hex'
    : null;

  return (
    <div
      id={`input-${String(input.index)}`}
      role="group"
      aria-label={`Input ${String(input.index)}`}
      tabIndex={-1}
      className={`scroll-mt-24 px-4 py-3 text-sm ${isHighlighted ? 'bg-accent/10 ring-accent ring-2 ring-inset' : ''}`}
    >
      <div className="flex items-start gap-2">
        <p className="text-textMuted min-w-0 flex-1 font-mono text-xs">
          {input.isCoinbase ? (
            'Coinbase'
          ) : input.prevTxId ? (
            <Link
              to={
                typeof input.prevOutputIndex === 'number'
                  ? transactionOutputPath(input.prevTxId, input.prevOutputIndex)
                  : transactionPath(input.prevTxId)
              }
              className="text-accent hover:text-accentSecondary"
            >
              <HashText value={input.prevTxId} truncate={16} />
              {typeof input.prevOutputIndex === 'number' ? `:${String(input.prevOutputIndex)}` : ''}
            </Link>
          ) : (
            '—'
          )}
        </p>
        {typeof input.valueSats === 'number' && (
          <p className="text-textPrimary shrink-0 font-medium">{formatSats(input.valueSats)}</p>
        )}
      </div>
      <div className="text-textMuted mt-1 flex flex-wrap items-center gap-3 text-xs">
        <span>Index {input.index}</span>
        <SaveBookmarkButton target={{ kind: 'input', txid, index: input.index }} compact />
        {input.scriptType && <span>{input.scriptType}</span>}
        {input.address &&
          (() => {
            const canonical = canonicalizeAddressForProvider(providerId, input.address);
            return (
              <Link
                to={addressPath(canonical)}
                className="text-accent hover:text-accentSecondary"
                state={providerId === 'libbitcoin' ? { displayAddress: input.address } : undefined}
              >
                <HashText value={input.address} truncate={12} className="font-mono" />
              </Link>
            );
          })()}
      </div>
      <>
        <button
          type="button"
          onClick={onToggle}
          aria-expanded={isExpanded}
          className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary mt-3 inline-flex items-center gap-1 rounded-md border px-2 py-1 text-[11px] font-medium tracking-wide uppercase"
        >
          {isExpanded ? 'Hide details' : 'Show details'}
        </button>
        {isExpanded && (
          <div className="border-accent/20 bg-surface/60 mt-3 space-y-3 rounded-lg border p-3 text-xs">
            {scriptExplainerEnabled ? (
              <ScriptExplainer
                hex={resolvedScriptHex}
                kind="input"
                scriptType={input.scriptType}
                isCoinbase={input.isCoinbase}
                witness={input.witness}
                loading={scriptHexLoading}
                error={scriptHexErrorMessage}
                onRetry={() => {
                  void refetchScriptHex();
                }}
              />
            ) : (
              <ScriptLookupStatus
                hex={resolvedScriptHex}
                loading={scriptHexLoading}
                error={scriptHexErrorMessage}
                onRetry={() => {
                  void refetchScriptHex();
                }}
              />
            )}
            {resolvedScriptHex !== undefined && (
              <DetailRow label="Script (hex)" value={resolvedScriptHex || '(empty)'} />
            )}
            {input.script?.mnemonic && (
              <DetailRow label="Script (mnemonic)" value={input.script.mnemonic} />
            )}
            {input.witness && input.witness.length > 0 && (
              <div className="space-y-1">
                <p className="text-accentSecondary text-[10px] tracking-wide uppercase">Witness</p>
                <div className="space-y-1">
                  {input.witness.map((item, idx) => (
                    <code
                      key={`${key}-witness-${String(idx)}`}
                      className="text-textMuted block rounded bg-black/30 px-2 py-1 font-mono text-[11px] break-all"
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
  providerId,
  isExpanded,
  onToggle,
}: {
  txid: string;
  output: TransactionOutput;
  isHighlighted: boolean;
  spend?: TransactionOutputSpend;
  spendLoading: boolean;
  providerId: string;
  isExpanded: boolean;
  onToggle: () => void;
}) {
  const { enabled: scriptExplainerEnabled } = useScriptExplainerPreference();
  const shouldFetchHex = isExpanded && output.script?.hex === undefined;
  const {
    data: scriptHex,
    isError: scriptHexIsError,
    error: scriptHexError,
    isFetching: scriptHexLoading,
    refetch: refetchScriptHex,
  } = useTransactionOutputScriptHex(txid, output.index, { enabled: shouldFetchHex });

  const resolvedScriptHex = output.script?.hex ?? (scriptHexIsError ? undefined : scriptHex);
  const scriptHexErrorMessage = scriptHexIsError
    ? scriptHexError instanceof Error
      ? scriptHexError.message
      : 'Failed to load script hex'
    : null;

  return (
    <div
      id={`output-${String(output.index)}`}
      role="group"
      aria-label={`Output ${String(output.index)}`}
      tabIndex={-1}
      className={`scroll-mt-24 px-4 py-3 text-sm ${isHighlighted ? 'bg-accent/10 ring-accent ring-2 ring-inset' : ''}`}
    >
      <div className="flex items-start gap-2">
        <p className="text-textMuted min-w-0 flex-1 text-xs">
          {output.address ? (
            <Link
              to={addressPath(canonicalizeAddressForProvider(providerId, output.address))}
              className="text-accent hover:text-accentSecondary"
              state={providerId === 'libbitcoin' ? { displayAddress: output.address } : undefined}
            >
              <HashText value={output.address} truncate={12} className="font-mono" />
            </Link>
          ) : (
            'Unknown'
          )}
        </p>
        <p className="text-textPrimary shrink-0 font-medium">{formatSats(output.valueSats)}</p>
      </div>
      <div className="text-textMuted mt-1 flex flex-wrap items-center gap-3 text-xs">
        <span>Index {output.index}</span>
        <SaveBookmarkButton target={{ kind: 'output', txid, index: output.index }} compact />
        {output.scriptType && <span>{output.scriptType}</span>}
        <button
          type="button"
          onClick={() => {
            void copyTextToClipboard(`${txid}:${String(output.index)}`);
          }}
          className="text-accent hover:text-accentSecondary"
        >
          Copy outpoint
        </button>
      </div>
      <div className="text-textMuted mt-2 text-xs" aria-live="polite">
        {spendLoading ? (
          'Checking spend status…'
        ) : spend?.status === 'spent' ? (
          <Link
            to={transactionInputPath(spend.txid, spend.inputIndex)}
            className="text-accent hover:text-accentSecondary inline-flex flex-wrap items-center gap-1"
          >
            Spent by <HashText value={spend.txid} truncate={12} /> · input {spend.inputIndex}
          </Link>
        ) : spend?.status === 'unspent' ? (
          'Unspent'
        ) : (
          'Spend status unavailable'
        )}
      </div>
      <>
        <button
          type="button"
          onClick={onToggle}
          aria-expanded={isExpanded}
          className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary mt-3 inline-flex items-center gap-1 rounded-md border px-2 py-1 text-[11px] font-medium tracking-wide uppercase"
        >
          {isExpanded ? 'Hide details' : 'Show details'}
        </button>
        {isExpanded && (
          <div className="border-accent/20 bg-surface/60 mt-3 space-y-3 rounded-lg border p-3 text-xs">
            {scriptExplainerEnabled ? (
              <ScriptExplainer
                hex={resolvedScriptHex}
                kind="output"
                loading={scriptHexLoading}
                error={scriptHexErrorMessage}
                onRetry={() => {
                  void refetchScriptHex();
                }}
              />
            ) : (
              <ScriptLookupStatus
                hex={resolvedScriptHex}
                loading={scriptHexLoading}
                error={scriptHexErrorMessage}
                onRetry={() => {
                  void refetchScriptHex();
                }}
              />
            )}
            {resolvedScriptHex !== undefined && (
              <DetailRow label="Script (hex)" value={resolvedScriptHex || '(empty)'} />
            )}
            {output.script?.mnemonic && (
              <DetailRow label="Script (mnemonic)" value={output.script.mnemonic} />
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

function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="space-y-1">
      <p className="text-accentSecondary text-[10px] tracking-wide uppercase">{label}</p>
      <code className="text-textMuted block rounded bg-black/30 px-2 py-1 font-mono text-[11px] break-all">
        {value}
      </code>
    </div>
  );
}
