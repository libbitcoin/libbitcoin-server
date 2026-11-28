import { useEffect, useRef, useState, type FormEvent, type ReactNode } from 'react';
import type { NetworkParams } from '@/lib/api/client';
import { isAddress } from '@/lib/api/providers/libbitcoin-mapper';
import type { MinerSettings } from '@/lib/mining/miner';
import { closeOnBackdrop } from '@/lib/utils/dialog';

/** Miner settings of the page, with the payout fields the node may supply. */
export interface PageSettings extends MinerSettings {
  address: string;
  tag: string;
}

/** Page settings required as the node does not advertise them. */
export interface RequiredSettings {
  url: boolean;
  address: boolean;
  tag: boolean;
}

const INPUT =
  'border-accent/40 bg-surface text-textPrimary placeholder:text-textMuted focus:border-accent focus:ring-accent w-full rounded-md border px-3 py-1.5 text-sm';

// The miner settings, edited in a modal dialog and applied on save.
export function MiningSettings({
  open,
  settings,
  require,
  wallet,
  network,
  maximum,
  protocols,
  onSave,
  onClose,
}: {
  open: boolean;
  settings: PageSettings;
  require: RequiredSettings;
  protocols?: boolean;
  wallet?: NetworkParams;
  network?: string;
  maximum: number;
  onSave: (settings: PageSettings) => void;
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

  const change = <Key extends keyof PageSettings>(key: Key, value: PageSettings[Key]) => {
    setDraft((current) => ({ ...current, [key]: value }));
  };

  const wrongAddress =
    require.address && wallet !== undefined && draft.address.trim() !== '' && !isAddress(draft.address, wallet);

  const missing =
    wrongAddress ||
    (require.url && !draft.url.trim()) ||
    (require.address && !draft.address.trim()) ||
    (require.tag && !draft.tag.trim()) ||
    (!require.address && !draft.worker.trim());

  const onSubmit = (event: FormEvent) => {
    event.preventDefault();
    onSave({
      ...draft,
      url: draft.url.trim(),
      worker: draft.worker.trim(),
      address: draft.address.trim(),
      tag: draft.tag.trim(),
    });
  };

  return (
    <dialog
      ref={dialog}
      aria-labelledby="mining-settings-title"
      onClose={onClose}
      onCancel={onClose}
      onMouseDown={closeOnBackdrop(onClose)}
      className="border-accent/40 bg-surface text-textPrimary m-auto w-[min(32rem,calc(100vw-2rem))] rounded-2xl border p-0 shadow-2xl backdrop:bg-black/60"
    >
      <form onSubmit={onSubmit} className="space-y-4 p-6">
        <h2 id="mining-settings-title" className="text-accent text-lg font-semibold">
          Settings
        </h2>
        {protocols && (
          <Field label="Protocol">
            <select
              className={INPUT}
              value={draft.protocol ?? 'v1'}
              onChange={(event) => {
                change('protocol', event.target.value === 'v2' ? 'v2' : 'v1');
              }}
            >
              <option value="v1">Stratum v1</option>
              <option value="v2">Stratum v2</option>
            </select>
          </Field>
        )}
        {require.url && (
          <Field label="Stratum service">
            <input
              className={INPUT}
              value={draft.url}
              spellCheck={false}
              placeholder="ws://host:port"
              onChange={(event) => {
                change('url', event.target.value);
              }}
            />
          </Field>
        )}
        {require.address && (
          <Field label="Payout address">
            <input
              className={INPUT}
              value={draft.address}
              spellCheck={false}
              onChange={(event) => {
                change('address', event.target.value);
              }}
            />
            {wrongAddress && (
              <span className="block text-xs text-rose-300">Not a {network ?? 'node'} address.</span>
            )}
          </Field>
        )}
        {require.tag && (
          <Field label="Coinbase tag">
            <input
              className={INPUT}
              value={draft.tag}
              spellCheck={false}
              onChange={(event) => {
                change('tag', event.target.value);
              }}
            />
          </Field>
        )}
        <div className={require.address ? '' : 'grid gap-4 sm:grid-cols-[minmax(0,1fr)_7rem]'}>
          {!require.address && (
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
          )}
          <Field label="Threads">
            <input
              className={INPUT}
              type="number"
              min={1}
              max={maximum}
              value={draft.threads}
              onChange={(event) => {
                change('threads', Math.min(maximum, Math.max(1, Math.floor(Number(event.target.value) || 1))));
              }}
            />
          </Field>
        </div>
        <Field label="Password">
          <input
            className={INPUT}
            type="password"
            value={draft.password}
            autoComplete="off"
            placeholder="optional"
            onChange={(event) => {
              change('password', event.target.value);
            }}
          />
        </Field>
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
            disabled={missing}
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
