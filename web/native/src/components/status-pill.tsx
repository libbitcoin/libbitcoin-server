interface StatusPillProps {
  status: 'connected' | 'degraded' | 'offline' | 'unknown';
  label: string;
  title?: string;
  labelClassName?: string;
}

const STATUS_STYLES: Record<StatusPillProps['status'], string> = {
  connected: 'bg-emerald-500/20 text-emerald-300 border-emerald-500/50',
  degraded: 'bg-amber-500/20 text-amber-300 border-amber-500/50',
  offline: 'bg-rose-500/20 text-rose-300 border-rose-500/50',
  unknown: 'bg-slate-500/20 text-slate-300 border-slate-500/50',
};

const STATUS_DOT: Record<StatusPillProps['status'], string> = {
  connected: 'bg-emerald-400',
  degraded: 'bg-amber-400',
  offline: 'bg-rose-400',
  unknown: 'bg-slate-400',
};

export function StatusPill({ status, label, title, labelClassName }: StatusPillProps) {
  return (
    <span
      tabIndex={0}
      title={title}
      className={`group flex min-w-0 items-center justify-center gap-0 overflow-hidden rounded-full border px-2 py-1 text-xs font-medium tracking-wide uppercase outline-none select-none focus-within:justify-start focus-within:gap-2 focus-within:px-3 hover:justify-start hover:gap-2 hover:px-3 motion-safe:transition-[padding,gap] motion-safe:duration-200 motion-safe:ease-out ${STATUS_STYLES[status]}`}
    >
      <span className={`h-2 w-2 rounded-full ${STATUS_DOT[status]}`} aria-hidden="true" />
      <span
        className={`max-w-0 min-w-0 truncate opacity-0 group-focus-within:max-w-[18rem] group-focus-within:translate-x-0 group-focus-within:opacity-100 group-hover:max-w-[18rem] group-hover:translate-x-0 group-hover:opacity-100 motion-safe:-translate-x-1 motion-safe:transition-[max-width,opacity,transform] motion-safe:duration-200 motion-safe:ease-out ${labelClassName ?? ''}`}
      >
        {label}
      </span>
    </span>
  );
}
