export type Coin = 'bitcoin' | 'litecoin';

const UNKNOWN = 'Unknown';

interface Prefixes {
  p2kh: Readonly<Record<number, string>>;
  p2sh: Readonly<Record<number, string>>;
  wif: Readonly<Record<number, string>>;
  witness: Readonly<Record<string, string>>;
  silent: Readonly<Record<string, string>>;
  hdPrivate: Readonly<Record<number, string>>;
  hdPublic: Readonly<Record<number, string>>;
}

const PREFIXES: Readonly<Record<Coin, Prefixes>> = {
  bitcoin: {
    p2kh: { 0: 'mainnet (1)', 111: 'test networks (m, n)' },
    p2sh: { 5: 'mainnet (3)', 196: 'test networks (2)' },
    wif: { 128: 'mainnet (5, K, L)', 239: 'test networks (9, c)' },
    witness: { bc: 'mainnet', tb: 'testnet and signet', bcrt: 'regtest' },
    silent: { sp: 'mainnet', tsp: 'testnet and signet', sprt: 'regtest' },
    hdPrivate: {
      0x0488ade4: 'mainnet (xprv)',
      0x049d7878: 'mainnet BIP49 (yprv)',
      0x04b2430c: 'mainnet BIP84 (zprv)',
      0x04358394: 'test networks (tprv)',
      0x044a4e28: 'test networks BIP49 (uprv)',
      0x045f18bc: 'test networks BIP84 (vprv)',
    },
    hdPublic: {
      0x0488b21e: 'mainnet (xpub)',
      0x049d7cb2: 'mainnet BIP49 (ypub)',
      0x04b24746: 'mainnet BIP84 (zpub)',
      0x043587cf: 'test networks (tpub)',
      0x044a5262: 'test networks BIP49 (upub)',
      0x045f1cf6: 'test networks BIP84 (vpub)',
    },
  },
  litecoin: {
    p2kh: { 48: 'mainnet (L)', 111: 'test networks (m, n)' },
    p2sh: {
      5: 'mainnet deprecated (3)',
      50: 'mainnet (M)',
      58: 'test networks (Q)',
      196: 'test networks deprecated (2)',
    },
    wif: { 176: 'mainnet (6, T)', 239: 'test networks (9, c)' },
    witness: { ltc: 'mainnet', tltc: 'testnet', rltc: 'regtest' },
    silent: {},
    hdPrivate: {
      0x0488ade4: 'mainnet (xprv)',
      0x019d9cfe: 'mainnet (Ltpv)',
      0x01b26792: 'mainnet BIP49 (Mtpv)',
      0x04358394: 'test networks (tprv)',
      0x0436ef7d: 'test networks (ttpv)',
    },
    hdPublic: {
      0x0488b21e: 'mainnet (xpub)',
      0x019da462: 'mainnet (Ltub)',
      0x01b26ef6: 'mainnet BIP49 (Mtub)',
      0x043587cf: 'test networks (tpub)',
      0x0436f6e1: 'test networks (ttub)',
    },
  },
};

export function coinOf(forks: readonly string[]): Coin {
  return forks.includes('ltc_scrypt_proof_of_work') ? 'litecoin' : 'bitcoin';
}

export function describePrefix(coin: Coin, field: keyof Prefixes, value: number | string) {
  const table: Readonly<Record<string, string | undefined>> = PREFIXES[coin][field];
  return table[String(value)] ?? UNKNOWN;
}
