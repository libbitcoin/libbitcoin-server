import type { ConnectionPhase } from '@/lib/reconnecting-websocket';

interface ConnectionPillProps {
  phase: ConnectionPhase;
  label: string;
}

const phaseStyles: Record<ConnectionPhase, string> = {
  idle: 'border-slate-500/50 bg-slate-500/20 text-slate-300',
  connecting: 'border-amber-500/50 bg-amber-500/20 text-amber-300',
  connected: 'border-emerald-500/50 bg-emerald-500/20 text-emerald-300',
  reconnecting: 'border-amber-500/50 bg-amber-500/20 text-amber-300',
};

const phaseDots: Record<ConnectionPhase, string> = {
  idle: 'bg-slate-400',
  connecting: 'bg-amber-400',
  connected: 'bg-emerald-400',
  reconnecting: 'bg-amber-400',
};

export function ConnectionPill({ phase, label }: ConnectionPillProps) {
  return (
    <span
      className={`inline-flex items-center gap-2 rounded-full border px-2 py-1 text-xs font-medium tracking-wide uppercase ${phaseStyles[phase]}`}
    >
      <span className={`h-2 w-2 rounded-full ${phaseDots[phase]}`} aria-hidden="true" />
      {label}
    </span>
  );
}
