export function dashboardPath() {
  return '/v1';
}

export function blocksPath() {
  return '/v1/block';
}

export function topPath() {
  return '/v1/top';
}

export function configurationPath() {
  return '/v1/configuration';
}

export function feesPath() {
  return '/v1/fees';
}

/** The section suffix of a block or template path (/txs or /txs/details). */
export function sectionSuffix(pathname: string) {
  if (pathname.endsWith('/txs/details')) return '/txs/details';
  if (pathname.endsWith('/txs')) return '/txs';
  return '';
}

/** The id of the page section a suffix opens at, if any. */
export function sectionId(suffix: string) {
  if (suffix === '/txs/details') return 'details';
  if (suffix === '/txs') return 'transactions';
  return undefined;
}

export function transactionsPath() {
  return '/v1/txs/unconfirmed';
}

export function templatePath(height: number | string) {
  return `/v1/template/${encodeURIComponent(String(height))}`;
}

export function investigationsPath() {
  return '/v1/investigations';
}

export function webMinerPath() {
  return '/v1/miner';
}

export function blockByHashPath(hash: string) {
  return `/v1/block/hash/${encodeURIComponent(hash)}`;
}

export function blockValidationPath(hash: string) {
  return `${blockByHashPath(hash)}/header/context`;
}

export function blockByHeightPath(height: number | string) {
  return `/v1/block/confirmed/${encodeURIComponent(String(height))}`;
}

export function transactionPath(txid: string) {
  return `/v1/tx/${encodeURIComponent(txid)}`;
}

export function transactionInputPath(txid: string, inputIndex: number) {
  return `/v1/input/${encodeURIComponent(txid)}/${String(inputIndex)}`;
}

export function transactionOutputPath(txid: string, outputIndex: number) {
  return `/v1/output/${encodeURIComponent(txid)}/${String(outputIndex)}`;
}

export function addressPath(addressOrHash: string) {
  return `/v1/address/${encodeURIComponent(addressOrHash)}`;
}

export function isAddressPath(pathname: string) {
  return pathname.startsWith('/v1/address/');
}

export function searchPath(query: string) {
  return `/v1/search?q=${encodeURIComponent(query)}`;
}
