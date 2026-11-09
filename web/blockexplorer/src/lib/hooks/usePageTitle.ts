import { useEffect } from 'react';

const BASE_TITLE = 'Libbitcoin Server';

/**
 * Sets the document title to `<title> · Libbitcoin Server`, or the base title
 * alone when no title is given. Every routed page should call this so titles
 * never go stale across client-side navigations.
 */
export function usePageTitle(title?: string | null) {
  useEffect(() => {
    const document_ = (globalThis as unknown as { document?: Document }).document;
    if (!document_) return;
    document_.title = title ? `${title} · ${BASE_TITLE}` : BASE_TITLE;
  }, [title]);
}
