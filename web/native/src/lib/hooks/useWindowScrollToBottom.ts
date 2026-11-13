import { useEffect, useRef } from 'react';

export interface UseWindowScrollToBottomOptions {
  enabled: boolean;
  onBottomReached: () => void;
  bottomThresholdPx?: number;
}

function isWindowScrolledToBottom(document_: Document, bottomThresholdPx: number) {
  const window_ = (globalThis as unknown as { window?: Window }).window;
  if (!window_) return false;

  const docEl = document_.documentElement;
  const body = document_.body;

  const scrollTop =
    typeof window_.scrollY === 'number'
      ? window_.scrollY
      : Math.max(docEl.scrollTop, body.scrollTop);

  const viewportBottom = scrollTop + window_.innerHeight;
  const scrollHeight = Math.max(docEl.scrollHeight, body.scrollHeight);

  return Math.ceil(viewportBottom) >= scrollHeight - bottomThresholdPx;
}

export function useWindowScrollToBottom({
  enabled,
  onBottomReached,
  bottomThresholdPx = 0,
}: UseWindowScrollToBottomOptions) {
  const enabledRef = useRef(enabled);
  const onBottomReachedRef = useRef(onBottomReached);
  const bottomThresholdPxRef = useRef(bottomThresholdPx);
  const hasTriggeredRef = useRef(false);

  useEffect(() => {
    enabledRef.current = enabled;
  }, [enabled]);

  useEffect(() => {
    onBottomReachedRef.current = onBottomReached;
  }, [onBottomReached]);

  useEffect(() => {
    bottomThresholdPxRef.current = bottomThresholdPx;
  }, [bottomThresholdPx]);

  useEffect(() => {
    const window_ = (globalThis as unknown as { window?: Window }).window;
    const document_ = (globalThis as unknown as { document?: Document }).document;
    if (!window_ || !document_) return;

    const onScroll = () => {
      const atBottom = isWindowScrolledToBottom(document_, bottomThresholdPxRef.current);
      if (!atBottom) {
        hasTriggeredRef.current = false;
        return;
      }
      if (!enabledRef.current || hasTriggeredRef.current) return;
      hasTriggeredRef.current = true;
      onBottomReachedRef.current();
    };

    window_.addEventListener('scroll', onScroll, { passive: true });
    return () => {
      window_.removeEventListener('scroll', onScroll);
    };
  }, []);
}
