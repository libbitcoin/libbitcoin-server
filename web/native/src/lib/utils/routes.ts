export function dashboardPath() {
  return '/v1';
}

export function blocksPath() {
  return '/v1/block';
}

export function investigationsPath() {
  return '/v1/investigations';
}

export function blockByHashPath(hash: string) {
  return `/v1/block/hash/${encodeURIComponent(hash)}`;
}

export function blockValidationPath(hash: string) {
  return `${blockByHashPath(hash)}/header/context`;
}

export function blockByHeightPath(height: number | string) {
  return `/v1/block/height/${encodeURIComponent(String(height))}`;
}

export function transactionPath(txid: string) {
  return `/v1/tx/${encodeURIComponent(txid)}`;
}

export function transactionInputPath(txid: string, inputIndex: number) {
  return `${transactionPath(txid)}#input-${String(inputIndex)}`;
}

export function transactionOutputPath(txid: string, outputIndex: number) {
  return `${transactionPath(txid)}#output-${String(outputIndex)}`;
}

export function addressPath(addressOrHash: string) {
  return `/v1/address/${encodeURIComponent(addressOrHash)}`;
}

export function searchPath(query: string) {
  return `/v1/search?q=${encodeURIComponent(query)}`;
}
