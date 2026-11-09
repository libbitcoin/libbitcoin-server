import { useEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { Link } from 'react-router-dom';
import { useSavedInvestigations } from '@/app/saved-investigations';
import { useProviderInfo } from '@/lib/api/client';
import {
  bookmarkIdentity,
  bookmarkSource,
  bookmarkTargetSchema,
  defaultBookmarkLabel,
  INBOX_ID,
  type BookmarkTarget,
} from '@/lib/utils/investigations';
import { investigationsPath } from '@/lib/utils/routes';

export const bookmarkFieldClass =
  'border-accent/40 bg-background text-textPrimary w-full rounded-md border px-3 py-2 text-sm';
export const bookmarkButtonClass =
  'border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-xs disabled:opacity-50';

export function BookmarkDialog({
  title,
  children,
  onClose,
}: {
  title: string;
  children: React.ReactNode;
  onClose: () => void;
}) {
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    const dialog = ref.current;
    dialog?.showModal();
    return () => {
      dialog?.close();
    };
  }, []);
  return createPortal(
    <dialog
      ref={ref}
      aria-label={title}
      onKeyDown={(event) => {
        event.stopPropagation();
      }}
      onCancel={(event) => {
        event.preventDefault();
        onClose();
      }}
      className="border-accent/40 bg-surface text-textPrimary fixed inset-0 m-auto max-h-[85vh] w-[calc(100%_-_2rem)] max-w-lg space-y-4 overflow-y-auto rounded-xl border p-5 shadow-2xl backdrop:bg-black/70"
    >
      <div className="flex items-center justify-between gap-3">
        <h2 className="text-accent text-lg font-semibold">{title}</h2>
        <button
          type="button"
          onClick={onClose}
          aria-label="Close dialog"
          className={bookmarkButtonClass}
        >
          Close
        </button>
      </div>
      {children}
    </dialog>,
    document.body
  );
}

export function SaveBookmarkButton({
  target,
  compact = false,
}: {
  target: BookmarkTarget;
  compact?: boolean;
}) {
  const metadata = useProviderInfo();
  const source = bookmarkSource(metadata);
  const { bookmarksByTarget } = useSavedInvestigations();
  const [open, setOpen] = useState(false);
  const parsed = bookmarkTargetSchema.safeParse(target);
  if (!parsed.success) return null;
  const normalized = parsed.data;
  const existing = bookmarksByTarget.has(bookmarkIdentity(normalized, source));
  return (
    <>
      <button
        type="button"
        className={bookmarkButtonClass}
        onClick={() => {
          setOpen(true);
        }}
        aria-label={`${existing ? 'Edit saved' : 'Save'} ${target.kind}${'index' in target ? ` ${String(target.index)}` : ''}`}
      >
        {existing ? '★ Saved' : compact ? '☆ Save' : '☆ Save to investigation'}
      </button>
      {open && (
        <BookmarkDialog
          title="Save to investigation"
          onClose={() => {
            setOpen(false);
          }}
        >
          <SaveBookmarkForm
            target={normalized}
            onClose={() => {
              setOpen(false);
            }}
          />
        </BookmarkDialog>
      )}
    </>
  );
}

function SaveBookmarkForm({ target, onClose }: { target: BookmarkTarget; onClose: () => void }) {
  const source = bookmarkSource(useProviderInfo());
  const { bookmarksByTarget, saveBookmark, error } = useSavedInvestigations();
  const matches = bookmarksByTarget.get(bookmarkIdentity(target, source)) ?? [];
  const [investigationId, setInvestigationId] = useState(matches[0]?.investigationId ?? INBOX_ID);
  const [label, setLabel] = useState(matches[0]?.label ?? defaultBookmarkLabel(target));
  const [notes, setNotes] = useState(matches[0]?.notes ?? '');
  return (
    <BookmarkSaveFields
      label={label}
      notes={notes}
      setLabel={setLabel}
      setNotes={setNotes}
      investigationId={investigationId}
      onInvestigationChange={(id) => {
        setInvestigationId(id);
        const existing = matches.find((item) => item.investigationId === id);
        if (existing) {
          setLabel(existing.label);
          setNotes(existing.notes);
        }
      }}
      onSave={(label, notes, newName) => {
        if (
          saveBookmark({
            target,
            source,
            investigationId,
            label,
            notes,
            newInvestigationName: newName,
          })
        )
          onClose();
      }}
      error={error}
    />
  );
}

function BookmarkSaveFields({
  label,
  notes,
  setLabel,
  setNotes,
  investigationId,
  onInvestigationChange,
  onSave,
  error,
}: {
  label: string;
  notes: string;
  setLabel: (value: string) => void;
  setNotes: (value: string) => void;
  investigationId: string;
  onInvestigationChange: (id: string) => void;
  onSave: (label: string, notes: string, newName?: string) => void;
  error: string | null;
}) {
  const { data } = useSavedInvestigations();
  const [name, setName] = useState('');
  return (
    <form
      className="space-y-4"
      onSubmit={(event) => {
        event.preventDefault();
        if (label.trim() && (investigationId !== 'new' || name.trim()))
          onSave(label.trim(), notes, investigationId === 'new' ? name.trim() : undefined);
      }}
    >
      <label className="block space-y-1 text-sm">
        <span>Investigation</span>
        <select
          className={bookmarkFieldClass}
          value={investigationId}
          onChange={(event) => {
            onInvestigationChange(event.target.value);
          }}
        >
          {data.investigations.map((item) => (
            <option key={item.id} value={item.id}>
              {item.name}
            </option>
          ))}
          <option value="new">New investigation…</option>
        </select>
      </label>
      {investigationId === 'new' && (
        <label className="block space-y-1 text-sm">
          <span>New investigation name</span>
          <input
            className={bookmarkFieldClass}
            required
            maxLength={80}
            value={name}
            onChange={(event) => {
              setName(event.target.value);
            }}
          />
        </label>
      )}
      <label className="block space-y-1 text-sm">
        <span>Label</span>
        <input
          className={bookmarkFieldClass}
          required
          maxLength={120}
          value={label}
          onChange={(event) => {
            setLabel(event.target.value);
          }}
        />
      </label>
      <label className="block space-y-1 text-sm">
        <span>Notes</span>
        <textarea
          className={bookmarkFieldClass}
          rows={3}
          maxLength={5000}
          value={notes}
          onChange={(event) => {
            setNotes(event.target.value);
          }}
        />
      </label>
      {error && (
        <p role="alert" className="text-accentSecondary text-sm">
          {error}
        </p>
      )}
      <div className="flex flex-wrap items-center justify-between gap-3">
        <button type="submit" className={bookmarkButtonClass}>
          Save bookmark
        </button>
        <Link to={investigationsPath()} className="text-accent text-sm underline">
          View saved investigations
        </Link>
      </div>
    </form>
  );
}
