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

const router = createBrowserRouter(
  createRoutesFromElements(
    <Route element={<AppShell />} errorElement={<NotFoundPage />}>
      <Route index element={<Navigate to="/v1" replace />} />
      <Route path="v1" element={<DashboardPage />} />
      <Route path="v1/investigations" element={<InvestigationsPage />} />
      <Route path="v1/top" element={<TopBlockPage />} />
      <Route path="v1/block" element={<BlocksPage />} />
      <Route path="v1/block/hash/:hash" element={<BlockPage />} />
      <Route path="v1/block/hash/:hash/header" element={<BlockPage />} />
      <Route path="v1/block/hash/:hash/header/context" element={<BlockPage />} />
      <Route path="v1/block/hash/:hash/details" element={<BlockPage />} />
      <Route path="v1/block/height/:height" element={<BlockPage />} />
      <Route path="v1/block/height/:height/header" element={<BlockPage />} />
      <Route path="v1/block/height/:height/header/context" element={<BlockPage />} />
      <Route path="v1/block/height/:height/details" element={<BlockPage />} />
      <Route path="v1/tx/:txid" element={<TransactionPage />} />
      <Route path="v1/tx/:txid/header" element={<TransactionPage />} />
      <Route path="v1/tx/:txid/details" element={<TransactionPage />} />
      <Route path="v1/address/:address" element={<AddressPage />} />
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
