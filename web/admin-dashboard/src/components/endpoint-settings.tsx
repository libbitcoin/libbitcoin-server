import { useState, type SyntheticEvent } from 'react';
import {
  browserNotificationPermission,
  requestBrowserNotificationPermission,
  type BrowserNotificationPermission,
} from '@/lib/browser-notifications';
import { type EndpointConfiguration, normalizeEndpointUrl } from '@/lib/endpoints';

interface EndpointSettingsProps {
  configuration: EndpointConfiguration;
  browserNotificationsEnabled: boolean;
  onClose: () => void;
  onBrowserNotificationsEnabledChange: (enabled: boolean) => void;
  onReset: () => void;
  onSave: (configuration: EndpointConfiguration) => void;
}

export function EndpointSettings({
  configuration,
  browserNotificationsEnabled,
  onClose,
  onBrowserNotificationsEnabledChange,
  onReset,
  onSave,
}: EndpointSettingsProps) {
  const [adminUrl, setAdminUrl] = useState(configuration.adminUrl);
  const [nativeUrl, setNativeUrl] = useState(configuration.nativeUrl);
  const [error, setError] = useState<string | null>(null);
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

  function submit(event: SyntheticEvent<HTMLFormElement>) {
    event.preventDefault();
    try {
      onSave({
        adminUrl: normalizeEndpointUrl(adminUrl),
        nativeUrl: normalizeEndpointUrl(nativeUrl),
      });
    } catch (error_) {
      setError(error_ instanceof Error ? error_.message : 'Invalid endpoint configuration');
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
        aria-labelledby="endpoint-settings-title"
        className="border-accent/30 bg-surface/95 max-h-[calc(100vh-2rem)] w-full max-w-xl overflow-y-auto rounded-2xl border shadow-2xl shadow-black/50"
      >
        <header className="border-accent/20 flex items-start justify-between border-b p-5">
          <div>
            <p className="text-accentSecondary text-xs tracking-wide uppercase">
              Runtime configuration
            </p>
            <h2 id="endpoint-settings-title" className="text-accent mt-1 text-xl font-semibold">
              Node endpoints
            </h2>
          </div>
          <button
            type="button"
            onClick={onClose}
            className="text-textMuted hover:text-accent rounded p-1 text-xl"
            aria-label="Close endpoint settings"
          >
            ×
          </button>
        </header>

        <form onSubmit={submit} className="space-y-5 p-5">
          <label className="block space-y-2">
            <span className="text-textPrimary text-sm font-medium">Admin endpoint</span>
            <input
              value={adminUrl}
              onChange={(event) => {
                setAdminUrl(event.target.value);
              }}
              className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-3 py-2 font-mono text-sm focus:ring-0"
              placeholder="http://node-host:admin-port"
              autoComplete="url"
            />
          </label>

          <label className="block space-y-2">
            <span className="text-textPrimary text-sm font-medium">Native endpoint</span>
            <input
              value={nativeUrl}
              onChange={(event) => {
                setNativeUrl(event.target.value);
              }}
              className="border-accent/40 bg-background text-textPrimary focus:border-accent w-full rounded-md border px-3 py-2 font-mono text-sm focus:ring-0"
              placeholder="http://node-host:native-port"
              autoComplete="url"
            />
          </label>

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
                notificationPermission === 'denied'
              }
              onClick={() => void toggleBrowserNotifications()}
              className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm disabled:cursor-not-allowed disabled:opacity-50"
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

          {error && (
            <p className="rounded-md border border-red-500/30 bg-red-500/10 px-3 py-2 text-sm text-red-300">
              {error}
            </p>
          )}

          <div className="flex flex-wrap justify-between gap-3 pt-2">
            <button
              type="button"
              onClick={onReset}
              className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-4 py-2 text-sm"
            >
              Reset defaults
            </button>
            <div className="flex gap-2">
              <button
                type="button"
                onClick={onClose}
                className="text-textMuted hover:text-accent rounded-md px-4 py-2 text-sm"
              >
                Cancel
              </button>
              <button
                type="submit"
                className="border-accent/40 text-accent hover:border-accent hover:text-accentSecondary rounded-md border px-3 py-2 text-sm font-medium"
              >
                Save endpoints
              </button>
            </div>
          </div>
        </form>
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
  if (permission === 'unsupported') return 'This browser does not support system notifications.';
  if (permission === 'denied') {
    return 'Notifications are blocked. Allow them in your browser settings to enable this feature.';
  }
  if (enabled) {
    return 'System notifications will appear when the node enters or leaves the near-current range.';
  }
  return 'Get a system notification when the node enters or leaves the near-current range.';
}
