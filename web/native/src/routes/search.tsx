import { useEffect, useMemo } from 'react';
import { Link, useNavigate, useSearchParams } from 'react-router-dom';
import { classifySearchQuery } from '@/lib/utils/search';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { useBlockHeader, useTransactionHeader } from '@/lib/hooks/useBlockchainData';
import { useProviderInfo } from '@/lib/api/client';
import {
  getNetwork,
  getNetworkParams,
  scriptHashFromAddress,
} from '@/lib/api/providers/libbitcoin-mapper';
import {
  addressPath,
  blockByHashPath,
  blockByHeightPath,
  dashboardPath,
  transactionPath,
  transactionOutputPath,
} from '@/lib/utils/routes';
import { rememberLibbitcoinAddressAlias } from '@/lib/utils/libbitcoin-address-alias';

const NETWORK = getNetwork();
const NETWORK_PARAMS = getNetworkParams(NETWORK);

function getHttpStatus(error: unknown) {
  if (!error || typeof error !== 'object' || Array.isArray(error)) return;
  const status = (error as { status?: unknown }).status;
  return typeof status === 'number' ? status : undefined;
}

function isNotFoundError(error: unknown) {
  return getHttpStatus(error) === 404;
}

function retryHashLookup(failureCount: number, error: Error) {
  return !isNotFoundError(error) && failureCount < 1;
}

function errorMessage(error: unknown) {
  return error instanceof Error ? error.message : 'Unknown provider error';
}

export function SearchPage() {
  const [params] = useSearchParams();
  const rawQuery = params.get('q') ?? '';
  const query = rawQuery.trim();
  const navigate = useNavigate();
  const provider = useProviderInfo();

  const classification = useMemo(() => classifySearchQuery(query), [query]);
  const hashValue = classification.type === 'hash' ? (classification.value ?? '') : '';
  const hashLookupEnabled = Boolean(hashValue);
  const blockLookup = useBlockHeader(hashValue, {
    enabled: hashLookupEnabled,
    retry: retryHashLookup,
  });
  const transactionLookup = useTransactionHeader(hashValue, {
    enabled: hashLookupEnabled,
    retry: retryHashLookup,
  });
  usePageTitle('Search');

  useEffect(() => {
    if (!query) return;
    if (classification.type === 'unknown' || !classification.value) return;
    if (classification.type === 'hash') return;
    const value = classification.value;
    if (classification.type === 'outpoint' && classification.outputIndex !== undefined) {
      void navigate(transactionOutputPath(value, classification.outputIndex), { replace: true });
      return;
    }
    const isLibbitcoin = provider.id === 'libbitcoin';
    const addressScriptInfo =
      classification.type === 'address' && isLibbitcoin
        ? scriptHashFromAddress(value, NETWORK_PARAMS)
        : null;
    const scriptHash = addressScriptInfo?.hash ?? null;
    if (classification.type === 'address' && isLibbitcoin && scriptHash) {
      rememberLibbitcoinAddressAlias(NETWORK, scriptHash, value);
    }
    const target =
      classification.type === 'block-height'
        ? blockByHeightPath(value)
        : addressPath(scriptHash ?? value);
    const state =
      classification.type === 'address' && isLibbitcoin ? { displayAddress: value } : undefined;
    void navigate(target, { replace: true, state });
  }, [classification, navigate, provider.id, query]);

  const blockLookupSettled = blockLookup.isSuccess || blockLookup.isError;
  const transactionLookupSettled = transactionLookup.isSuccess || transactionLookup.isError;
  const blockMissing = blockLookup.isError && isNotFoundError(blockLookup.error);
  const transactionMissing = transactionLookup.isError && isNotFoundError(transactionLookup.error);
  const uniqueBlockMatch = blockLookup.isSuccess && transactionMissing;
  const uniqueTransactionMatch = transactionLookup.isSuccess && blockMissing;
  const blockMatchHash = blockLookup.data?.hash;
  const transactionMatchId = transactionLookup.data?.txid;

  useEffect(() => {
    if (!hashValue || !blockLookupSettled || !transactionLookupSettled) return;
    if (uniqueBlockMatch && blockMatchHash) {
      void navigate(blockByHashPath(blockMatchHash), { replace: true });
      return;
    }
    if (uniqueTransactionMatch && transactionMatchId) {
      void navigate(transactionPath(transactionMatchId), { replace: true });
    }
  }, [
    blockMatchHash,
    blockLookupSettled,
    hashValue,
    navigate,
    transactionMatchId,
    transactionLookupSettled,
    uniqueBlockMatch,
    uniqueTransactionMatch,
  ]);

  if (!query) {
    return (
      <div className="text-textPrimary py-10">
        <p className="text-textMuted">
          Enter a block hash, transaction ID, outpoint (txid:vout), or address in the search bar.
        </p>
      </div>
    );
  }

  if (classification.type === 'unknown') {
    return (
      <div className="text-textPrimary space-y-4 py-10">
        <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
          Unrecognized query
        </h1>
        <p className="text-textMuted">
          We couldn’t determine whether “{rawQuery}” is a block, transaction, or address.
        </p>
        <p className="text-textMuted text-sm">
          Try entering a block height (digits only), a 64-character hash, an outpoint (txid:vout),
          or a valid base58/bech32 address.
        </p>
        <Link to={dashboardPath()} className="text-accent hover:text-accentSecondary">
          Return to dashboard
        </Link>
      </div>
    );
  }

  if (classification.type === 'hash') {
    if (!blockLookupSettled || !transactionLookupSettled) {
      return <div className="text-textMuted py-10">Checking blocks and transactions…</div>;
    }

    if (uniqueBlockMatch || uniqueTransactionMatch) {
      return <div className="text-textMuted py-10">Redirecting…</div>;
    }

    if (blockLookup.isSuccess && transactionLookup.isSuccess) {
      return (
        <div className="text-textPrimary space-y-4 py-10">
          <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
            Hash has multiple matches
          </h1>
          <p className="text-textMuted break-all">
            Choose which resource you intended for “{hashValue}”.
          </p>
          <div className="flex flex-wrap gap-3">
            <Link
              to={blockByHashPath(blockLookup.data.hash)}
              className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm"
            >
              View block
            </Link>
            <Link
              to={transactionPath(transactionLookup.data.txid)}
              className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm"
            >
              View transaction
            </Link>
          </div>
        </div>
      );
    }

    if (blockMissing && transactionMissing) {
      return (
        <div className="text-textPrimary space-y-4 py-10">
          <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
            Hash not found
          </h1>
          <p className="text-textMuted break-all">
            No block or transaction matched “{hashValue}” on {provider.label}.
          </p>
          <Link to={dashboardPath()} className="text-accent hover:text-accentSecondary">
            Return to dashboard
          </Link>
        </div>
      );
    }

    const retrying = blockLookup.isFetching || transactionLookup.isFetching;
    return (
      <div className="text-textPrimary space-y-4 py-10">
        <h1 tabIndex={-1} className="text-accent text-2xl font-semibold">
          Hash lookup incomplete
        </h1>
        <p className="text-textMuted">
          The provider could not conclusively check both blocks and transactions.
        </p>
        {blockLookup.isSuccess ? (
          <Link
            to={blockByHashPath(blockLookup.data.hash)}
            className="text-accent hover:text-accentSecondary block"
          >
            View matching block
          </Link>
        ) : null}
        {transactionLookup.isSuccess ? (
          <Link
            to={transactionPath(transactionLookup.data.txid)}
            className="text-accent hover:text-accentSecondary block"
          >
            View matching transaction
          </Link>
        ) : null}
        {blockLookup.isError && !blockMissing ? (
          <p className="text-textMuted text-sm break-words">
            Block lookup: {errorMessage(blockLookup.error)}
          </p>
        ) : null}
        {transactionLookup.isError && !transactionMissing ? (
          <p className="text-textMuted text-sm break-words">
            Transaction lookup: {errorMessage(transactionLookup.error)}
          </p>
        ) : null}
        <button
          type="button"
          className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-3 py-2 text-sm disabled:cursor-wait disabled:opacity-60"
          disabled={retrying}
          onClick={() => {
            const retries: Array<Promise<unknown>> = [];
            if (blockLookup.isError) retries.push(blockLookup.refetch());
            if (transactionLookup.isError) retries.push(transactionLookup.refetch());
            void Promise.all(retries);
          }}
        >
          {retrying ? 'Retrying…' : 'Retry lookup'}
        </button>
      </div>
    );
  }

  return <div className="text-textMuted py-10">Redirecting…</div>;
}
