import { useId, useState } from 'react';
import { Link } from 'react-router-dom';
import { ChevronDownIcon } from '@heroicons/react/24/outline';
import { useProviderInfo, type BlockValidation } from '@/lib/api/client';
import { useBlockValidation } from '@/lib/hooks/useBlockchainData';
import { blockValidationPath } from '@/lib/utils/routes';

// Descriptions refer to https://github.com/bitcoin/bips. Activation comes only from the node.
const RULE_DESCRIPTIONS: Partial<Record<string, string>> = {
  bip30: 'Duplicate transaction protection',
  bip34: 'Coinbase block height',
  bip42: 'Finite subsidy supply',
  bip65: 'Absolute script timelocks',
  bip66: 'DER signature encoding',
  bip68: 'Relative sequence locks',
  bip90: 'Buried activation rules',
  bip112: 'Relative script timelocks',
  bip113: 'Median time for locktime',
  bip141: 'SegWit consensus rules',
  bip143: 'SegWit v0 signature hashing',
  bip147: 'Empty multisig dummy element',
  bip341: 'Taproot spending rules',
  bip342: 'Tapscript validation',
};

const VALIDATION_FLAGS = [
  ['validated', 'Validated'],
  ['confirmed', 'Confirmed'],
  ['confirmable', 'Confirmable'],
  ['unconfirmable', 'Unconfirmable'],
] as const;

export function BlockValidationInspector({
  blockHash,
  initiallyExpanded = false,
}: {
  blockHash: string;
  initiallyExpanded?: boolean;
}) {
  const [expanded, setExpanded] = useState(initiallyExpanded);
  const regionId = useId();
  const headingId = useId();
  const provider = useProviderInfo();
  const query = useBlockValidation(blockHash, { enabled: expanded });

  return (
    <section
      className="border-accent/30 bg-surface/80 rounded-xl border"
      aria-labelledby={headingId}
    >
      <h2 id={headingId}>
        <button
          type="button"
          aria-expanded={expanded}
          aria-controls={regionId}
          onClick={() => {
            setExpanded((value) => !value);
          }}
          className="text-accent flex w-full items-center justify-between gap-4 rounded-xl px-6 py-4 text-left font-semibold"
        >
          Block validation inspector
          <ChevronDownIcon
            aria-hidden="true"
            className={`h-5 w-5 shrink-0 transition-transform ${expanded ? 'rotate-180' : ''}`}
          />
        </button>
      </h2>
      <div id={regionId} hidden={!expanded}>
        {expanded && (
          <div className="border-accent/20 space-y-6 border-t p-6">
            <div className="flex flex-wrap items-center justify-between gap-3 text-sm">
              <p className="text-textMuted">Reported by {provider.label} for this block.</p>
              <div className="flex flex-wrap items-center gap-4">
                <Link
                  className="text-accent hover:text-accentSecondary"
                  to={blockValidationPath(blockHash)}
                >
                  Link to inspector
                </Link>
                <button
                  type="button"
                  className="text-accent hover:text-accentSecondary disabled:cursor-wait disabled:opacity-60"
                  disabled={query.isFetching}
                  onClick={() => {
                    void query.refetch();
                  }}
                >
                  {query.isFetching
                    ? 'Refreshing…'
                    : query.isError
                      ? 'Retry validation lookup'
                      : 'Refresh validation'}
                </button>
              </div>
            </div>
            {query.isPending ? (
              <p role="status" className="text-textMuted text-sm">
                Loading block validation…
              </p>
            ) : query.isError ? (
              <div
                role="alert"
                className="border-accentSecondary/30 bg-accentSecondary/10 rounded-lg border p-4 text-sm"
              >
                <p className="text-accentSecondary font-medium">Block validation unavailable</p>
                <p className="text-textMuted mt-1 break-words">{query.error.message}</p>
              </div>
            ) : (
              <ValidationDetails validation={query.data} />
            )}
          </div>
        )}
      </div>
    </section>
  );
}

function ValidationDetails({ validation }: { validation: BlockValidation }) {
  // Sort the new entries array; toSorted is outside this app's ES2022 target.
  // eslint-disable-next-line unicorn/no-array-sort
  const rules = Object.entries(validation.rules ?? {}).sort(([left], [right]) =>
    left.localeCompare(right, 'en', { numeric: true })
  );
  const activeCount = rules.filter(([, active]) => active).length;
  const medianTime = validation.medianTimePast;
  const isoTime = medianTime === undefined ? undefined : new Date(medianTime * 1000).toISOString();

  return (
    <>
      <dl className="grid gap-4 text-sm sm:grid-cols-2">
        <div>
          <dt className="text-accentSecondary text-xs tracking-wide uppercase">Median time past</dt>
          <dd className="text-textPrimary mt-1">
            {isoTime ? (
              <>
                <time dateTime={isoTime}>{isoTime.replace('T', ' ').replace('.000Z', ' UTC')}</time>
                <span className="text-textMuted mt-1 block text-xs">
                  Unix timestamp: {medianTime}
                </span>
              </>
            ) : (
              'Not reported'
            )}
          </dd>
        </div>
        {validation.inBestChain !== undefined && (
          <div>
            <dt className="text-accentSecondary text-xs tracking-wide uppercase">In best chain</dt>
            <dd className="mt-1">
              <FlagValue value={validation.inBestChain} />
            </dd>
          </div>
        )}
      </dl>

      <div className="space-y-3">
        <h3 className="text-accent text-sm font-semibold">Validation state</h3>
        {validation.state ? (
          <dl className="grid grid-cols-2 gap-4 text-sm sm:grid-cols-4">
            {VALIDATION_FLAGS.map(([key, label]) => (
              <div key={key}>
                <dt className="text-textMuted">{label}</dt>
                <dd className="mt-1">
                  <FlagValue value={validation.state?.[key]} />
                </dd>
              </div>
            ))}
          </dl>
        ) : (
          <p className="text-textMuted text-sm">
            Validation flags are not reported by this provider.
          </p>
        )}
      </div>

      <div className="space-y-3">
        <div className="flex flex-wrap items-baseline justify-between gap-2">
          <h3 className="text-accent text-sm font-semibold">Consensus rules</h3>
          {rules.length > 0 && (
            <p className="text-textMuted text-xs">
              {activeCount} of {rules.length} reported flags active
            </p>
          )}
        </div>
        <p className="text-textMuted text-sm">
          Flags apply to this block’s context on the selected node. Missing flags are not inferred.
        </p>
        {rules.length > 0 ? (
          <ul
            className="grid gap-3 sm:grid-cols-2 xl:grid-cols-3"
            aria-label="Reported consensus rules"
          >
            {rules.map(([rule, active]) => {
              const description = Object.hasOwn(RULE_DESCRIPTIONS, rule)
                ? RULE_DESCRIPTIONS[rule]
                : undefined;
              const bipNumber = /^bip(\d+)$/u.exec(rule)?.[1];
              return (
                <li key={rule} className="border-accent/20 min-w-0 rounded-lg border p-3 text-sm">
                  <div className="flex items-start justify-between gap-3">
                    {description && bipNumber ? (
                      <a
                        href={`https://github.com/bitcoin/bips/blob/master/bip-${bipNumber.padStart(4, '0')}.mediawiki`}
                        target="_blank"
                        rel="noreferrer"
                        className="text-accent hover:text-accentSecondary font-medium"
                      >
                        BIP {bipNumber}
                      </a>
                    ) : (
                      <span className="text-textPrimary break-all">{rule}</span>
                    )}
                    <FlagValue value={active} positive="Active" negative="Inactive" />
                  </div>
                  {description && <p className="text-textMuted mt-2 text-xs">{description}</p>}
                </li>
              );
            })}
          </ul>
        ) : (
          <p className="text-textMuted text-sm">
            {validation.rules === undefined
              ? 'Consensus rules are not reported by this provider.'
              : 'No consensus rule flags were returned for this block.'}
          </p>
        )}
      </div>
    </>
  );
}

function FlagValue({
  value,
  positive = 'Yes',
  negative = 'No',
}: {
  value?: boolean;
  positive?: string;
  negative?: string;
}) {
  return (
    <span
      className={`inline-flex shrink-0 rounded-md px-2 py-0.5 text-xs ${value === undefined ? 'text-textMuted bg-white/5' : 'bg-accent/10 text-textPrimary'}`}
    >
      {value === undefined ? 'Not reported' : value ? positive : negative}
    </span>
  );
}
