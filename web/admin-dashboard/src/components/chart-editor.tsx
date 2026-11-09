import { useEffect, useMemo, useState, type SyntheticEvent } from 'react';
import {
  CHART_COLORS,
  CHART_RANGES,
  CHART_TEMPLATES,
  CHART_TYPES,
  MAX_CHART_SERIES,
  MAX_CHART_TITLE_LENGTH,
  chartAxisUnit,
  chartValidationError,
  createChartFromTemplate,
  createCustomChart,
  isTimeSeriesChart,
  type ChartDefinition,
  type DashboardChartDefinition,
  type ChartSeriesDefinition,
} from '@/lib/chart-config';
import { unitLabel } from '@/lib/chart-data';
import {
  SIGNAL_DEFINITIONS,
  getMeasureDefinition,
  getSignalDefinition,
  type ChartType,
  type SignalMeasure,
} from '@/lib/signal-catalog';

interface ChartEditorProps {
  chart: DashboardChartDefinition;
  mode: 'add' | 'edit';
  onCancel: () => void;
  onSave: (chart: DashboardChartDefinition) => void;
}

const rangeLabels: Record<(typeof CHART_RANGES)[number], string> = {
  300: '5 minutes',
  900: '15 minutes',
  3600: '1 hour',
};

const typeLabels: Record<ChartType, string> = {
  line: 'Line',
  step: 'Step',
  area: 'Area',
  bar: 'Bar',
};

function cloneChart(chart: DashboardChartDefinition): DashboardChartDefinition {
  if (!isTimeSeriesChart(chart)) return { ...chart };
  return { ...chart, series: chart.series.map((item) => ({ ...item })) };
}

function groupedSignals() {
  const groups = new Map<string, (typeof SIGNAL_DEFINITIONS)[number][]>();
  for (const signal of SIGNAL_DEFINITIONS) {
    const group = groups.get(signal.category) ?? [];
    group.push(signal);
    groups.set(signal.category, group);
  }
  return [...groups];
}

const signalGroups = groupedSignals();

export function ChartEditor({ chart, mode, onCancel, onSave }: ChartEditorProps) {
  const [draft, setDraft] = useState(() => cloneChart(chart));
  const validationError = chartValidationError(draft);
  const axisUnit = chartAxisUnit(draft);

  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === 'Escape') onCancel();
    }
    globalThis.addEventListener('keydown', handleKeyDown);
    return () => {
      globalThis.removeEventListener('keydown', handleKeyDown);
    };
  }, [onCancel]);

  const compatibleChartTypes = useMemo(
    () =>
      new Set(
        CHART_TYPES.filter(
          (chartType) =>
            isTimeSeriesChart(draft) &&
            draft.series.every((item) =>
              getMeasureDefinition(
                getSignalDefinition(item.signalId),
                item.measure
              ).chartTypes.includes(chartType)
            )
        )
      ),
    [draft]
  );

  function updateTimeSeries(update: (current: ChartDefinition) => ChartDefinition) {
    setDraft((current) => (isTimeSeriesChart(current) ? update(current) : current));
  }

  function replaceSeries(index: number, series: ChartSeriesDefinition) {
    updateTimeSeries((current) => ({
      ...current,
      series: current.series.map((item, itemIndex) => (itemIndex === index ? series : item)),
    }));
  }

  function selectSignal(index: number, signalId: string) {
    if (!isTimeSeriesChart(draft)) return;
    const current = draft.series[index];
    const signal = getSignalDefinition(signalId);
    replaceSeries(index, { ...current, signalId, measure: signal.defaultMeasure });
  }

  function addSeries() {
    if (!isTimeSeriesChart(draft)) return;
    if (draft.series.length >= MAX_CHART_SERIES) return;
    const currentUnit = chartAxisUnit(draft);
    const signal = SIGNAL_DEFINITIONS.find((candidate) =>
      candidate.allowedMeasures.some(
        (measure) =>
          currentUnit === null || getMeasureDefinition(candidate, measure).unit === currentUnit
      )
    );
    if (!signal) return;
    const measure =
      signal.allowedMeasures.find(
        (item) => currentUnit === null || getMeasureDefinition(signal, item).unit === currentUnit
      ) ?? signal.defaultMeasure;
    const next: ChartSeriesDefinition = {
      id: `series-${Date.now().toString(36)}-${String(draft.series.length + 1)}`,
      signalId: signal.id,
      measure,
      color: CHART_COLORS[draft.series.length % CHART_COLORS.length] ?? CHART_COLORS[0],
    };
    updateTimeSeries((current) => ({ ...current, series: [...current.series, next] }));
  }

  function submit(event: SyntheticEvent<HTMLFormElement>) {
    event.preventDefault();
    if (validationError) return;
    onSave({ ...draft, title: draft.title.trim() });
  }

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4 backdrop-blur-sm"
      role="presentation"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) onCancel();
      }}
    >
      <section
        role="dialog"
        aria-modal="true"
        aria-labelledby="chart-editor-title"
        className="border-accent/30 bg-surface/95 max-h-[92vh] w-full max-w-4xl overflow-y-auto rounded-2xl border shadow-2xl shadow-black/60"
      >
        <header className="border-accent/20 flex items-start justify-between border-b p-5">
          <div>
            <p className="text-accentSecondary text-xs tracking-wide uppercase">
              Dashboard builder
            </p>
            <h2 id="chart-editor-title" className="text-accent mt-1 text-xl font-semibold">
              {mode === 'add' ? 'Add chart' : 'Edit chart'}
            </h2>
          </div>
          <button
            type="button"
            onClick={onCancel}
            className="text-textMuted hover:text-accent rounded p-1 text-xl"
            aria-label="Close chart editor"
          >
            ×
          </button>
        </header>

        {mode === 'add' && (
          <div className="border-accentSecondary/20 border-b p-5">
            <p className="text-accentSecondary mb-3 text-xs tracking-wide uppercase">
              Start from a template
            </p>
            <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-5">
              {CHART_TEMPLATES.map((template) => (
                <button
                  key={template.id}
                  type="button"
                  onClick={() => {
                    setDraft(createChartFromTemplate(template.id));
                  }}
                  className="border-accent/30 bg-background hover:border-accent rounded-md border p-3 text-left"
                >
                  <span className="text-textPrimary block text-sm font-medium">
                    {template.label}
                  </span>
                  <span className="text-textMuted mt-1 block text-xs leading-4">
                    {template.description}
                  </span>
                </button>
              ))}
              <button
                type="button"
                onClick={() => {
                  setDraft(createCustomChart());
                }}
                className="border-accent/30 bg-background hover:border-accent rounded-md border p-3 text-left"
              >
                <span className="text-textPrimary block text-sm font-medium">Custom</span>
                <span className="text-textMuted mt-1 block text-xs leading-4">
                  Begin with one editable series.
                </span>
              </button>
            </div>
          </div>
        )}

        <form onSubmit={submit} className="space-y-6 p-5">
          <div className="grid gap-4 md:grid-cols-[minmax(0,1fr)_11rem_11rem]">
            <label className="space-y-2">
              <span className="text-sm font-medium">Chart title</span>
              <input
                value={draft.title}
                maxLength={MAX_CHART_TITLE_LENGTH}
                onChange={(event) => {
                  setDraft((current) => ({ ...current, title: event.target.value }));
                }}
                className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-3 py-2 text-sm focus:ring-0"
              />
            </label>
            {isTimeSeriesChart(draft) ? (
              <>
                <label className="space-y-2">
                  <span className="text-sm font-medium">Chart type</span>
                  <select
                    value={draft.visualization}
                    onChange={(event) => {
                      updateTimeSeries((current) => ({
                        ...current,
                        visualization: event.target.value as ChartType,
                      }));
                    }}
                    className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-3 py-2 text-sm focus:ring-0"
                  >
                    {CHART_TYPES.map((chartType) => (
                      <option
                        key={chartType}
                        value={chartType}
                        disabled={!compatibleChartTypes.has(chartType)}
                      >
                        {typeLabels[chartType]}
                      </option>
                    ))}
                  </select>
                </label>
                <label className="space-y-2">
                  <span className="text-sm font-medium">Time range</span>
                  <select
                    value={draft.range}
                    onChange={(event) => {
                      updateTimeSeries((current) => ({
                        ...current,
                        range: Number(event.target.value) as ChartDefinition['range'],
                      }));
                    }}
                    className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-3 py-2 text-sm focus:ring-0"
                  >
                    {CHART_RANGES.map((range) => (
                      <option key={range} value={range}>
                        {rangeLabels[range]}
                      </option>
                    ))}
                  </select>
                </label>
              </>
            ) : (
              <label className="space-y-2">
                <span className="text-sm font-medium">Bar color</span>
                <input
                  type="color"
                  value={draft.color}
                  onChange={(event) => {
                    const color = event.target.value;
                    setDraft((current) =>
                      isTimeSeriesChart(current) ? current : { ...current, color }
                    );
                  }}
                  className="border-accent/40 bg-background h-10 w-full rounded-md border p-1"
                />
              </label>
            )}
          </div>

          {isTimeSeriesChart(draft) ? (
            <section className="space-y-3">
              <div className="flex flex-wrap items-center justify-between gap-3">
                <div>
                  <h3 className="text-accent text-lg font-semibold">Series</h3>
                  <p className="text-textMuted mt-1 text-xs">
                    One chart uses one axis unit
                    {axisUnit === null ? '.' : `: ${unitLabel(axisUnit)}.`}
                  </p>
                </div>
                <button
                  type="button"
                  onClick={addSeries}
                  disabled={draft.series.length >= MAX_CHART_SERIES}
                  className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary disabled:text-textMuted rounded-md border px-3 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-50"
                >
                  Add series
                </button>
              </div>

              <div className="space-y-3">
                {draft.series.map((item, index) => {
                  const signal = getSignalDefinition(item.signalId);
                  const measure = getMeasureDefinition(signal, item.measure);
                  return (
                    <article
                      key={item.id}
                      className="border-accentSecondary/20 bg-accent/5 grid gap-3 rounded-xl border p-4 lg:grid-cols-[minmax(12rem,1.2fr)_minmax(11rem,1fr)_minmax(9rem,0.8fr)_3.5rem_auto]"
                    >
                      <label className="space-y-1.5">
                        <span className="text-textMuted text-xs">Signal {String(index + 1)}</span>
                        <select
                          aria-label={`Signal ${String(index + 1)}`}
                          value={item.signalId}
                          onChange={(event) => {
                            selectSignal(index, event.target.value);
                          }}
                          className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-2 py-2 text-sm focus:ring-0"
                        >
                          {signalGroups.map(([category, signals]) => (
                            <optgroup key={category} label={category}>
                              {signals.map((option) => (
                                <option key={option.id} value={option.id}>
                                  {option.label}
                                </option>
                              ))}
                            </optgroup>
                          ))}
                        </select>
                      </label>
                      <label className="space-y-1.5">
                        <span className="text-textMuted text-xs">Measurement</span>
                        <select
                          aria-label={`Measurement ${String(index + 1)}`}
                          value={item.measure}
                          onChange={(event) => {
                            replaceSeries(index, {
                              ...item,
                              measure: event.target.value as SignalMeasure,
                            });
                          }}
                          className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-2 py-2 text-sm focus:ring-0"
                          title={measure.description}
                        >
                          {signal.allowedMeasures.map((measureId) => (
                            <option key={measureId} value={measureId}>
                              {getMeasureDefinition(signal, measureId).label}
                            </option>
                          ))}
                        </select>
                      </label>
                      <label className="space-y-1.5">
                        <span className="text-textMuted text-xs">Display label</span>
                        <input
                          aria-label={`Display label ${String(index + 1)}`}
                          value={item.label ?? ''}
                          onChange={(event) => {
                            replaceSeries(index, {
                              ...item,
                              label: event.target.value || undefined,
                            });
                          }}
                          placeholder={signal.label}
                          className="border-accent/40 bg-background text-textPrimary placeholder:text-textMuted/50 focus:border-accent w-full rounded-md border px-2 py-2 text-sm focus:ring-0"
                        />
                      </label>
                      <label className="space-y-1.5">
                        <span className="text-textMuted text-xs">Color</span>
                        <input
                          aria-label={`Series color ${String(index + 1)}`}
                          type="color"
                          value={item.color}
                          onChange={(event) => {
                            replaceSeries(index, { ...item, color: event.target.value });
                          }}
                          className="border-accent/40 bg-background h-10 w-full rounded-md border p-1"
                        />
                      </label>
                      <button
                        type="button"
                        onClick={() => {
                          updateTimeSeries((current) => ({
                            ...current,
                            series: current.series.filter(
                              (_series, itemIndex) => itemIndex !== index
                            ),
                          }));
                        }}
                        disabled={draft.series.length === 1}
                        aria-label={`Remove series ${String(index + 1)}`}
                        className="text-textMuted self-end rounded-md px-3 py-2 text-sm hover:text-red-300 disabled:cursor-not-allowed disabled:opacity-30"
                      >
                        Remove
                      </button>
                    </article>
                  );
                })}
              </div>
            </section>
          ) : (
            <p className="text-textMuted text-sm">
              Shows the latest transaction-pool fee distribution, refreshed every five seconds. Each
              bar shows the virtual size of one fee-rate group, labeled by its minimum fee rate in
              sat/vB.
            </p>
          )}

          {validationError && (
            <p className="rounded-md border border-amber-500/30 bg-amber-500/10 px-3 py-2 text-sm text-amber-200">
              {validationError}
            </p>
          )}

          <div className="border-accentSecondary/20 flex justify-end gap-2 border-t pt-4">
            <button
              type="button"
              onClick={onCancel}
              className="text-textMuted hover:text-accent rounded-md px-4 py-2 text-sm"
            >
              Cancel
            </button>
            <button
              type="submit"
              disabled={validationError !== null}
              className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-40"
            >
              {mode === 'add' ? 'Add chart' : 'Save changes'}
            </button>
          </div>
        </form>
      </section>
    </div>
  );
}
