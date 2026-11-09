import { useEffect, useState } from 'react';

// Freshness must advance even when the telemetry worker stops publishing.
export function useCurrentTime(): number {
  const [now, setNow] = useState(Date.now);

  useEffect(() => {
    const update = () => {
      setNow(Date.now());
    };
    const timer = setInterval(update, 1_000);
    // Catch up immediately when a throttled or suspended page becomes active.
    window.addEventListener('focus', update);
    document.addEventListener('visibilitychange', update);
    return () => {
      clearInterval(timer);
      window.removeEventListener('focus', update);
      document.removeEventListener('visibilitychange', update);
    };
  }, []);

  return now;
}
