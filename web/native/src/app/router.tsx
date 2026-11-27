import {
  createBrowserRouter,
  createRoutesFromElements,
  Navigate,
  Route,
  RouterProvider,
} from 'react-router-dom';
import { Suspense } from 'react';
import { AppShell } from './shell';
import { DashboardPage } from '@/routes/dashboard';
import { BlockPage } from '@/routes/block';
import { TransactionPage } from '@/routes/transaction';
import { AddressPage } from '@/routes/address';
import { SearchPage } from '@/routes/search';
import { NotFoundPage } from '@/routes/not-found';
import { BlocksPage } from '@/routes/blocks';
import { TopBlockPage } from '@/routes/top-block';
import { InvestigationsPage } from '@/routes/investigations';
import { ConfigurationPage } from '@/routes/configuration';
import { FeesPage } from '@/routes/fees';
import { TransactionsPage } from '@/routes/transactions';
import { PoolRedirect, TemplatePage } from '@/routes/template';
import { WebMinerPage } from '@/routes/web-miner';
import {
  BlockTransactionRedirect,
  CandidateBlockRedirect,
} from '@/routes/redirects';

// Suffixes of the block paths that open the block page.
const blockViews = [
  '',
  '/header',
  '/header/context',
  '/details',
  '/txs',
  '/txs/details',
  '/filter/:type',
  '/filter/:type/hash',
  '/filter/:type/header',
];

const blockTransactions = [
  '/tx/:position',
  '/tx/:position/input/:index/script',
  '/tx/:position/output/:index/script',
];

const blocks = ['v1/block/hash/:hash', 'v1/block/confirmed/:height', 'v1/block/candidate/:height'];

const router = createBrowserRouter(
  createRoutesFromElements(
    <Route element={<AppShell />} errorElement={<NotFoundPage />}>
      <Route index element={<Navigate to="/v1" replace />} />
      <Route path="v1" element={<DashboardPage />} />
      <Route path="v1/investigations" element={<InvestigationsPage />} />
      <Route path="v1/configuration" element={<ConfigurationPage />} />
      <Route path="v1/top" element={<TopBlockPage />} />
      <Route path="v1/top/confirmed" element={<TopBlockPage />} />
      <Route path="v1/top/candidate" element={<TopBlockPage />} />
      <Route path="v1/top/confirmed/height" element={<TopBlockPage />} />
      <Route path="v1/top/candidate/height" element={<TopBlockPage />} />
      <Route path="v1/block" element={<BlocksPage />} />
      {blockViews.map((suffix) => (
        <Route key={`hash${suffix}`} path={`v1/block/hash/:hash${suffix}`} element={<BlockPage />} />
      ))}
      {blockViews.map((suffix) => (
        <Route
          key={`confirmed${suffix}`}
          path={`v1/block/confirmed/:height${suffix}`}
          element={<BlockPage />}
        />
      ))}
      {blockViews.map((suffix) => (
        <Route
          key={`candidate${suffix}`}
          path={`v1/block/candidate/:height${suffix}`}
          element={<CandidateBlockRedirect />}
        />
      ))}
      {blocks.flatMap((block) =>
        blockTransactions.map((suffix) => (
          <Route
            key={`${block}${suffix}`}
            path={`${block}${suffix}`}
            element={<BlockTransactionRedirect />}
          />
        ))
      )}
      <Route path="v1/tx/:txid" element={<TransactionPage />} />
      <Route path="v1/tx/:txid/header" element={<TransactionPage />} />
      <Route path="v1/tx/:txid/details" element={<TransactionPage />} />
      <Route path="v1/input/:txid" element={<TransactionPage />} />
      <Route path="v1/input/:txid/:index" element={<TransactionPage />} />
      <Route path="v1/input/:txid/:index/script" element={<TransactionPage />} />
      <Route path="v1/input/:txid/:index/witness" element={<TransactionPage />} />
      <Route path="v1/output/:txid" element={<TransactionPage />} />
      <Route path="v1/output/:txid/:index" element={<TransactionPage />} />
      <Route path="v1/output/:txid/:index/script" element={<TransactionPage />} />
      <Route path="v1/output/:txid/:index/spender" element={<TransactionPage />} />
      <Route path="v1/output/:txid/:index/spenders" element={<TransactionPage />} />
      <Route path="v1/address/:address" element={<AddressPage />} />
      <Route path="v1/address/:address/confirmed" element={<AddressPage />} />
      <Route path="v1/address/:address/unconfirmed" element={<AddressPage />} />
      <Route path="v1/address/:address/balance" element={<AddressPage />} />
      <Route path="v1/template" element={<PoolRedirect />} />
      <Route path="v1/template/:height" element={<TemplatePage />} />
      <Route path="v1/template/:height/txs" element={<TemplatePage />} />
      <Route path="v1/template/:height/txs/details" element={<TemplatePage />} />
      <Route path="v1/fees" element={<FeesPage />} />
      <Route path="v1/miner" element={<WebMinerPage />} />
      <Route path="v1/headers" element={<BlocksPage />} />
      <Route path="v1/headers/top" element={<BlocksPage />} />
      <Route path="v1/headers/confirmed" element={<BlocksPage />} />
      <Route path="v1/headers/candidate" element={<BlocksPage />} />
      <Route path="v1/headers/confirmed/top" element={<BlocksPage />} />
      <Route path="v1/headers/candidate/top" element={<BlocksPage />} />
      <Route path="v1/txs" element={<TransactionsPage />} />
      <Route path="v1/txs/details" element={<TransactionsPage />} />
      <Route path="v1/txs/confirmed" element={<TransactionsPage />} />
      <Route path="v1/txs/confirmed/details" element={<TransactionsPage />} />
      <Route path="v1/txs/unconfirmed" element={<TransactionsPage />} />
      <Route path="v1/txs/unconfirmed/details" element={<TransactionsPage />} />
      <Route path="v1/search" element={<SearchPage />} />
      <Route path="*" element={<NotFoundPage />} />
    </Route>
  )
);

export function AppRouter() {
  return (
    <Suspense fallback={<div className="p-6">Loading…</div>}>
      <RouterProvider router={router} />
    </Suspense>
  );
}
