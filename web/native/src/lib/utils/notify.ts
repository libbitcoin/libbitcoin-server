// Shows a browser notification, asking permission the first time. Browsers
// provide notifications only to secure contexts (https or localhost).
export function notifyBrowser(title: string, body: string, onClick: () => void): void {
  if (!('Notification' in globalThis) || !globalThis.isSecureContext) return;

  const show = () => {
    const notification = new Notification(title, { body });
    notification.addEventListener('click', () => {
      globalThis.focus();
      onClick();
      notification.close();
    });
  };

  if (Notification.permission === 'granted') {
    show();
    return;
  }

  if (Notification.permission === 'default') {
    void Notification.requestPermission().then((permission) => {
      if (permission === 'granted') show();
    });
  }
}
