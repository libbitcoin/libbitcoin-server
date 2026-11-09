import { useMemo, useState } from 'react';
import { ScriptLookupStatus, type ScriptLookupStatusProps } from './script-lookup-status';
import {
  asciiPreview,
  decodeScript,
  explainInput,
  identifyScript,
  isHexBytes,
  type DecodedScript,
} from '@/lib/utils/script-explainer';

const PAGE_SIZE = 40;

export function ScriptExplainer({
  hex,
  kind,
  scriptType,
  isCoinbase = false,
  witness = [],
  loading = false,
  error,
  onRetry,
}: ScriptLookupStatusProps & {
  kind: 'input' | 'output';
  scriptType?: string;
  isCoinbase?: boolean;
  witness?: string[];
}) {
  const decoded = useMemo(
    () => (hex === undefined || isCoinbase ? undefined : decodeScript(hex)),
    [hex, isCoinbase]
  );
  const input = useMemo(
    () => explainInput(decoded, witness, scriptType, isCoinbase),
    [decoded, witness, scriptType, isCoinbase]
  );
  const pattern = kind === 'output' && decoded ? identifyScript(decoded) : undefined;
  const [witnessLimit, setWitnessLimit] = useState(PAGE_SIZE);
  return (
    <section
      aria-label="Script explainer"
      className="border-accent/30 bg-accent/5 space-y-3 rounded-lg border p-3 text-xs"
    >
      <h3 className="text-accent text-sm font-semibold">Script explainer</h3>
      <p className="text-textMuted">
        Explains structure without executing scripts or verifying signatures. Opcode notes use
        current rules.
      </p>
      {kind === 'input' && <p className="text-textPrimary">{input.description}</p>}
      <ScriptLookupStatus hex={hex} loading={loading} error={error} onRetry={onRetry} />
      {pattern && (
        <div>
          <p className="text-textPrimary font-medium">{pattern.name}</p>
          <p className="text-textMuted mt-1">{pattern.description}</p>
        </div>
      )}
      {decoded && (
        <ScriptBreakdown
          key={decoded.hex}
          script={decoded}
          dataLabels={pattern?.dataLabels ?? input.dataLabels}
        />
      )}
      {input.redeemScript !== undefined && kind === 'input' && (
        <EmbeddedScript hex={input.redeemScript} title="Redeem script" isRedeemScript />
      )}
      {kind === 'input' && witness.length > 0 && (
        <div className="space-y-2">
          <h4 className="text-accent font-medium">
            Witness stack · {witness.length} {witness.length === 1 ? 'item' : 'items'}
          </h4>
          {!isCoinbase && (
            <p className="text-textMuted">
              Roles use the provider’s funding-output type and item positions. Script and public-key
              commitments are not verified.
            </p>
          )}
          <ol className="space-y-2" aria-label="Witness explanations">
            {input.witnessItems.slice(0, witnessLimit).map((item) => {
              const value = witness[item.index];
              return (
                <li key={item.index} className="border-accent/20 min-w-0 rounded border p-2">
                  <p className="text-textPrimary font-medium">
                    Item {item.index} · {item.label}
                  </p>
                  <p className="text-textMuted mt-1">{item.description}</p>
                  <p className="text-textMuted mt-1">
                    {isHexBytes(value) ? `${String(value.length / 2)} bytes` : 'Invalid hex'}
                  </p>
                  <DataPreview hex={value} />
                  {item.scriptContext && (
                    <EmbeddedScript hex={value} title={item.label} context={item.scriptContext} />
                  )}
                </li>
              );
            })}
          </ol>
          {witnessLimit < witness.length && (
            <button
              type="button"
              className="text-accent underline"
              onClick={() => {
                setWitnessLimit((limit) => limit + PAGE_SIZE);
              }}
            >
              Show more witness items
            </button>
          )}
        </div>
      )}
    </section>
  );
}

function EmbeddedScript({
  hex,
  title,
  context = 'legacy',
  isRedeemScript = false,
}: {
  hex: string;
  title: string;
  context?: 'legacy' | 'tapscript';
  isRedeemScript?: boolean;
}) {
  const script = useMemo(() => decodeScript(hex, context), [hex, context]);
  const pattern =
    context === 'legacy'
      ? identifyScript(script, isRedeemScript ? 'redeem' : 'witness')
      : undefined;
  return (
    <details className="border-accent/20 mt-2 rounded border p-2">
      <summary className="text-accent cursor-pointer">Explain {title.toLowerCase()}</summary>
      <div className="mt-3 space-y-2">
        {pattern && (
          <p className="text-textMuted">
            {pattern.name} — {pattern.description}
          </p>
        )}
        <ScriptBreakdown key={hex} script={script} dataLabels={pattern?.dataLabels} />
      </div>
    </details>
  );
}

function ScriptBreakdown({
  script,
  dataLabels,
}: {
  script: DecodedScript;
  dataLabels?: Partial<Record<number, string>>;
}) {
  const [limit, setLimit] = useState(PAGE_SIZE);
  return (
    <div className="space-y-2">
      {script.error && (
        <p role="alert" className="text-accentSecondary break-words">
          {script.error}
        </p>
      )}
      {script.notice && <p className="text-textMuted">{script.notice}</p>}
      {!script.error && script.hex === '' && (
        <p className="text-textMuted">Empty script · 0 bytes</p>
      )}
      {script.tokens.length > 0 && (
        <details>
          <summary className="text-accent cursor-pointer">
            Opcode breakdown · {script.tokens.length}
            {script.error || script.notice ? ' decoded' : ''}{' '}
            {script.tokens.length === 1 ? 'operation' : 'operations'}
          </summary>
          <ol className="mt-3 space-y-2" aria-label="Script operations">
            {script.tokens.slice(0, limit).map((token, index) => (
              <li key={token.offset} className="border-accent/20 min-w-0 rounded border p-2">
                <div className="flex flex-wrap items-baseline justify-between gap-2">
                  <code className="text-textPrimary font-semibold">{token.name}</code>
                  <span className="text-textMuted">
                    Byte {token.offset} · 0x{token.opcode.toString(16).padStart(2, '0')}
                  </span>
                </div>
                <p className="text-textMuted mt-1">{token.description}</p>
                {dataLabels?.[index] && (
                  <p className="text-accentSecondary mt-1">{dataLabels[index]}</p>
                )}
                {token.data !== undefined && <DataPreview hex={token.data} />}
              </li>
            ))}
          </ol>
          {limit < script.tokens.length && (
            <button
              type="button"
              className="text-accent mt-3 underline"
              onClick={() => {
                setLimit((value) => value + PAGE_SIZE);
              }}
            >
              Show more operations
            </button>
          )}
        </details>
      )}
    </div>
  );
}

function DataPreview({ hex }: { hex: string }) {
  const preview = asciiPreview(hex);
  return (
    <div className="mt-1 min-w-0 space-y-1">
      <code className="text-textMuted block font-mono break-all">
        {hex === '' ? '(empty byte string)' : hex.length > 160 ? `${hex.slice(0, 160)}…` : hex}
      </code>
      {preview !== undefined && (
        <p className="text-textMuted break-all">
          ASCII preview: <span className="text-textPrimary">{preview}</span>
        </p>
      )}
    </div>
  );
}
