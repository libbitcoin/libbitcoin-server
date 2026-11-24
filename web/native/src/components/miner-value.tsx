import { useMemo } from 'react';
import type { TransactionSummary } from '@/lib/api/client';
import {
  useBlockMiner,
  useBlockValidation,
  useTransactionInputScriptHex,
} from '@/lib/hooks/useBlockchainData';
import { coinbaseText } from '@/lib/utils/coinbase';
import { identifyMiner, type Miner } from '@/lib/utils/miner';

// The input script of a coinbase, from the tx or fetched.
function useCoinbaseScript(coinbase?: TransactionSummary) {
  const input = coinbase?.inputs[0];
  const isCoinbase = input?.isCoinbase === true;
  const txid = isCoinbase ? (coinbase?.txid ?? '') : '';
  const { data: fetchedHex, isError } = useTransactionInputScriptHex(txid, 0, {
    enabled: isCoinbase && input.script?.hex === undefined,
  });
  return { isCoinbase, isError, scriptHex: input?.script?.hex ?? fetchedHex };
}

export function MinerValue({ coinbase }: { coinbase?: TransactionSummary }) {
  const { isCoinbase, isError, scriptHex } = useCoinbaseScript(coinbase);
  const miner = useMemo(() => {
    if (!coinbase || scriptHex === undefined) return;
    const addresses = coinbase.outputs.flatMap((output) => output.address ?? []);
    return identifyMiner(scriptHex, addresses) ?? null;
  }, [coinbase, scriptHex]);

  if (!coinbase) return '…';
  if (!isCoinbase || isError) return '—';
  return <MinerName miner={miner} />;
}

export function CoinbaseTextValue({
  coinbase,
  hash,
}: {
  coinbase?: TransactionSummary;
  hash?: string;
}) {
  const { data: validation, isError } = useBlockValidation(hash ?? '');
  const input = coinbase?.inputs[0];
  const mnemonic = input?.isCoinbase ? input.script?.mnemonic : undefined;
  const bip34 = validation?.rules?.bip34;

  if (!coinbase) return '…';
  if (mnemonic === undefined || isError) return '—';
  if (bip34 === undefined) return '…';
  const lines = coinbaseText(mnemonic, bip34);
  if (lines.length === 0) return '—';
  return (
    <span className="flex flex-col font-mono text-xs break-all">
      {lines.map((line, index) => (
        <span key={index}>{line}</span>
      ))}
    </span>
  );
}

export function BlockMinerValue({ hash }: { hash?: string }) {
  const { data: miner, isError } = useBlockMiner(hash ?? '');
  if (!hash || isError) return '—';
  return <MinerName miner={miner} />;
}

function MinerName({ miner }: { miner?: Miner | null }) {
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
