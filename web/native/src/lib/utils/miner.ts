import pools from '@/vendor/mining-pools/pools.json';

export interface Miner {
  name: string;
  link?: string;
}

interface Pool {
  name: string;
  link?: string;
  addresses: string[];
  tags: string[];
}

const POOLS: Pool[] = pools.map((pool) => ({
  name: pool.name,
  link: pool.link || undefined,
  addresses: pool.addresses,
  tags: pool.tags.map((tag) => tag.toLowerCase()),
}));

function hexToAscii(hex: string) {
  let text = '';
  for (let index = 0; index + 1 < hex.length; index += 2) {
    text += String.fromCodePoint(Number.parseInt(hex.slice(index, index + 2), 16));
  }
  return text;
}

// Pools in list order, each by payout address then by coinbase tag (case-insensitive).
export function identifyMiner(scriptHex: string, addresses: string[]): Miner | undefined {
  const text = hexToAscii(scriptHex).toLowerCase();
  const pool = POOLS.find(
    (item) =>
      item.addresses.some((address) => addresses.includes(address)) ||
      item.tags.some((tag) => text.includes(tag))
  );
  return pool ? { name: pool.name, link: pool.link } : undefined;
}
