import { useState } from 'react';
import { ThemeSelect } from './theme-select';
import {
  browserNotificationPermission,
  requestBrowserNotificationPermission,
  type BrowserNotificationPermission,
} from '@/lib/browser-notifications';
import { HEADER_ACTION } from '@/lib/styles';

interface SettingsProps {
  browserNotificationsEnabled: boolean;
  onClose: () => void;
  onBrowserNotificationsEnabledChange: (enabled: boolean) => void;
}

export function Settings({
  browserNotificationsEnabled,
  onClose,
  onBrowserNotificationsEnabledChange,
}: SettingsProps) {
  const [notificationPermission, setNotificationPermission] =
    useState<BrowserNotificationPermission>(browserNotificationPermission);
  const [requestingNotifications, setRequestingNotifications] = useState(false);
  const [notificationError, setNotificationError] = useState<string | null>(null);

  const notificationsEnabled = browserNotificationsEnabled && notificationPermission === 'granted';

  async function toggleBrowserNotifications() {
    if (notificationsEnabled) {
      onBrowserNotificationsEnabledChange(false);
      return;
    }

    setRequestingNotifications(true);
    setNotificationError(null);
    try {
      const permission = await requestBrowserNotificationPermission();
      setNotificationPermission(permission);
      if (permission === 'granted') {
        onBrowserNotificationsEnabledChange(true);
      } else if (permission === 'denied') {
        setNotificationError('Permission was denied. It can be changed in your browser settings.');
      }
    } catch {
      setNotificationError('The browser could not enable notifications.');
    } finally {
      setRequestingNotifications(false);
    }
  }

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4 backdrop-blur-sm"
      role="presentation"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) onClose();
      }}
    >
      <section
        role="dialog"
        aria-modal="true"
        aria-labelledby="settings-title"
        className="border-accent/30 bg-surface/95 max-h-[calc(100vh-2rem)] w-full max-w-xl overflow-y-auto rounded-2xl border shadow-2xl shadow-black/50"
      >
        <header className="border-accent/20 flex items-start justify-between border-b p-5">
          <h2 id="settings-title" className="text-accent text-xl font-semibold">
            Settings
          </h2>
          <button
            type="button"
            onClick={onClose}
            className="text-textMuted hover:text-accent rounded p-1 text-xl"
            aria-label="Close settings"
          >
            ×
          </button>
        </header>

        <div className="space-y-5 p-5">
          <ThemeSelect />
          <div className="border-accentSecondary/20 bg-accent/5 space-y-3 rounded-xl border p-4">
            <div>
              <h3 className="text-textPrimary text-sm font-medium">Browser notifications</h3>
              <p id="browser-notifications-help" className="text-textMuted mt-1 text-xs">
                {notificationDescription(
                  notificationPermission,
                  notificationsEnabled,
                  requestingNotifications
                )}
              </p>
            </div>
            <button
              type="button"
              aria-describedby="browser-notifications-help"
              disabled={
                requestingNotifications ||
                notificationPermission === 'unsupported' ||
                notificationPermission === 'denied' ||
                !globalThis.isSecureContext
              }
              onClick={() => void toggleBrowserNotifications()}
              className={`${HEADER_ACTION} disabled:cursor-not-allowed`}
            >
              {notificationButtonLabel(
                notificationPermission,
                notificationsEnabled,
                requestingNotifications
              )}
            </button>
            {notificationError && (
              <p className="text-sm text-red-300" role="alert">
                {notificationError}
              </p>
            )}
          </div>

          <div className="flex justify-end pt-2">
            <button
              type="button"
              onClick={onClose}
              className="border-accent bg-accent/20 text-accent hover:bg-accent/30 rounded-md border px-4 py-1.5 text-sm font-medium disabled:cursor-not-allowed disabled:opacity-50"
            >
              Done
            </button>
          </div>
        </div>
      </section>
    </div>
  );
}

function notificationButtonLabel(
  permission: BrowserNotificationPermission,
  enabled: boolean,
  requesting: boolean
): string {
  if (requesting) return 'Requesting permission…';
  if (!globalThis.isSecureContext) return 'Requires https';
  if (permission === 'unsupported') return 'Notifications unavailable';
  if (permission === 'denied') return 'Blocked by browser';
  return enabled ? 'Disable notifications' : 'Enable notifications';
}

function notificationDescription(
  permission: BrowserNotificationPermission,
  enabled: boolean,
  requesting: boolean
): string {
  if (requesting) return 'Waiting for your browser permission choice.';
  if (!globalThis.isSecureContext) {
    return 'Notifications require https or localhost.';
  }
  if (permission === 'unsupported') return 'This browser does not support system notifications.';
  if (permission === 'denied') {
    return 'Notifications are blocked. Allow them in your browser settings to enable this feature.';
  }
  if (enabled) {
    return 'System notifications will appear when the node enters or leaves the near-current range.';
  }
  return 'Get a system notification when the node enters or leaves the near-current range.';
}
