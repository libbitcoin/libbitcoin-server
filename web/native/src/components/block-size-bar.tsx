import { formatNumber } from '@/lib/utils/format';

const MAX_BLOCK_BYTES = 4_000_000;
const ONE_MB = 1_000_000;

interface BlockSizeBarProps {
  nominal?: number;
  size?: number;
  weight?: number;
}

interface BlockSizeBreakdown {
  hasSplit: boolean;
  nominalBytes?: number;
  totalBytes: number;
  witnessBytes?: number;
}

export function BlockSizeBar({ nominal, size, weight }: BlockSizeBarProps) {
  const breakdown = getBlockSizeBreakdown({ nominal, size, weight });
  if (!breakdown) return null;

  const markers = Array.from({ length: 5 }, (_, index) => index * ONE_MB);
  const nominalWidthPct = getWidthPct(breakdown.nominalBytes ?? breakdown.totalBytes);
  const witnessWidthPct = getWidthPct(breakdown.witnessBytes ?? 0);
  const usagePct = (clamp(breakdown.totalBytes, 0, MAX_BLOCK_BYTES) / MAX_BLOCK_BYTES) * 100;

  return (
    <section className="border-accent/30 bg-surface/80 space-y-5 rounded-xl border p-6">
      <div className="flex items-center justify-between gap-3 text-sm">
        <span className="text-accentSecondary text-xs tracking-wide uppercase">Block Size</span>
        <span className="text-textPrimary">
          {formatMegabytes(breakdown.totalBytes)} / 4.00 MB
        </span>
      </div>

      <div className="relative">
        <div className="border-accentSecondary/20 from-accent/8 to-surface/60 relative h-8 overflow-hidden rounded-full border bg-gradient-to-r">
          {markers.slice(1).map((marker) => (
            <div
              key={marker}
              aria-hidden="true"
              className="border-textMuted/30 absolute inset-y-0 border-r"
              style={{ left: `${String((marker / MAX_BLOCK_BYTES) * 100)}%` }}
            />
          ))}
          <div
            aria-label="Nominal block bytes"
            className="bg-accent absolute inset-y-0 left-0"
            style={{ width: `${String(nominalWidthPct)}%` }}
          />
          {breakdown.hasSplit && (
            <div
              aria-label="Witness block bytes"
              className="bg-accentSecondary absolute inset-y-0"
              style={{
                left: `${String(nominalWidthPct)}%`,
                width: `${String(witnessWidthPct)}%`,
              }}
            />
          )}
          <div className="pointer-events-none absolute inset-0 bg-[linear-gradient(180deg,rgba(255,255,255,0.18),rgba(255,255,255,0))]" />
        </div>
        <div className="mt-2 grid grid-cols-5 text-[11px] text-textMuted">
          {markers.map((marker) => (
            <span key={marker} className="text-center first:text-left last:text-right">
              {marker === 0 ? '0' : formatMegabytes(marker)}
            </span>
          ))}
        </div>
      </div>

      <div className="grid gap-3 text-xs sm:grid-cols-3">
        <MetricCard
          colorClass="bg-accent"
          label={breakdown.hasSplit ? 'Nominal' : 'Used'}
          primaryValue={formatBytes(breakdown.nominalBytes ?? breakdown.totalBytes)}
          detail={formatMegabytes(breakdown.nominalBytes ?? breakdown.totalBytes)}
        />
        <MetricCard
          colorClass="bg-accentSecondary"
          label="Witness"
          primaryValue={
            breakdown.hasSplit && typeof breakdown.witnessBytes === 'number'
              ? formatBytes(breakdown.witnessBytes)
              : 'Unavailable'
          }
          detail={
            breakdown.hasSplit && typeof breakdown.witnessBytes === 'number'
              ? formatMegabytes(breakdown.witnessBytes)
              : 'Witness split unavailable'
          }
        />
        <MetricCard
          colorClass="bg-textPrimary"
          label="Total"
          primaryValue={formatBytes(breakdown.totalBytes)}
          detail={`${formatNumber(usagePct, { maximumFractionDigits: 1 })}% of 4.00 MB max`}
        />
      </div>
    </section>
  );
}

function MetricCard({
  colorClass,
  detail,
  label,
  primaryValue,
}: {
  colorClass: string;
  detail: string;
  label: string;
  primaryValue: string;
}) {
  return (
    <div className="border-accentSecondary/20 bg-accent/5 space-y-1 rounded-lg border p-3">
      <p className="text-accentSecondary tracking-wide uppercase">
        <span className={`${colorClass} mr-2 inline-block h-2 w-2 rounded-sm`} />
        {label}
      </p>
      <p className="text-textPrimary text-sm font-medium">{primaryValue}</p>
      <p className="text-textMuted">{detail}</p>
    </div>
  );
}

function getBlockSizeBreakdown({
  nominal,
  size,
  weight,
}: BlockSizeBarProps): BlockSizeBreakdown | null {
  const totalBytes =
    typeof size === 'number'
      ? Math.max(0, size)
      : typeof nominal === 'number' && typeof weight === 'number'
        ? Math.max(0, nominal) + Math.max(0, weight - Math.max(0, nominal) * 4)
        : undefined;

  if (typeof totalBytes !== 'number') return null;

  const explicitNominal =
    typeof nominal === 'number' ? clamp(Math.max(0, nominal), 0, totalBytes) : undefined;

  if (typeof explicitNominal === 'number') {
    return {
      hasSplit: true,
      nominalBytes: explicitNominal,
      totalBytes,
      witnessBytes: Math.max(0, totalBytes - explicitNominal),
    };
  }

  if (typeof size === 'number' && typeof weight === 'number') {
    // Segwit weight math: total_size = non_witness + witness, weight = 4*non_witness + witness.
    const derivedNominal = clamp(Math.round((weight - size) / 3), 0, size);
    return {
      hasSplit: true,
      nominalBytes: derivedNominal,
      totalBytes,
      witnessBytes: Math.max(0, totalBytes - derivedNominal),
    };
  }

  return {
    hasSplit: false,
    nominalBytes: totalBytes,
    totalBytes,
  };
}

function clamp(value: number, min: number, max: number) {
  return Math.min(Math.max(value, min), max);
}

function getWidthPct(bytes: number) {
  return (clamp(bytes, 0, MAX_BLOCK_BYTES) / MAX_BLOCK_BYTES) * 100;
}

function formatMegabytes(value: number) {
  return `${formatNumber(value / ONE_MB, { minimumFractionDigits: 2, maximumFractionDigits: 2 })} MB`;
}

function formatBytes(value: number) {
  return `${formatNumber(value, { maximumFractionDigits: 0 })} bytes`;
}
