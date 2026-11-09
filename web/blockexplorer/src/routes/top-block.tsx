import { useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useBlockHeader } from '@/lib/hooks/useBlockchainData';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import { blockByHashPath, blocksPath } from '@/lib/utils/routes';

export function TopBlockPage() {
  usePageTitle('Latest Block');
  const navigate = useNavigate();
  const {
    data: block,
    isError,
    error,
  } = useBlockHeader('latest');

  useEffect(() => {
    if (!block?.hash) return;
    void navigate(blockByHashPath(block.hash), { replace: true });
  }, [block?.hash, navigate]);

  if (isError) {
    return (
      <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary rounded-xl border px-4 py-3 text-sm">
        <p className="text-accent font-semibold">Could not load top block</p>
        <p className="text-textMuted mt-1 break-words">
          {error instanceof Error ? error.message : 'Unknown error'}
        </p>
        <Link to={blocksPath()} className="text-accent hover:text-accentSecondary mt-3 inline-block">
          Browse blocks
        </Link>
      </div>
    );
  }

  return <div className="text-textMuted py-10 text-sm">Loading top block...</div>;
}
