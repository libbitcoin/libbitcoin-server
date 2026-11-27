import { useEffect, useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import { useBlockchainClient, type BlockReference } from '@/lib/api/client';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import {
  blockByHashPath,
  blocksPath,
  transactionInputPath,
  transactionOutputPath,
  transactionPath,
} from '@/lib/utils/routes';

function Unresolved({ title, error }: { title: string; error: unknown }) {
  return (
    <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
      <p className="text-accent font-semibold">{title}</p>
      <p className="text-textMuted mt-1 break-words">
        {error instanceof Error ? error.message : 'Unknown error'}
      </p>
      <Link to={blocksPath()} className="text-accent hover:text-accentSecondary mt-3 inline-block">
        Browse blocks
      </Link>
    </div>
  );
}

// A candidate block route opens the same view of the block by its hash.
export function CandidateBlockRedirect() {
  usePageTitle('Candidate Block');
  const { height = '' } = useParams<{ height: string }>();
  const { pathname } = useLocation();
  const navigate = useNavigate();
  const client = useBlockchainClient();
  const [error, setError] = useState<unknown>(null);
  const suffix = pathname.slice(`/v1/block/candidate/${height}`.length);

  useEffect(() => {
    let active = true;
    client
      .getCandidateBlockHash(Number(height))
      .then((hash) => {
        if (active) void navigate(`${blockByHashPath(hash)}${suffix}`, { replace: true });
      })
      .catch((reason: unknown) => {
        if (active) setError(reason);
      });

    return () => {
      active = false;
    };
  }, [client, height, navigate, suffix]);

  if (error) return <Unresolved title="Could not load candidate block" error={error} />;
  return <div className="text-textMuted py-10 text-sm">Loading candidate block...</div>;
}

function transactionTarget(pathname: string, txid: string, index?: string): string {
  if (index === undefined) return transactionPath(txid);
  if (pathname.includes('/input/')) return transactionInputPath(txid, Number(index));
  return transactionOutputPath(txid, Number(index));
}

function blockReference(pathname: string, id: string): BlockReference {
  if (pathname.startsWith('/v1/block/candidate/')) return { chain: 'candidate', id };
  if (pathname.startsWith('/v1/block/confirmed/')) return { chain: 'confirmed', id };
  return { chain: 'hash', id };
}

// A block tx route opens the tx, at the input or output a script route names.
export function BlockTransactionRedirect() {
  usePageTitle('Transaction');
  const { hash, height, position = '', index } = useParams();
  const { pathname } = useLocation();
  const navigate = useNavigate();
  const client = useBlockchainClient();
  const [error, setError] = useState<unknown>(null);
  const block = blockReference(pathname, hash ?? height ?? '');
  const chain = block.chain;
  const id = block.id;

  useEffect(() => {
    let active = true;
    client
      .getBlockTransactionHash({ chain, id }, Number(position))
      .then((txid) => {
        if (active) void navigate(transactionTarget(pathname, txid, index), { replace: true });
      })
      .catch((reason: unknown) => {
        if (active) setError(reason);
      });

    return () => {
      active = false;
    };
  }, [chain, client, id, index, navigate, pathname, position]);

  if (error) return <Unresolved title="Could not load block transaction" error={error} />;
  return <div className="text-textMuted py-10 text-sm">Loading transaction...</div>;
}
