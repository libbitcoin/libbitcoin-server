import type { ReactNode } from 'react';

interface MetricItemProps {
  label: string;
  value: ReactNode;
  detail: ReactNode;
  numeric?: boolean;
  emphasized?: boolean;
}

export function MetricItem({
  label,
  value,
  detail,
  numeric = true,
  emphasized = false,
}: MetricItemProps) {
  return (
    <div className="border-accent/30 bg-surface/80 min-w-0 space-y-2 rounded-2xl border p-5">
      <dt className="text-accentSecondary text-xs tracking-wide uppercase">{label}</dt>
      <dd
        className={`text-2xl font-semibold break-words ${numeric ? 'tabular-nums' : ''} ${emphasized ? 'text-accent' : 'text-textPrimary'}`}
      >
        {value}
      </dd>
      <dd className="text-textMuted text-sm break-words">{detail}</dd>
    </div>
  );
}
