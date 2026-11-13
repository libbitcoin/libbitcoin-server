import { formatHash } from '@/lib/utils/format';

interface HashTextProps {
  value: string;
  truncate?: number;
  className?: string;
}

export function HashText({ value, truncate = 8, className }: HashTextProps) {
  return (
    <span className={className}>
      <span className="inline md:hidden">{formatHash(value, truncate)}</span>
      <span className="hidden break-all md:inline">{value}</span>
    </span>
  );
}
