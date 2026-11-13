const STORAGE_KEY = 'libbitcoinAdminDashboardBrowserNotifications';

export type BrowserNotificationPermission = NotificationPermission | 'unsupported';

function browserStorage(): Storage | null {
  try {
    const window_ = (globalThis as unknown as { window?: Window }).window;
    return window_?.localStorage ?? null;
  } catch {
    return null;
  }
}

function notificationApi(): typeof Notification | null {
  const notification = (globalThis as unknown as { Notification?: typeof Notification })
    .Notification;
  return typeof notification === 'function' ? notification : null;
}

export function browserNotificationPermission(): BrowserNotificationPermission {
  return notificationApi()?.permission ?? 'unsupported';
}

export async function requestBrowserNotificationPermission(): Promise<BrowserNotificationPermission> {
  const notification = notificationApi();
  if (!notification) return 'unsupported';
  if (notification.permission !== 'default') return notification.permission;
  return notification.requestPermission();
}

export function loadBrowserNotificationsEnabled(
  storage: Storage | null = browserStorage()
): boolean {
  try {
    return (
      browserNotificationPermission() === 'granted' && storage?.getItem(STORAGE_KEY) === 'true'
    );
  } catch {
    return false;
  }
}

export function saveBrowserNotificationsEnabled(
  enabled: boolean,
  storage: Storage | null = browserStorage()
): void {
  try {
    storage?.setItem(STORAGE_KEY, String(enabled));
  } catch {
    // Storage may be disabled or full; the in-memory preference remains usable.
  }
}

export function showBrowserNotification(title: string, options: NotificationOptions): boolean {
  const notification = notificationApi();
  if (!notification || notification.permission !== 'granted') return false;

  try {
    new notification(title, options);
    return true;
  } catch {
    // Notifications can still be rejected by browser or operating-system policy.
    return false;
  }
}
