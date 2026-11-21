import { expect, test } from 'vitest';
import { feeHistogramBins } from '@/lib/fee-histogram';

test('fee_histogram__fee_histogram_bins__empty__empty', () => {
  expect(feeHistogramBins([])).toEqual([]);
});

test('fee_histogram__fee_histogram_bins__unordered__ascending_rate', () => {
  expect(feeHistogramBins([[5_000, 200], [1_000, 100], [3_000, 300]])).toEqual([{ minimumRate: 1_000, virtualSize: 100 }, { minimumRate: 3_000, virtualSize: 300 }, { minimumRate: 5_000, virtualSize: 200 }]);
});

test('fee_histogram__fee_histogram_bins__empty_group__excluded', () => {
  expect(feeHistogramBins([[2_000, 0], [1_000, 100]])).toEqual([{ minimumRate: 1_000, virtualSize: 100 }]);
});
