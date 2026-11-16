import { CHART_COLORS } from './chart-config';

// Adapt the built-in palette for light backgrounds without rewriting saved colors.
export function chartColor(color: string, styles: CSSStyleDeclaration): string {
  const index = (CHART_COLORS as readonly string[]).indexOf(color.toLowerCase());
  return index === -1
    ? color
    : styles.getPropertyValue(`--chart-color-${String(index)}`).trim() || color;
}
