import { useMemo } from 'react';
import type { TransactionSummary } from '@/lib/api/client';
import { useTransactionInputScriptHex } from '@/lib/hooks/useBlockchainData';
import { identifyMiner } from '@/lib/utils/miner';

export function MinerValue({ coinbase }: { coinbase?: TransactionSummary }) {
  const input = coinbase?.inputs[0];
  const isCoinbase = input?.isCoinbase === true;
  const txid = isCoinbase ? (coinbase?.txid ?? '') : '';
  const { data: fetchedHex, isError } = useTransactionInputScriptHex(txid, 0, {
    enabled: isCoinbase && input.script?.hex === undefined,
  });
  const scriptHex = input?.script?.hex ?? fetchedHex;
  const miner = useMemo(() => {
    if (!coinbase || scriptHex === undefined) return;
    const addresses = coinbase.outputs.flatMap((output) => output.address ?? []);
    return identifyMiner(scriptHex, addresses) ?? null;
  }, [coinbase, scriptHex]);

  if (!coinbase) return '…';
  if (!isCoinbase || isError) return '—';
  if (miner === undefined) return '…';
  if (miner === null) return 'Unknown';
  if (!miner.link) return miner.name;
  return (
    <a
      href={miner.link}
      target="_blank"
      rel="noreferrer"
      className="text-accent hover:text-accentSecondary"
    >
      {miner.name}
    </a>
  );
}
