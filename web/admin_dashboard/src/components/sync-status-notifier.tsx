import { useEffect, useRef, useState } from 'react';
import { showBrowserNotification } from '@/lib/browser-notifications';
import { formatInteger } from '@/lib/format';
import { type ChainSync, SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS } from '@/lib/sync-status';

type SyncState = 'behind' | 'near-current';

interface SyncNotice {
  state: SyncState;
  blocksBehind: number;
}

interface SyncStatusNotifierProps {
  sync: ChainSync | null;
  browserNotificationsEnabled: boolean;
}

const NOTICE_DURATION_MS = 8_000;

export function SyncStatusNotifier({ sync, browserNotificationsEnabled }: SyncStatusNotifierProps) {
  const previousStateReference = useRef<SyncState | null>(null);
  const [notice, setNotice] = useState<SyncNotice | null>(null);
  const atGenesis = sync !== null && sync.candidate.height === 0;
  const blocksRemaining = atGenesis ? null : (sync?.blocksRemaining ?? null);

  useEffect(() => {
    const next = currentSyncState(blocksRemaining);
    if (!next) return;

    const previous = previousStateReference.current;
    previousStateReference.current = next.state;
    if (previous === null || previous === next.state) return;

    // This state represents a threshold-crossing event, not a value derived
    // continuously from the current props.
    setNotice(next);
    if (browserNotificationsEnabled) {
      const content = notificationContent(next);
      showBrowserNotification(content.title, {
        body: content.body,
        tag: 'libbitcoin-sync-status',
      });
    }
  }, [browserNotificationsEnabled, blocksRemaining]);

  useEffect(() => {
    if (!notice) return;
    const timer = setTimeout(() => {
      setNotice((current) => (current === notice ? null : current));
    }, NOTICE_DURATION_MS);
    return () => {
      clearTimeout(timer);
    };
  }, [notice]);

  if (!notice) return null;

  const nearCurrent = notice.state === 'near-current';
  return (
    <aside
      role="status"
      aria-live="polite"
      aria-atomic="true"
      className={`fixed right-4 bottom-4 left-4 z-[60] rounded-xl border p-4 shadow-2xl shadow-black/50 sm:left-auto sm:w-96 ${
        nearCurrent
          ? 'bg-noticeSuccess/95 border-emerald-400/35'
          : 'bg-noticeWarning/95 border-amber-400/35'
      }`}
    >
      <div className="flex items-start gap-4">
        <div className="min-w-0 flex-1">
          <p
            className={
              nearCurrent ? 'font-semibold text-emerald-200' : 'font-semibold text-amber-200'
            }
          >
            {nearCurrent ? 'Node near current' : 'Node fell behind'}
          </p>
          <p className="text-noticeText/75 mt-1 text-sm">{noticeDescription(notice)}</p>
        </div>
        <button
          type="button"
          aria-label="Dismiss synchronization notification"
          onClick={() => {
            setNotice(null);
          }}
          className="text-noticeText/60 hover:text-noticeText text-lg leading-none"
        >
          ×
        </button>
      </div>
    </aside>
  );
}

function notificationContent(notice: SyncNotice): { title: string; body: string } {
  if (notice.state === 'near-current') {
    return {
      title: 'Libbitcoin node near current',
      body: noticeDescription(notice),
    };
  }

  return {
    title: 'Libbitcoin node fell behind',
    body: noticeDescription(notice),
  };
}

function currentSyncState(blocksBehind: number | null): SyncNotice | null {
  if (blocksBehind === null) return null;

  return {
    state: blocksBehind <= SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS ? 'near-current' : 'behind',
    blocksBehind,
  };
}

function noticeDescription(notice: SyncNotice): string {
  if (notice.state === 'near-current') {
    return `Confirmed top is within ${String(SYNC_PROGRESS_HIDE_THRESHOLD_BLOCKS)} blocks of the top candidate header.`;
  }
  return `Confirmed top is ${formatInteger(notice.blocksBehind)} blocks below the top candidate header.`;
}
