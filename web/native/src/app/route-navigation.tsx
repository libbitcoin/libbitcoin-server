import { useEffect, useRef } from 'react';
import {
  NavigationType,
  Outlet,
  ScrollRestoration,
  useLocation,
  useNavigationType,
} from 'react-router-dom';

const SCROLL_STORAGE_KEY = 'blockchainExplorerScrollPositions';

export function RouteContent() {
  const mainRef = useRef<HTMLElement>(null);
  const location = useLocation();
  const navigationType = useNavigationType();

  useEffect(() => {
    // ScrollRestoration owns POP navigation so Back/Forward returns users to
    // their previous reading position without moving focus back to the top.
    if (navigationType === NavigationType.Pop) return;

    const main = mainRef.current;
    if (!main) return;

    // Anchored transaction rows manage focus when their asynchronous data arrives.
    const target = location.hash
      ? // eslint-disable-next-line unicorn/prefer-query-selector -- Fragment IDs are not CSS selectors.
        document.getElementById(location.hash.slice(1))
      : (main.querySelector<HTMLElement>('h1') ?? main);
    target?.focus({ preventScroll: true });
  }, [location.hash, location.key, navigationType]);

  return (
    <>
      <main
        ref={mainRef}
        tabIndex={-1}
        className="mx-auto min-h-[calc(100vh-4rem)] max-w-7xl px-4 py-8"
      >
        <Outlet />
      </main>
      <ScrollRestoration storageKey={SCROLL_STORAGE_KEY} />
    </>
  );
}
