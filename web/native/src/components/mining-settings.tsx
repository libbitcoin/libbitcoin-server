import { useEffect, useRef, useState, type FormEvent, type ReactNode } from 'react';
import type { MinerSettings } from '@/lib/mining/miner';

const INPUT =
  'border-accent/40 bg-surface text-textPrimary placeholder:text-textMuted focus:border-accent focus:ring-accent w-full rounded-md border px-3 py-1.5 text-sm';

// The miner settings, edited in a modal dialog and applied on save.
export function MiningSettings({
  open,
  settings,
  cores,
  onSave,
  onClose,
}: {
  open: boolean;
  settings: MinerSettings;
  cores: number;
  onSave: (settings: MinerSettings) => void;
  onClose: () => void;
}) {
  const dialog = useRef<HTMLDialogElement>(null);
  const [draft, setDraft] = useState(settings);

  useEffect(() => {
    const element = dialog.current;
    if (!element) return;
    if (open && !element.open) {
      setDraft(settings);
      element.showModal();
    } else if (!open && element.open) element.close();
  }, [open, settings]);

  const change = <Key extends keyof MinerSettings>(key: Key, value: MinerSettings[Key]) => {
    setDraft((current) => ({ ...current, [key]: value }));
  };

  const onSubmit = (event: FormEvent) => {
    event.preventDefault();
    onSave({ ...draft, url: draft.url.trim(), worker: draft.worker.trim() });
  };

  return (
    <dialog
      ref={dialog}
      aria-labelledby="mining-settings-title"
      onClose={onClose}
      onCancel={onClose}
      className="border-accent/40 bg-surface text-textPrimary m-auto w-[min(32rem,calc(100vw-2rem))] rounded-2xl border p-0 shadow-2xl backdrop:bg-black/60"
    >
      <form onSubmit={onSubmit} className="space-y-4 p-6">
        <h2 id="mining-settings-title" className="text-accent text-lg font-semibold">
          Miner settings
        </h2>
        <Field label="Stratum service">
          <input
            className={INPUT}
            value={draft.url}
            spellCheck={false}
            placeholder="ws://host:3333"
            onChange={(event) => {
              change('url', event.target.value);
            }}
          />
        </Field>
        <div className="grid gap-4 sm:grid-cols-[minmax(0,1fr)_7rem]">
          <Field label="Worker">
            <input
              className={INPUT}
              value={draft.worker}
              spellCheck={false}
              onChange={(event) => {
                change('worker', event.target.value);
              }}
            />
          </Field>
          <Field label="Threads">
            <input
              className={INPUT}
              type="number"
              min={1}
              max={cores}
              value={draft.threads}
              onChange={(event) => {
                change('threads', Math.min(cores, Math.max(1, Math.floor(Number(event.target.value) || 1))));
              }}
            />
          </Field>
        </div>
        <div className="flex justify-end gap-3 pt-2">
          <button
            type="button"
            onClick={onClose}
            className="border-accent/40 text-textPrimary hover:border-accent hover:text-accent rounded-md border px-4 py-1.5 text-sm"
          >
            Cancel
          </button>
          <button
            type="submit"
            disabled={!draft.url.trim() || !draft.worker.trim()}
            className="border-accent bg-accent/20 text-accent hover:bg-accent/30 rounded-md border px-4 py-1.5 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-50"
          >
            Save
          </button>
        </div>
      </form>
    </dialog>
  );
}

function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="block space-y-1">
      <span className="text-accentSecondary text-xs tracking-wide uppercase">{label}</span>
      {children}
    </label>
  );
}
