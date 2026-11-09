import { useEffect, useMemo, useState } from 'react';
import {
  MAX_CHARTS,
  defaultChartDefinitions,
  duplicateChart,
  loadChartDefinitions,
  saveChartDefinitions,
  createCustomChart,
  isTimeSeriesChart,
  type ChartDefinition,
  type DashboardChartDefinition,
} from '@/lib/chart-config';
import { selectChartData, unitLabel, unitSuffix, zeroBasedUnit } from '@/lib/chart-data';
import type { MetricsSnapshot } from '@/lib/live-metrics';
import { getSignalDefinition, type SignalUnit } from '@/lib/signal-catalog';
import { useCurrentTime } from '@/lib/use-current-time';
import { ChartEditor } from './chart-editor';
import { PoolFeeHistogram } from './pool-fee-histogram';
import { TimeSeriesChart, type ChartSeries } from './time-series-chart';

interface TelemetryDashboardProps {
  metrics: MetricsSnapshot;
}

interface EditorState {
  mode: 'add' | 'edit';
  chart: DashboardChartDefinition;
}

const rangeLabels: Record<ChartDefinition['range'], string> = {
  300: '5m',
  900: '15m',
  3600: '1h',
};

const integerFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 0 });
const numberFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 });

function formatIntegerAxis(value: number): string {
  return integerFormatter.format(value);
}

function formatNumberAxis(value: number): string {
  return numberFormatter.format(value);
}

function formatterForUnit(unit: SignalUnit): (value: number) => string {
  return unit === 'height' || unit === 'occurrences' ? formatIntegerAxis : formatNumberAxis;
}

export function TelemetryDashboard({ metrics }: TelemetryDashboardProps) {
  const now = useCurrentTime();
  const [charts, setCharts] = useState(loadChartDefinitions);
  const [editor, setEditor] = useState<EditorState | null>(null);
  const [resetArmed, setResetArmed] = useState(false);

  useEffect(() => {
    saveChartDefinitions(charts);
  }, [charts]);

  function saveEditedChart(chart: DashboardChartDefinition) {
    setCharts((current) => {
      if (editor?.mode === 'add') {
        return current.length >= MAX_CHARTS ? current : [...current, chart];
      }
      return current.map((item) => (item.id === chart.id ? chart : item));
    });
    setEditor(null);
  }

  function moveChart(index: number, offset: -1 | 1) {
    setCharts((current) => {
      const target = index + offset;
      if (target < 0 || target >= current.length) return current;
      const next = [...current];
      const selected = next[index];
      const adjacent = next[target];
      next[index] = adjacent;
      next[target] = selected;
      return next;
    });
  }

  function resetCharts() {
    if (!resetArmed) {
      setResetArmed(true);
      return;
    }
    setCharts(defaultChartDefinitions());
    setResetArmed(false);
  }

  return (
    <section className="space-y-4">
      <div className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-xs tracking-wide uppercase">Live telemetry</p>
          <h2 className="text-accent mt-1 text-2xl font-semibold">Custom charts</h2>
          <p className="text-textMuted mt-1 text-sm">
            Session data only · {String(charts.length)} of {String(MAX_CHARTS)} charts
          </p>
        </div>
        <div className="flex flex-wrap gap-2">
          <button
            type="button"
            onClick={() => {
              setResetArmed(false);
              setEditor({ mode: 'add', chart: createCustomChart() });
            }}
            disabled={charts.length >= MAX_CHARTS}
            className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-40"
          >
            Add chart
          </button>
          <button
            type="button"
            onClick={resetCharts}
            onBlur={() => {
              setResetArmed(false);
            }}
            className={`rounded-md border px-3 py-2 text-sm font-medium ${
              resetArmed
                ? 'border-amber-400/60 text-amber-200'
                : 'border-accent/40 text-accent hover:border-accent hover:text-accentSecondary'
            }`}
          >
            {resetArmed ? 'Confirm reset' : 'Reset defaults'}
          </button>
        </div>
      </div>

      {charts.length === 0 ? (
        <div className="border-accent/30 bg-surface/80 rounded-2xl border border-dashed px-6 py-16 text-center">
          <p className="text-textPrimary font-medium">No charts in this dashboard.</p>
          <p className="text-textMuted mt-2 text-sm">
            Add a custom chart or start from a template.
          </p>
        </div>
      ) : (
        <div className="grid gap-6 xl:grid-cols-2">
          {charts.map((chart, index) => (
            <TelemetryChartCard
              key={chart.id}
              chart={chart}
              metrics={metrics}
              now={now}
              canMoveEarlier={index > 0}
              canMoveLater={index < charts.length - 1}
              canDuplicate={charts.length < MAX_CHARTS}
              onToggleScale={() => {
                setCharts((current) =>
                  current.map((item) =>
                    item.id === chart.id
                      ? { ...item, scale: item.scale === 'compressed' ? 'linear' : 'compressed' }
                      : item
                  )
                );
              }}
              onDelete={() => {
                setCharts((current) => current.filter((item) => item.id !== chart.id));
              }}
              onDuplicate={() => {
                setCharts((current) =>
                  current.length >= MAX_CHARTS ? current : [...current, duplicateChart(chart)]
                );
              }}
              onEdit={() => {
                setEditor({ mode: 'edit', chart });
              }}
              onMoveEarlier={() => {
                moveChart(index, -1);
              }}
              onMoveLater={() => {
                moveChart(index, 1);
              }}
            />
          ))}
        </div>
      )}

      {editor && (
        <ChartEditor
          key={`${editor.mode}-${editor.chart.id}`}
          chart={editor.chart}
          mode={editor.mode}
          onCancel={() => {
            setEditor(null);
          }}
          onSave={saveEditedChart}
        />
      )}
    </section>
  );
}

function TelemetryChartCard({
  chart,
  metrics,
  now,
  canMoveEarlier,
  canMoveLater,
  canDuplicate,
  onDelete,
  onDuplicate,
  onEdit,
  onMoveEarlier,
  onMoveLater,
  onToggleScale,
}: {
  chart: DashboardChartDefinition;
  metrics: MetricsSnapshot;
  now: number;
  canMoveEarlier: boolean;
  canMoveLater: boolean;
  canDuplicate: boolean;
  onDelete: () => void;
  onDuplicate: () => void;
  onEdit: () => void;
  onMoveEarlier: () => void;
  onMoveLater: () => void;
  onToggleScale: () => void;
}) {
  const [pausedSnapshot, setPausedSnapshot] = useState<MetricsSnapshot | null>(null);
  const [resetVersion, setResetVersion] = useState(0);
  const sourceSnapshot = pausedSnapshot ?? metrics;
  const selected = useMemo(
    () => (isTimeSeriesChart(chart) ? selectChartData(sourceSnapshot, chart) : null),
    [chart, sourceSnapshot]
  );
  const series = useMemo<ChartSeries[]>(
    () =>
      isTimeSeriesChart(chart)
        ? chart.series.map((item) => ({
            label: item.label ?? getSignalDefinition(item.signalId).label,
            color: item.color,
          }))
        : [],
    [chart]
  );
  const valueFormatter = formatterForUnit(selected?.unit ?? 'vbytes');
  const emptyMessage = `Waiting for ${series.length === 1 ? series[0]?.label?.toLowerCase() : 'the selected signals'}…`;

  return (
    <article className="border-accent/30 bg-surface/80 min-w-0 overflow-hidden rounded-2xl border">
      <header className="border-accentSecondary/20 space-y-3 border-b px-4 py-4 sm:px-5">
        <div className="flex flex-wrap items-start justify-between gap-3">
          <div className="min-w-0">
            <h3 className="text-accent truncate text-lg font-semibold" title={chart.title}>
              {chart.title}
            </h3>
            <p className="text-textMuted mt-1 text-xs">
              {isTimeSeriesChart(chart) && selected
                ? `${unitLabel(selected.unit)} · ${rangeLabels[chart.range]} · ${chart.visualization}`
                : 'Virtual size by fee-rate group · latest snapshot'}
              {chart.scale === 'compressed' ? ' · compressed scale' : ''}
              {pausedSnapshot ? ' · paused' : ''}
            </p>
          </div>
          <div className="flex flex-wrap justify-end gap-1 text-xs">
            <button
              type="button"
              aria-label={`Compress peaks for ${chart.title}`}
              aria-pressed={chart.scale === 'compressed'}
              title="Use a log-like Y axis to compress large peaks. Zero, negative values, and original measurements are preserved."
              onClick={onToggleScale}
              className={`rounded-md border px-2 py-1.5 ${
                chart.scale === 'compressed'
                  ? 'border-accent/40 bg-accentSecondary/20 text-accent'
                  : 'border-accent/30 text-textMuted hover:text-accent hover:bg-accentSecondary/10'
              }`}
            >
              Compress peaks
            </button>
            <ChartAction
              label={pausedSnapshot ? 'Resume chart' : 'Pause chart'}
              onClick={() => {
                setPausedSnapshot((current) =>
                  current ? null : isTimeSeriesChart(chart) ? metrics : { ...metrics, now }
                );
              }}
            >
              {pausedSnapshot ? 'Resume' : 'Pause'}
            </ChartAction>
            {isTimeSeriesChart(chart) && (
              <ChartAction
                label="Reset chart zoom"
                onClick={() => {
                  setResetVersion((value) => value + 1);
                }}
              >
                Reset zoom
              </ChartAction>
            )}
            <ChartAction label="Edit chart" onClick={onEdit}>
              Edit
            </ChartAction>
            <ChartAction label="Duplicate chart" onClick={onDuplicate} disabled={!canDuplicate}>
              Duplicate
            </ChartAction>
            <ChartAction
              label={`Move ${chart.title} earlier`}
              onClick={onMoveEarlier}
              disabled={!canMoveEarlier}
            >
              ←
            </ChartAction>
            <ChartAction
              label={`Move ${chart.title} later`}
              onClick={onMoveLater}
              disabled={!canMoveLater}
            >
              →
            </ChartAction>
            <ChartAction label="Delete chart" onClick={onDelete} danger>
              Delete
            </ChartAction>
          </div>
        </div>
      </header>
      <div className="min-w-0 p-3 sm:p-5">
        {isTimeSeriesChart(chart) && selected ? (
          <TimeSeriesChart
            key={resetVersion}
            data={selected.data}
            series={series}
            visualization={chart.visualization}
            scale={chart.scale}
            zeroBased={zeroBasedUnit(selected.unit)}
            valueFormatter={valueFormatter}
            valueSuffix={unitSuffix(selected.unit)}
            forceEmpty={!selected.hasObservedData}
            emptyMessage={emptyMessage}
          />
        ) : (
          !isTimeSeriesChart(chart) && (
            <PoolFeeHistogram
              snapshot={sourceSnapshot.poolSnapshot}
              now={pausedSnapshot?.now ?? now}
              color={chart.color}
              scale={chart.scale}
            />
          )
        )}
      </div>
      {chart.scale === 'compressed' && (
        <p className="text-textMuted px-5 pb-3 text-[11px]">
          Log-like Y axis · peaks compressed · labels and hover values show original measurements.
        </p>
      )}
      {isTimeSeriesChart(chart) && chart.series.some((item) => item.signalId === 'event:10') && (
        <p className="text-textMuted px-5 pb-3 text-[11px]">
          Archival includes compact-block recovery; rejected and duplicate submissions are excluded.
        </p>
      )}
      {selected && selected.unobservedLabels.length > 0 && selected.hasObservedData && (
        <footer className="border-accent/10 text-textMuted border-t px-5 py-2 text-[11px]">
          Not observed this session: {selected.unobservedLabels.join(', ')}
        </footer>
      )}
    </article>
  );
}

function ChartAction({
  label,
  children,
  onClick,
  disabled = false,
  danger = false,
}: {
  label: string;
  children: React.ReactNode;
  onClick: () => void;
  disabled?: boolean;
  danger?: boolean;
}) {
  return (
    <button
      type="button"
      aria-label={label}
      onClick={onClick}
      disabled={disabled}
      className={`rounded-md px-2 py-1.5 disabled:cursor-not-allowed disabled:opacity-30 ${
        danger
          ? 'text-textMuted hover:bg-red-500/10 hover:text-red-300'
          : 'text-textMuted hover:text-accent hover:bg-accentSecondary/10'
      }`}
    >
      {children}
    </button>
  );
}
