import type uPlot from 'uplot';

// Select readable ticks in the original units, including fractions near zero.
// The built-in asinh ticks omit fractions inside its linear threshold.
export const compressedAxisSplits: uPlot.Axis.Splits = (
  chart,
  _axis,
  minimum,
  maximum,
  increment
) => {
  if (!Number.isFinite(minimum) || !Number.isFinite(maximum) || maximum <= minimum) return [];

  const candidates: Array<{ value: number; priority: number }> = [];
  if (Math.asinh(maximum) - Math.asinh(minimum) < 1.5) {
    // For small values or a narrow interval of large heights, use regular ticks.
    const step = Number.isFinite(increment) && increment > 0 ? increment : (maximum - minimum) / 4;
    const start = Math.ceil(minimum / step) * step;
    for (let index = 0; index < 100; index++) {
      const value = Number((start + index * step).toPrecision(14));
      if (value > maximum) break;
      candidates.push({ value, priority: value === 0 ? 0 : 1 });
    }
  } else {
    const largestExponent = Math.ceil(Math.log10(Math.max(Math.abs(minimum), Math.abs(maximum))));
    for (
      let exponent = Math.min(-1, largestExponent - 1);
      exponent <= largestExponent;
      exponent++
    ) {
      for (const multiple of [1, 2, 5]) {
        const value = multiple * 10 ** exponent;
        for (const signed of [-value, value]) {
          if (Number.isFinite(signed) && signed >= minimum && signed <= maximum)
            candidates.push({ value: signed, priority: multiple === 1 ? 1 : 2 });
        }
      }
    }
    if (minimum <= 0 && maximum >= 0) candidates.push({ value: 0, priority: 0 });
  }

  // Prefer zero and powers of ten, filling intermediate ticks only if they fit.
  candidates.sort((left, right) => left.priority - right.priority || left.value - right.value);
  const selected: number[] = [];
  const positions: number[] = [];
  for (const { value } of candidates) {
    const position = chart.valToPos(value, 'y');
    if (Number.isFinite(position) && positions.every((other) => Math.abs(position - other) >= 36)) {
      selected.push(value);
      positions.push(position);
    }
  }
  selected.sort((left, right) => left - right);
  return selected;
};
