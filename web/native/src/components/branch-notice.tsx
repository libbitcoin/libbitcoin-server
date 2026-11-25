import type { ReactNode } from 'react';
import { Link } from 'react-router-dom';

export function WeakBranchBadge() {
  return (
    <span className="border-accentSecondary/50 text-accentSecondary rounded border px-1.5 py-0.5 text-[10px] font-medium tracking-wide whitespace-nowrap uppercase">
      Weak branch
    </span>
  );
}

// A block or template not of the strong chain, with the strong block at its
// height. It remains valid and viewable.
export function WeakBranchNotice({
  children,
  to,
  label,
}: {
  children: ReactNode;
  to: string;
  label: string;
}) {
  return (
    <div className="border-accentSecondary/40 bg-accentSecondary/10 text-textPrimary flex flex-wrap items-center justify-between gap-3 rounded-xl border px-4 py-3 text-sm">
      <p className="flex flex-wrap items-center gap-2">
        <WeakBranchBadge />
        <span className="text-textMuted">{children}</span>
      </p>
      <Link to={to} className="text-accent hover:text-accentSecondary font-medium">
        {label} →
      </Link>
    </div>
  );
}
