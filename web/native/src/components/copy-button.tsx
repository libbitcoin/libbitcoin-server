import { CheckIcon, Square2StackIcon } from '@heroicons/react/24/outline';
import { useEffect, useState } from 'react';
import { copyTextToClipboard } from '@/lib/utils/clipboard';

const COPIED_MS = 1500;

export function CopyButton({ value, label }: { value: string; label: string }) {
  const [copied, setCopied] = useState(false);

  useEffect(() => {
    if (!copied) return;
    const timer = setTimeout(() => {
      setCopied(false);
    }, COPIED_MS);
    return () => {
      clearTimeout(timer);
    };
  }, [copied]);

  const Icon = copied ? CheckIcon : Square2StackIcon;
  return (
    <button
      type="button"
      aria-label={copied ? 'Copied' : label}
      title={copied ? 'Copied' : label}
      className="text-textMuted hover:text-accent shrink-0"
      onClick={() => {
        void copyTextToClipboard(value).then(setCopied);
      }}
    >
      <Icon className={copied ? 'h-4 w-4' : 'h-4 w-4 -scale-x-100'} aria-hidden="true" />
    </button>
  );
}
