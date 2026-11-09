import { useMemo, useState } from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { useSavedInvestigations } from '@/app/saved-investigations';
import {
  BookmarkDialog,
  bookmarkButtonClass,
  bookmarkFieldClass,
} from '@/components/save-bookmark';
import { createBlockchainClient, useProviderSwitcher } from '@/lib/api/client';
import { getNetworkParams, scriptHashFromAddress } from '@/lib/api/providers/libbitcoin-mapper';
import { usePageTitle } from '@/lib/hooks/usePageTitle';
import {
  bookmarkIdentifier,
  bookmarkPath,
  bookmarkSource,
  INBOX_ID,
  sourceKey,
  type Investigation,
  type SavedBookmark,
} from '@/lib/utils/investigations';

export function InvestigationsPage() {
  usePageTitle('Saved investigations');
  const store = useSavedInvestigations();
  const { data, error } = store;
  const { availableProviders, setProvider } = useProviderSwitcher();
  const navigate = useNavigate();
  const [params, setParams] = useSearchParams();
  const selected = data.investigations.find((item) => item.id === params.get('investigation'));
  const [query, setQuery] = useState('');
  const [kind, setKind] = useState('all');
  const [newName, setNewName] = useState('');
  const [message, setMessage] = useState('');
  const [fileError, setFileError] = useState('');
  const [editInvestigation, setEditInvestigation] = useState<Investigation | null>(null);
  const [editBookmark, setEditBookmark] = useState<SavedBookmark | null>(null);
  const [removing, setRemoving] = useState<{
    kind: 'bookmark' | 'investigation';
    id: string;
    label: string;
  } | null>(null);
  const sources = useMemo(
    () =>
      new Set(
        availableProviders.map((id) =>
          sourceKey(bookmarkSource(createBlockchainClient(id).metadata))
        )
      ),
    [availableProviders]
  );
  const matching = data.bookmarks
    .filter((item) => {
      if (selected && item.investigationId !== selected.id) return false;
      if (kind !== 'all' && item.target.kind !== kind) return false;
      const group = data.investigations.find((entry) => entry.id === item.investigationId);
      return [
        item.label,
        item.notes,
        bookmarkIdentifier(item.target),
        item.source.providerId,
        item.source.endpoint,
        group?.name ?? '',
      ].some((value) => value.toLowerCase().includes(query.trim().toLowerCase()));
    })
    // eslint-disable-next-line unicorn/no-array-sort -- filter creates a new array; the app targets ES2022.
    .sort((a, b) => b.updatedAt - a.updatedAt);
  const [limit, setLimit] = useState(30);
  const selectInvestigation = (id?: string) => {
    setParams(id ? { investigation: id } : {});
    setLimit(30);
  };
  const openBookmark = (item: SavedBookmark) => {
    if (!sources.has(sourceKey(item.source))) return;
    let state: { displayAddress: string } | undefined;
    if (item.target.kind === 'address' && item.target.displayAddress && item.source.network) {
      const decoded = scriptHashFromAddress(
        item.target.displayAddress,
        getNetworkParams(item.source.network)
      );
      if (decoded?.hash === item.target.address)
        state = { displayAddress: item.target.displayAddress };
    }
    setProvider(item.source.providerId);
    void navigate(bookmarkPath(item.target), { state });
  };
  const exportData = () => {
    const exported = store.exportData();
    if (exported === null) return;
    const url = URL.createObjectURL(new Blob([exported], { type: 'application/json' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = 'saved-investigations.json';
    link.click();
    globalThis.setTimeout(() => {
      URL.revokeObjectURL(url);
    }, 1000);
  };
  const visibleCount = selected
    ? data.bookmarks.filter((item) => item.investigationId === selected.id).length
    : data.bookmarks.length;
  return (
    <div className="space-y-6">
      <header className="flex flex-wrap items-end justify-between gap-4">
        <div>
          <p className="text-accentSecondary text-sm tracking-wide uppercase">Research workspace</p>
          <h1 tabIndex={-1} className="text-accent text-3xl font-semibold">
            Saved investigations
          </h1>
          <p className="text-textMuted mt-2 text-sm">
            Bookmarks and notes are saved in this browser. Export a backup to keep them elsewhere.
          </p>
        </div>
        <div className="flex gap-2">
          <button type="button" className={bookmarkButtonClass} onClick={exportData}>
            Export JSON
          </button>
          <label className={`${bookmarkButtonClass} cursor-pointer`}>
            Import JSON
            <input
              type="file"
              accept=".json,application/json"
              aria-label="Import investigations"
              className="sr-only"
              onChange={(event) => {
                const file = event.target.files?.[0];
                event.target.value = '';
                if (!file) return;
                setMessage('');
                setFileError('');
                if (file.size > 10_000_000) {
                  setFileError('Choose a JSON file smaller than 10 MB.');
                  return;
                }
                void file
                  .text()
                  .then((raw) => {
                    if (store.importData(raw))
                      setMessage(
                        'Imported as new investigations. Existing bookmarks were preserved.'
                      );
                  })
                  .catch(() => {
                    setFileError('Could not read the selected file.');
                  });
              }}
            />
          </label>
        </div>
      </header>
      {(error || fileError) && (
        <p
          role="alert"
          className="border-accentSecondary/40 text-accentSecondary rounded-lg border p-3 text-sm"
        >
          {fileError || error}
        </p>
      )}
      {message && (
        <p role="status" className="text-accent text-sm">
          {message}
        </p>
      )}
      <div className="grid items-start gap-6 lg:grid-cols-[16rem_minmax(0,1fr)]">
        <aside
          className="border-accent/30 bg-surface/80 space-y-4 rounded-xl border p-4"
          aria-label="Investigations"
        >
          <h2 className="text-accent text-sm font-semibold">Investigations</h2>
          <div className="max-h-80 space-y-1 overflow-y-auto">
            <button
              type="button"
              className={`${bookmarkButtonClass} w-full text-left ${selected ? '' : 'bg-accent/10'}`}
              aria-pressed={!selected}
              onClick={() => {
                selectInvestigation();
              }}
            >
              All bookmarks · {data.bookmarks.length}
            </button>
            {data.investigations.map((item) => (
              <button
                type="button"
                key={item.id}
                className={`${bookmarkButtonClass} w-full text-left break-words ${selected?.id === item.id ? 'bg-accent/10' : ''}`}
                aria-pressed={selected?.id === item.id}
                onClick={() => {
                  selectInvestigation(item.id);
                }}
              >
                {item.name} ·{' '}
                {data.bookmarks.filter((entry) => entry.investigationId === item.id).length}
              </button>
            ))}
          </div>
          <form
            className="space-y-2"
            onSubmit={(event) => {
              event.preventDefault();
              if (!newName.trim()) return;
              const id = store.createInvestigation(newName.trim());
              if (id) {
                setNewName('');
                selectInvestigation(id);
              }
            }}
          >
            <label className="block space-y-1 text-sm">
              <span>New investigation</span>
              <input
                className={bookmarkFieldClass}
                value={newName}
                maxLength={80}
                required
                placeholder="e.g. Payment follow-up"
                onChange={(event) => {
                  setNewName(event.target.value);
                }}
              />
            </label>
            <button type="submit" className={bookmarkButtonClass}>
              Create investigation
            </button>
          </form>
        </aside>
        <section className="min-w-0 space-y-4" aria-label="Saved bookmarks">
          <div className="flex flex-wrap items-start justify-between gap-3">
            <div className="min-w-0">
              <h2 className="text-accent text-xl font-semibold break-words">
                {selected?.name ?? 'All bookmarks'}
              </h2>
              <p className="text-textMuted text-sm">
                {visibleCount} saved {visibleCount === 1 ? 'item' : 'items'}
              </p>
            </div>
            {selected && (
              <div className="flex gap-2">
                <button
                  type="button"
                  className={bookmarkButtonClass}
                  onClick={() => {
                    setEditInvestigation(selected);
                  }}
                >
                  Edit investigation
                </button>
                {selected.id !== INBOX_ID && (
                  <button
                    type="button"
                    className={bookmarkButtonClass}
                    onClick={() => {
                      setRemoving({ kind: 'investigation', id: selected.id, label: selected.name });
                    }}
                  >
                    Delete investigation
                  </button>
                )}
              </div>
            )}
          </div>
          {selected?.notes && (
            <p className="text-textPrimary bg-surface/80 rounded-lg p-4 text-sm break-words whitespace-pre-wrap">
              {selected.notes}
            </p>
          )}
          <div className="flex flex-wrap gap-3">
            <label className="min-w-0 flex-1">
              <span className="sr-only">Search bookmarks</span>
              <input
                type="search"
                className={bookmarkFieldClass}
                placeholder="Search labels, notes, identifiers…"
                value={query}
                onChange={(event) => {
                  setQuery(event.target.value);
                  setLimit(30);
                }}
              />
            </label>
            <label>
              <span className="sr-only">Bookmark type</span>
              <select
                className={bookmarkFieldClass}
                value={kind}
                onChange={(event) => {
                  setKind(event.target.value);
                  setLimit(30);
                }}
              >
                <option value="all">All types</option>
                {['block', 'transaction', 'address', 'input', 'output'].map((type) => (
                  <option key={type} value={type}>
                    {type[0].toUpperCase() + type.slice(1)}
                  </option>
                ))}
              </select>
            </label>
          </div>
          {matching.length === 0 && (
            <div className="border-accent/20 bg-surface/60 rounded-xl border border-dashed p-8 text-center">
              <h3 className="text-textPrimary font-medium">
                {visibleCount === 0 ? 'Start an investigation' : 'No matching bookmarks'}
              </h3>
              <p className="text-textMuted mt-2 text-sm">
                {visibleCount === 0
                  ? 'Visit a block, transaction, or address and choose “Save to investigation”. You can also save individual transaction inputs and outputs.'
                  : 'Try another search or bookmark type.'}
              </p>
            </div>
          )}
          <ul className="space-y-3">
            {matching.slice(0, limit).map((item) => (
              <li
                key={item.id}
                className="border-accent/30 bg-surface/80 min-w-0 space-y-3 rounded-xl border p-4"
              >
                <div className="flex flex-wrap items-start justify-between gap-3">
                  <div className="min-w-0 flex-1">
                    <p className="text-accentSecondary text-xs uppercase">
                      {item.target.kind} ·{' '}
                      {data.investigations.find((entry) => entry.id === item.investigationId)?.name}
                    </p>
                    <h3 className="text-textPrimary mt-1 font-semibold break-words">
                      {item.label}
                    </h3>
                  </div>
                  <div className="flex gap-2">
                    <button
                      type="button"
                      className={bookmarkButtonClass}
                      disabled={!sources.has(sourceKey(item.source))}
                      onClick={() => {
                        openBookmark(item);
                      }}
                    >
                      Open
                    </button>
                    <button
                      type="button"
                      className={bookmarkButtonClass}
                      onClick={() => {
                        setEditBookmark(item);
                      }}
                    >
                      Edit
                    </button>
                  </div>
                </div>
                <p className="text-textMuted font-mono text-xs break-all">
                  {bookmarkIdentifier(item.target)}
                </p>
                {item.notes && (
                  <p className="text-textPrimary text-sm break-words whitespace-pre-wrap">
                    {item.notes}
                  </p>
                )}
                <p className="text-textMuted text-xs break-all">
                  {item.source.providerId} {item.source.network ? `· ${item.source.network}` : ''} ·{' '}
                  {item.source.endpoint || 'Default endpoint'}
                </p>
                {!sources.has(sourceKey(item.source)) && (
                  <p className="text-accentSecondary text-xs">
                    The saved provider, endpoint, or network is not configured. Restore that source
                    to open this bookmark.
                  </p>
                )}
              </li>
            ))}
          </ul>
          {limit < matching.length && (
            <button
              type="button"
              className={bookmarkButtonClass}
              onClick={() => {
                setLimit((value) => value + 30);
              }}
            >
              Show more bookmarks
            </button>
          )}
        </section>
      </div>
      {editInvestigation && (
        <BookmarkDialog
          title="Edit investigation"
          onClose={() => {
            setEditInvestigation(null);
          }}
        >
          <InvestigationEditor
            investigation={editInvestigation}
            onDone={() => {
              setEditInvestigation(null);
            }}
          />
        </BookmarkDialog>
      )}
      {editBookmark && (
        <BookmarkDialog
          title="Edit bookmark"
          onClose={() => {
            setEditBookmark(null);
          }}
        >
          <BookmarkEditor
            bookmark={editBookmark}
            onDone={() => {
              setEditBookmark(null);
            }}
            onRemove={() => {
              setRemoving({ kind: 'bookmark', id: editBookmark.id, label: editBookmark.label });
              setEditBookmark(null);
            }}
          />
        </BookmarkDialog>
      )}
      {removing && (
        <BookmarkDialog
          title={removing.kind === 'bookmark' ? 'Remove bookmark?' : 'Delete investigation?'}
          onClose={() => {
            setRemoving(null);
          }}
        >
          <p className="text-sm break-words">
            {removing.kind === 'bookmark'
              ? `Remove “${removing.label}” from your bookmarks?`
              : `Delete “${removing.label}” and its ${String(data.bookmarks.filter((item) => item.investigationId === removing.id).length)} bookmarks?`}
          </p>
          <div className="flex gap-3">
            <button
              type="button"
              className={bookmarkButtonClass}
              onClick={() => {
                setRemoving(null);
              }}
            >
              Cancel
            </button>
            <button
              type="button"
              className={bookmarkButtonClass}
              onClick={() => {
                if (
                  removing.kind === 'bookmark'
                    ? store.removeBookmark(removing.id)
                    : store.deleteInvestigation(removing.id)
                ) {
                  if (removing.kind === 'investigation') selectInvestigation();
                  setRemoving(null);
                }
              }}
            >
              Confirm removal
            </button>
          </div>
          {error && (
            <p role="alert" className="text-accentSecondary text-sm">
              {error}
            </p>
          )}
        </BookmarkDialog>
      )}
    </div>
  );
}

function InvestigationEditor({
  investigation,
  onDone,
}: {
  investigation: Investigation;
  onDone: () => void;
}) {
  const { updateInvestigation, error } = useSavedInvestigations();
  const [name, setName] = useState(investigation.name);
  const [notes, setNotes] = useState(investigation.notes);
  return (
    <form
      className="space-y-4"
      onSubmit={(event) => {
        event.preventDefault();
        if (name.trim() && updateInvestigation(investigation.id, name.trim(), notes)) onDone();
      }}
    >
      <label className="block space-y-1">
        <span>Name</span>
        <input
          required
          maxLength={80}
          className={bookmarkFieldClass}
          value={name}
          onChange={(event) => {
            setName(event.target.value);
          }}
        />
      </label>
      <label className="block space-y-1">
        <span>Notes</span>
        <textarea
          rows={5}
          maxLength={5000}
          className={bookmarkFieldClass}
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
      <button type="submit" className={bookmarkButtonClass}>
        Save investigation
      </button>
    </form>
  );
}

function BookmarkEditor({
  bookmark,
  onDone,
  onRemove,
}: {
  bookmark: SavedBookmark;
  onDone: () => void;
  onRemove: () => void;
}) {
  const { data, updateBookmark, error } = useSavedInvestigations();
  const [label, setLabel] = useState(bookmark.label);
  const [notes, setNotes] = useState(bookmark.notes);
  const [investigationId, setInvestigationId] = useState(bookmark.investigationId);
  return (
    <form
      className="space-y-4"
      onSubmit={(event) => {
        event.preventDefault();
        if (label.trim() && updateBookmark(bookmark.id, label.trim(), notes, investigationId))
          onDone();
      }}
    >
      <label className="block space-y-1">
        <span>Investigation</span>
        <select
          className={bookmarkFieldClass}
          value={investigationId}
          onChange={(event) => {
            setInvestigationId(event.target.value);
          }}
        >
          {data.investigations.map((item) => (
            <option key={item.id} value={item.id}>
              {item.name}
            </option>
          ))}
        </select>
      </label>
      <label className="block space-y-1">
        <span>Label</span>
        <input
          required
          maxLength={120}
          className={bookmarkFieldClass}
          value={label}
          onChange={(event) => {
            setLabel(event.target.value);
          }}
        />
      </label>
      <label className="block space-y-1">
        <span>Notes</span>
        <textarea
          rows={4}
          maxLength={5000}
          className={bookmarkFieldClass}
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
      <div className="flex flex-wrap justify-between gap-3">
        <button type="submit" className={bookmarkButtonClass}>
          Save changes
        </button>
        <button type="button" className={bookmarkButtonClass} onClick={onRemove}>
          Remove bookmark
        </button>
      </div>
    </form>
  );
}
