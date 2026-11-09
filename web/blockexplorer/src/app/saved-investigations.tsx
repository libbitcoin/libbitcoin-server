import {
  createContext,
  type ReactNode,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
} from 'react';
import {
  bookmarkIdentity,
  emptyInvestigations,
  INBOX_ID,
  INVESTIGATIONS_STORAGE_KEY,
  mergeInvestigations,
  newInvestigationId,
  parseInvestigations,
  type BookmarkSource,
  type BookmarkTarget,
  type InvestigationsData,
  type SavedBookmark,
} from '@/lib/utils/investigations';

function readData() {
  const raw = globalThis.localStorage.getItem(INVESTIGATIONS_STORAGE_KEY);
  return raw ? parseInvestigations(raw) : emptyInvestigations();
}

function loadData() {
  try {
    return { data: readData(), error: null };
  } catch {
    return {
      data: emptyInvestigations(),
      error: 'Saved investigations could not be read. Existing storage has been left untouched.',
    };
  }
}

function useInvestigationsState() {
  const [state, setState] = useState(loadData);
  const [error, setError] = useState<string | null>(state.error);
  const bookmarksByTarget = useMemo(() => {
    const lookup = new Map<string, SavedBookmark[]>();
    for (const bookmark of state.data.bookmarks) {
      const identity = bookmarkIdentity(bookmark.target, bookmark.source);
      const entries = lookup.get(identity) ?? [];
      entries.push(bookmark);
      lookup.set(identity, entries);
    }
    return lookup;
  }, [state.data.bookmarks]);
  const mutate = useCallback((change: (data: InvestigationsData) => InvestigationsData) => {
    try {
      // Read the latest snapshot so sequential edits from other tabs are preserved.
      const next = parseInvestigations(JSON.stringify(change(readData())));
      globalThis.localStorage.setItem(INVESTIGATIONS_STORAGE_KEY, JSON.stringify(next));
      setState({ data: next, error: null });
      setError(null);
      return true;
    } catch (error_) {
      setError(
        error_ instanceof Error
          ? `Could not save: ${error_.message}`
          : 'Could not save investigations. Browser storage may be unavailable or full.'
      );
      return false;
    }
  }, []);

  useEffect(() => {
    const handleStorage = (event: StorageEvent) => {
      if (event.key !== INVESTIGATIONS_STORAGE_KEY && event.key !== null) return;
      const loaded = loadData();
      setState(loaded);
      setError(loaded.error);
    };
    globalThis.addEventListener('storage', handleStorage);
    return () => {
      globalThis.removeEventListener('storage', handleStorage);
    };
  }, []);

  const saveBookmark = (input: {
    target: BookmarkTarget;
    source: BookmarkSource;
    investigationId: string;
    newInvestigationName?: string;
    label: string;
    notes: string;
  }) =>
    mutate((data) => {
      const now = Date.now();
      const investigationId = input.newInvestigationName
        ? newInvestigationId()
        : input.investigationId;
      const investigations = input.newInvestigationName
        ? [
            ...data.investigations,
            {
              id: investigationId,
              name: input.newInvestigationName,
              notes: '',
              createdAt: now,
              updatedAt: now,
            },
          ]
        : data.investigations;
      const identity = bookmarkIdentity(input.target, input.source);
      const existing = data.bookmarks.find(
        (item) =>
          item.investigationId === investigationId &&
          bookmarkIdentity(item.target, item.source) === identity
      );
      const bookmark = {
        id: existing?.id ?? newInvestigationId(),
        investigationId,
        target: input.target,
        source: input.source,
        label: input.label,
        notes: input.notes,
        createdAt: existing?.createdAt ?? now,
        updatedAt: now,
      };
      return {
        ...data,
        investigations,
        bookmarks: existing
          ? data.bookmarks.map((item) => (item.id === existing.id ? bookmark : item))
          : [bookmark, ...data.bookmarks],
      };
    });

  return {
    data: state.data,
    bookmarksByTarget,
    error,
    saveBookmark,
    createInvestigation: (name: string) => {
      const id = newInvestigationId();
      return mutate((data) => ({
        ...data,
        investigations: [
          ...data.investigations,
          { id, name, notes: '', createdAt: Date.now(), updatedAt: Date.now() },
        ],
      }))
        ? id
        : null;
    },
    updateInvestigation: (id: string, name: string, notes: string) =>
      mutate((data) => ({
        ...data,
        investigations: data.investigations.map((item) =>
          item.id === id ? { ...item, name, notes, updatedAt: Date.now() } : item
        ),
      })),
    deleteInvestigation: (id: string) =>
      mutate((data) =>
        id === INBOX_ID
          ? data
          : {
              ...data,
              investigations: data.investigations.filter((item) => item.id !== id),
              bookmarks: data.bookmarks.filter((item) => item.investigationId !== id),
            }
      ),
    updateBookmark: (id: string, label: string, notes: string, investigationId: string) =>
      mutate((data) => {
        const existing = data.bookmarks.find((item) => item.id === id);
        if (!existing) throw new Error('This bookmark no longer exists.');
        if (
          data.bookmarks.some(
            (item) =>
              item.id !== id &&
              item.investigationId === investigationId &&
              bookmarkIdentity(item.target, item.source) ===
                bookmarkIdentity(existing.target, existing.source)
          )
        ) {
          throw new Error(
            'This item is already saved in that investigation. Its existing notes were preserved.'
          );
        }
        return {
          ...data,
          bookmarks: data.bookmarks.map((item) =>
            item.id === id
              ? { ...item, label, notes, investigationId, updatedAt: Date.now() }
              : item
          ),
        };
      }),
    removeBookmark: (id: string) =>
      mutate((data) => ({ ...data, bookmarks: data.bookmarks.filter((item) => item.id !== id) })),
    importData: (raw: string) =>
      mutate((data) => mergeInvestigations(data, parseInvestigations(raw))),
    exportData: () => {
      try {
        return JSON.stringify(readData(), null, 2);
      } catch {
        setError(
          'Could not read investigations for export. Existing storage has been left untouched.'
        );
        return null;
      }
    },
  };
}

const InvestigationsContext = createContext<ReturnType<typeof useInvestigationsState> | null>(null);
export function SavedInvestigationsProvider({ children }: { children: ReactNode }) {
  const value = useInvestigationsState();
  return <InvestigationsContext.Provider value={value}>{children}</InvestigationsContext.Provider>;
}
export function useSavedInvestigations() {
  const value = useContext(InvestigationsContext);
  if (!value) throw new Error('SavedInvestigationsProvider missing in component tree');
  return value;
}
