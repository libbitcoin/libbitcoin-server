/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_BLOCKCHAIN_PROVIDER?: 'libbitcoin' | 'blockstream' | 'mempool' | 'external';
  readonly VITE_BLOCKSTREAM_API_URL?: string;
  readonly VITE_MEMPOOL_API_URL?: string;
  readonly VITE_BLOCKCHAIN_API_URL?: string;
  readonly VITE_LIBBITCOIN_API_URL?: string;
  readonly VITE_BITCOIN_NETWORK?: 'mainnet' | 'testnet' | 'regtest';
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
