import type { NativeFees } from './native-contract';

export interface FeeHistogramBin {
  minimumRate: number;
  virtualSize: number;
}

// Keep the server's size-based groups intact. They cannot be accurately
// redistributed into fixed fee-rate intervals without the individual fees.
export function feeHistogramBins(histogram: NativeFees['histogram']): FeeHistogramBin[] {
  const bins = histogram
    .filter(([, virtualSize]) => virtualSize > 0)
    .map(([minimumRate, virtualSize]) => ({ minimumRate, virtualSize }));
  bins.sort((left, right) => left.minimumRate - right.minimumRate);
  return bins;
}

const rateFormatter = new Intl.NumberFormat(undefined, { maximumSignificantDigits: 8 });
const sizeFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 0 });

export function formatFeeRate(value: number): string {
  return rateFormatter.format(value);
}

export function formatVirtualSize(value: number): string {
  return `${sizeFormatter.format(value)} vB`;
}
