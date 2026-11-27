import { type ReactNode, useEffect, useMemo, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import {
  useAddressSummary,
  useAddressTransactions,
  useAddressUtxos,
  useNetwork,
} from '@/lib/hooks/useBlockchainData';
import { useNow } from '@/lib/hooks/useNow';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { usePaginatedList } from '@/lib/hooks/usePaginatedList';
import { useWindowScrollToBottom } from '@/lib/hooks/useWindowScrollToBottom';
import {
  formatBtc,
  formatHash,
  formatNumber,
  formatRelativeTime,
  formatSats,
  formatTimestamp,
  satsToBtc,
} from '@/lib/utils/format';
import { useBlockchainClient, type AddressActivityItem } from '@/lib/api/client';
import { AddressQueriesDisabledError } from '@/lib/api/errors';
import { AddressQRCode } from '@/components/qr-code';
import { HashText } from '@/components/hash-text';
import { CopyButton } from '@/components/copy-button';
import { SaveBookmarkButton } from '@/components/save-bookmark';
import { scriptHashFromAddress } from '@/lib/api/providers/libbitcoin-mapper';
import {
  addressPath,
  blockByHeightPath,
  dashboardPath,
  transactionPath,
  transactionOutputPath,
} from '@/lib/utils/routes';
import { inferAddressType } from '@/lib/utils/address';
import {
  lookupLibbitcoinAddressAlias,
  rememberLibbitcoinAddressAlias,
} from '@/lib/utils/libbitcoin-address-alias';
const UTXO_PAGE_SIZE = 25;

const activityKey = (item: AddressActivityItem) => item.id;

function ledgerStatusLabel(item: AddressActivityItem): string {
  if (item.direction === 'sent') return 'Sent';
  if (item.spent === true) return 'Spent';
  if (item.spent === false) return 'Unspent';
  return '—';
}

export function AddressPage() {
  const { address: addressParam } = useParams();
  const navigate = useNavigate();
  const location = useLocation();
  const rawIdentifier = addressParam ?? '';
  const client = useBlockchainClient();
  const now = useNow();
  const { network, params: networkParams } = useNetwork();
  const stateDisplayAddress = (() => {
    const state = location.state as { displayAddress?: unknown } | null;
    const value = typeof state?.displayAddress === 'string' ? state.displayAddress.trim() : '';
    if (!value) return null;
    if (!inferAddressType(value)) return null;
    return value;
  })();
  const paramValue = rawIdentifier.trim();
  const queryAddressKey = useMemo(() => {
    if (!paramValue) return '';

    const isScriptHash = /^[0-9a-fA-F]{64}$/u.test(paramValue);
    const lookupHash = isScriptHash ? paramValue.toLowerCase() : null;
    const rememberedAddress =
      lookupHash && network ? lookupLibbitcoinAddressAlias(network, lookupHash) : null;

    return stateDisplayAddress ?? rememberedAddress ?? paramValue;
  }, [network, paramValue, stateDisplayAddress]);
  const canonicalLibbitcoinHash = useMemo(() => {
    if (!networkParams) return null;
    const info = scriptHashFromAddress(queryAddressKey, networkParams);
    return info?.hash ?? null;
  }, [networkParams, queryAddressKey]);
  const {
    data,
    isLoading: summaryLoading,
    isError: summaryIsError,
    error: summaryError,
  } = useAddressSummary(queryAddressKey);
  const {
    data: initialTransactions,
    isLoading: initialTransactionsLoading,
    error: initialTransactionsError,
  } = useAddressTransactions(queryAddressKey);
  const {
    data: utxos,
    isLoading: utxosLoading,
    isError: utxosIsError,
    error: utxosError,
    isFetching: utxosFetching,
    refetch: refetchUtxos,
  } = useAddressUtxos(queryAddressKey);
  const addressDisabled = [summaryError, utxosError, initialTransactionsError].some(
    (error) => error instanceof AddressQueriesDisabledError
  );
  const [utxoPagination, setUtxoPagination] = useState({ key: '', limit: UTXO_PAGE_SIZE });
  const totalUtxoValue = useMemo(
    () => utxos?.reduce((total, utxo) => total + utxo.valueSats, 0) ?? 0,
    [utxos]
  );
  const titleValue = data?.address ?? queryAddressKey;
  usePageTitle(
    titleValue
      ? `Address ${/^[0-9a-f]{64}$/iu.test(titleValue) ? formatHash(titleValue, 8) : titleValue}`
      : 'Address'
  );
  // The last row carries an opaque cursor only when another ledger page exists.
  const {
    items: activity,
    isFetchingMore,
    hasMore,
    loadMore,
    loadMoreError,
  } = usePaginatedList<AddressActivityItem>({
    initialItems: initialTransactions,
    resetDeps: [client, queryAddressKey, initialTransactions],
    getItemKey: activityKey,
    fetchNextPage: (items) => {
      const last = items.at(-1);
      const cursor = last?.cursor;
      if (!queryAddressKey || cursor === undefined) return Promise.resolve([]);
      return client.getAddressTransactions(queryAddressKey, cursor);
    },
    canFetchMore: (items) => {
      const last = items.at(-1);
      const cursor = last?.cursor;
      return Boolean(queryAddressKey && cursor);
    },
  });

  useEffect(() => {
    if (!paramValue || !network || !canonicalLibbitcoinHash) return;
    rememberLibbitcoinAddressAlias(network, canonicalLibbitcoinHash, queryAddressKey);
    const normalizedParam = /^[0-9a-fA-F]{64}$/u.test(paramValue) ? paramValue.toLowerCase() : null;
    if (normalizedParam !== canonicalLibbitcoinHash) {
      void navigate(addressPath(canonicalLibbitcoinHash), {
        replace: true,
        state: inferAddressType(queryAddressKey) ? { displayAddress: queryAddressKey } : undefined,
      });
    }
  }, [canonicalLibbitcoinHash, navigate, network, paramValue, queryAddressKey]);

  useWindowScrollToBottom({
    enabled: hasMore && !isFetchingMore && loadMoreError === null,
    onBottomReached: () => {
      void loadMore();
    },
  });

  if (!rawIdentifier.trim()) {
    return (
      <div className="text-textPrimary py-10">
        <p className="text-textMuted">Address missing.</p>
        <Link to={dashboardPath()} className="text-accent hover:text-accentSecondary">
          Go back
        </Link>
      </div>
    );
  }

  const addressValue = data?.address ?? queryAddressKey;

  const balanceBtc = formatBtc(satsToBtc(data?.balance));
  const receivedBtc = formatBtc(satsToBtc(data?.totalReceived));
  const sentBtc = formatBtc(satsToBtc(data?.totalSent));

  const utxoPaginationKey = JSON.stringify([client.metadata.endpoint ?? '', queryAddressKey]);
  const visibleUtxoLimit =
    utxoPagination.key === utxoPaginationKey ? utxoPagination.limit : UTXO_PAGE_SIZE;
  const visibleUtxos = (utxos ?? []).slice(0, visibleUtxoLimit);

  return (
    <div className="space-y-8">
      {addressDisabled ? (
        <div className="border-accent/30 bg-surface/80 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Address queries are disabled</p>
          <p className="text-textMuted mt-1">
            This server is configured without an address index.
          </p>
        </div>
      ) : summaryIsError ? (
        <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
          <p className="text-accent font-semibold">Could not load address</p>
          <p className="text-textMuted mt-1 break-words">
            {summaryError instanceof Error ? summaryError.message : 'Unknown error'}
          </p>
        </div>
      ) : null}
      <header className="space-y-3">
        <p className="text-accentSecondary text-sm tracking-wide uppercase">Address</p>
        <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
          <div className="flex min-w-0 items-center gap-2">
            <h1 tabIndex={-1} className="text-accent text-2xl font-semibold break-all">
              {addressValue || '—'}
            </h1>
            {addressValue.trim() && <CopyButton value={addressValue} label="Copy address" />}
          </div>
          {data && (
            <SaveBookmarkButton
              target={{
                kind: 'address',
                address: canonicalLibbitcoinHash ?? queryAddressKey,
                displayAddress: inferAddressType(addressValue) ? addressValue : undefined,
              }}
            />
          )}
        </div>
      </header>

      <section className="grid gap-6 lg:grid-cols-[minmax(0,1fr)_auto]">
        {!addressDisabled && (
          <div className="border-accent/30 bg-surface/80 grid gap-4 rounded-xl border p-6 text-sm sm:grid-cols-2 lg:grid-cols-3">
            <InfoItem label="Balance" value={balanceBtc} />
            <InfoItem label="Total received" value={receivedBtc} />
            <InfoItem label="Total sent" value={sentBtc} />
            <InfoItem
              label="Transaction count"
              value={summaryLoading ? '—' : formatNumber(data?.txCount, { maximumFractionDigits: 0 })}
            />
            <InfoItem
              label="UTXO count"
              value={formatNumber(data?.utxoCount ?? undefined, {
                maximumFractionDigits: 0,
              })}
            />
            <InfoItem label="Address type" value={data?.addressType ?? '—'} />
          </div>
        )}
        <div className="place-self-start">
          <AddressQRCode value={addressValue} />
        </div>
      </section>

      {!addressDisabled && (
        <section aria-labelledby="address-utxos-heading">
          <header className="mb-4 flex flex-wrap items-end justify-between gap-3">
            <div>
              <h2 id="address-utxos-heading" className="text-accent text-lg font-semibold">
                Unspent Outputs
              </h2>
            </div>
            <p className="text-textMuted text-sm" aria-live="polite">
              {utxosLoading
                ? 'Loading outputs…'
                : utxosIsError
                  ? 'Outputs unavailable'
                  : `${formatNumber(utxos?.length ?? 0, {
                      maximumFractionDigits: 0,
                    })} outputs · ${formatSats(totalUtxoValue)}`}
            </p>
          </header>
          <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
            <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
              <caption className="sr-only">
                Unspent transaction outputs for {addressValue || 'this address'}
              </caption>
              <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
                <tr>
                  <th scope="col" className="px-4 py-3 text-left">
                    Outpoint
                  </th>
                  <th scope="col" className="px-4 py-3 text-right">
                    Value
                  </th>
                  <th scope="col" className="px-4 py-3 text-left">
                    Status
                  </th>
                  <th scope="col" className="px-4 py-3 text-left">
                    Block
                  </th>
                </tr>
              </thead>
              <tbody>
                {utxosLoading && (
                  <tr>
                    <td colSpan={4} className="text-textMuted px-4 py-6 text-center">
                      Loading unspent outputs…
                    </td>
                  </tr>
                )}
                {utxosIsError && (
                  <tr>
                    <td colSpan={4} className="px-4 py-6 text-center">
                      <div role="alert">
                        <p className="text-accentSecondary">Could not load unspent outputs.</p>
                        <p className="text-textMuted mt-1 text-xs break-words">
                          {utxosError instanceof Error ? utxosError.message : 'Unknown error'}
                        </p>
                        <button
                          type="button"
                          onClick={() => {
                            void refetchUtxos();
                          }}
                          disabled={utxosFetching}
                          className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary mt-3 rounded-md border px-3 py-1.5 text-xs font-medium disabled:cursor-not-allowed disabled:opacity-60"
                        >
                          {utxosFetching ? 'Retrying…' : 'Retry'}
                        </button>
                      </div>
                    </td>
                  </tr>
                )}
                {!utxosLoading && !utxosIsError && visibleUtxos.length === 0 && (
                  <tr>
                    <td colSpan={4} className="text-textMuted px-4 py-6 text-center">
                      No unspent outputs are currently reported for this address.
                    </td>
                  </tr>
                )}
                {!utxosIsError &&
                  visibleUtxos.map((utxo) => (
                    <tr
                      key={`${utxo.txid}:${String(utxo.outputIndex)}`}
                      className="hover:bg-accentSecondary/10"
                    >
                      <td className="px-4 py-3">
                        <Link
                          to={transactionOutputPath(utxo.txid, utxo.outputIndex)}
                          className="text-accent hover:text-accentSecondary font-mono"
                          title={`${utxo.txid}:${String(utxo.outputIndex)}`}
                          aria-label={`${utxo.txid}:${String(utxo.outputIndex)}`}
                        >
                          <HashText value={utxo.txid} truncate={12} />:
                          {formatNumber(utxo.outputIndex, { maximumFractionDigits: 0 })}
                        </Link>
                      </td>
                      <td className="text-textPrimary px-4 py-3 text-right whitespace-nowrap">
                        {formatSats(utxo.valueSats)}
                      </td>
                      <td
                        className={`px-4 py-3 ${
                          utxo.status === 'confirmed' ? 'text-textPrimary' : 'text-accentSecondary'
                        }`}
                      >
                        {utxo.status === 'confirmed' ? 'Confirmed' : 'Unconfirmed'}
                      </td>
                      <td className="text-textMuted px-4 py-3 whitespace-nowrap">
                        {utxo.blockHeight === undefined ? (
                          utxo.status === 'unconfirmed' ? (
                            'Unconfirmed'
                          ) : (
                            '—'
                          )
                        ) : (
                          <Link
                            to={blockByHeightPath(utxo.blockHeight)}
                            className="text-accent hover:text-accentSecondary"
                          >
                            {formatNumber(utxo.blockHeight, { maximumFractionDigits: 0 })}
                          </Link>
                        )}
                        {utxo.timestamp ? (
                          <span className="mt-1 block text-xs">
                            {formatRelativeTime(utxo.timestamp, now)} ·{' '}
                            {formatTimestamp(utxo.timestamp)}
                          </span>
                        ) : null}
                      </td>
                    </tr>
                  ))}
              </tbody>
            </table>
          </div>
          {(utxos?.length ?? 0) > visibleUtxos.length && (
            <div className="mt-4 flex justify-center">
              <button
                type="button"
                onClick={() => {
                  setUtxoPagination({
                    key: utxoPaginationKey,
                    limit: visibleUtxoLimit + UTXO_PAGE_SIZE,
                  });
                }}
                className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary rounded-md border px-4 py-2 text-sm font-medium"
              >
                Load more outputs
              </button>
            </div>
          )}
        </section>
      )}

      {!addressDisabled && (
        <section>
          <header className="mb-4 flex items-center justify-between gap-4">
            <h2 className="text-accent text-lg font-semibold">Recent Transactions</h2>
            <p className="text-textMuted text-sm">Showing {activity.length} entries</p>
          </header>
          <div className="border-accent/30 overflow-hidden overflow-x-auto rounded-xl border">
            <table className="divide-accentSecondary/30 min-w-full divide-y text-sm">
              <thead className="bg-accentSecondary/20 text-textMuted text-xs uppercase">
                <tr>
                  <th className="px-4 py-3 text-left">TxID</th>
                  <th className="px-4 py-3 text-right">Value change</th>
                  <th className="px-4 py-3 text-left">Status</th>
                </tr>
              </thead>
              <tbody>
                {initialTransactionsLoading && activity.length === 0 && (
                  <tr>
                    <td colSpan={3} className="text-textMuted px-4 py-6 text-center">
                      Loading transactions…
                    </td>
                  </tr>
                )}
                {initialTransactionsError && activity.length === 0 && (
                  <tr>
                    <td colSpan={3} className="text-accentSecondary px-4 py-6 text-center">
                      Could not load transactions.
                    </td>
                  </tr>
                )}
                {!initialTransactionsLoading &&
                  !initialTransactionsError &&
                  activity.length === 0 && (
                    <tr>
                      <td colSpan={3} className="text-textMuted px-4 py-6 text-center">
                        No transactions recorded.
                      </td>
                    </tr>
                  )}
                {activity.map((item) => (
                  <tr key={item.id} className="hover:bg-accentSecondary/10">
                    <td className="px-4 py-3">
                      <Link
                        to={transactionPath(item.txid)}
                        className="text-accent hover:text-accentSecondary font-mono"
                      >
                        <HashText value={item.txid} truncate={12} />
                      </Link>
                    </td>
                    <td
                      className={`px-4 py-3 text-right ${
                        item.valueChange >= 0 ? 'text-textPrimary' : 'text-accentSecondary'
                      }`}
                    >
                      {item.valueChange >= 0 ? '+' : '-'}
                      {formatSats(Math.abs(item.valueChange))}
                    </td>
                    <td className="text-textMuted px-4 py-3">{ledgerStatusLabel(item)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          {hasMore && (
            <div className="mt-4 flex flex-col items-center gap-3">
              {loadMoreError && (
                <p role="alert" className="text-accentSecondary text-center text-sm break-words">
                  Could not load more transactions. {loadMoreError}
                </p>
              )}
              <button
                type="button"
                onClick={() => {
                  void loadMore();
                }}
                disabled={isFetchingMore}
                className="border-accent text-accent hover:border-accentSecondary hover:text-accentSecondary rounded-md border px-4 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-60"
              >
                {isFetchingMore
                  ? 'Loading more…'
                  : loadMoreError
                    ? 'Retry loading transactions'
                    : 'Load more transactions'}
              </button>
            </div>
          )}
        </section>
      )}
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
